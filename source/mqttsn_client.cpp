/*
 *  Copyright (c) 2018, Vit Holasek
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

#include <string.h>
#include "mqttsn_client.hpp"
#include "mqttsn_serializer.hpp"
#include "fsl_debug_console.h"

/**
 * @file
 *   This file includes implementation of MQTT-SN protocol v1.2 client.
 *
 */

/**
 * Maximal supported MQTT-SN message size in bytes.
 *
 */
#define MAX_PACKET_SIZE 255
/**
 * Minimal MQTT-SN message size in bytes.
 *
 */
#define MQTTSN_MIN_PACKET_LENGTH 2

namespace ot {

namespace Mqttsn {
// TODO: Implement retransmission and DUP behavior
// TODO: Implement OT logging

template <typename CallbackType>
MessageMetadata<CallbackType>::MessageMetadata()
{
    ;
}

template <typename CallbackType>
MessageMetadata<CallbackType>::MessageMetadata(const Ip6::Address &aDestinationAddress, uint16_t aDestinationPort, uint16_t aMessageId, uint32_t aTimestamp, uint32_t aRetransmissionTimeout, CallbackType aCallback, void* aContext)
    : mDestinationAddress(aDestinationAddress)
    , mDestinationPort(aDestinationPort)
    , mMessageId(aMessageId)
    , mTimestamp(aTimestamp)
    , mRetransmissionTimeout(aRetransmissionTimeout)
    , mRetransmissionCount(0)
    , mCallback(aCallback)
    , mContext(aContext)
{
    ;
}

template <typename CallbackType>
otError MessageMetadata<CallbackType>::AppendTo(Message &aMessage) const
{
    return aMessage.Append(this, sizeof(*this));
}

template <typename CallbackType>
uint16_t MessageMetadata<CallbackType>::ReadFrom(Message &aMessage)
{
    return aMessage.Read(aMessage.GetLength() - sizeof(*this), sizeof(*this), this);
}

template <typename CallbackType>
uint16_t MessageMetadata<CallbackType>::GetLength() const
{
    return sizeof(*this);
}

template <typename CallbackType>
WaitingMessagesQueue<CallbackType>::WaitingMessagesQueue(TimeoutCallbackFunc aTimeoutCallback, void* aTimeoutContext)
    : mQueue()
    , mTimeoutCallback(aTimeoutCallback)
    , mTimeoutContext(aTimeoutContext)
{
    ;
}

template <typename CallbackType>
WaitingMessagesQueue<CallbackType>::~WaitingMessagesQueue(void)
{
    ForceTimeout();
}

template <typename CallbackType>
otError WaitingMessagesQueue<CallbackType>::EnqueueCopy(const Message &aMessage, uint16_t aLength, const MessageMetadata<CallbackType> &aMetadata)
{
    otError error = OT_ERROR_NONE;
    Message *messageCopy = nullptr;

    VerifyOrExit((messageCopy = aMessage.Clone(aLength)) != NULL, error = OT_ERROR_NO_BUFS);
    SuccessOrExit(error = aMetadata.AppendTo(*messageCopy));
    SuccessOrExit(error = mQueue.Enqueue(*messageCopy));

exit:
    return error;
}

template <typename CallbackType>
otError WaitingMessagesQueue<CallbackType>::Dequeue(Message &aMessage)
{
    otError error = mQueue.Dequeue(aMessage);
    aMessage.Free();
    return error;
}

template <typename CallbackType>
Message* WaitingMessagesQueue<CallbackType>::Find(uint16_t aMessageId, MessageMetadata<CallbackType> &aMetadata)
{
    Message* message = mQueue.GetHead();
    while (message)
    {
        aMetadata.ReadFrom(*message);
        if (aMessageId == aMetadata.mMessageId)
        {
            return message;
        }
        message = message->GetNext();
    }
    return nullptr;
}

template <typename CallbackType>
otError WaitingMessagesQueue<CallbackType>::HandleTimer()
{
    otError error = OT_ERROR_NONE;
    Message* message = mQueue.GetHead();
    while (message)
    {
        Message* current = message;
        message = message->GetNext();
        MessageMetadata<CallbackType> metadata;
        metadata.ReadFrom(*current);
        // check if message timed out
        if (metadata.mTimestamp + metadata.mRetransmissionTimeout <= TimerMilli::GetNow())
        {
            if (mTimeoutCallback)
            {
                // Invoke timeout callback
                mTimeoutCallback(metadata, mTimeoutContext);
            }
            SuccessOrExit(error = Dequeue(*current));
        }
    }
exit:
    return error;
}

template <typename CallbackType>
void WaitingMessagesQueue<CallbackType>::ForceTimeout()
{
    Message* message = mQueue.GetHead();
    while (message)
    {
        Message* current = message;
        message = message->GetNext();
        MessageMetadata<CallbackType> metadata;
        metadata.ReadFrom(*current);
        if (mTimeoutCallback)
        {
            mTimeoutCallback(metadata, mTimeoutContext);
        }
        Dequeue(*current);
    }
}



MqttsnClient::MqttsnClient(Instance& instance)
    : InstanceLocator(instance)
    , mSocket(GetInstance().GetThreadNetif().GetIp6().GetUdp())
    , mConfig()
    , mMessageId(1)
    , mPingReqTime(0)
    , mGwTimeout(0)
    , mDisconnectRequested(false)
    , mSleepRequested(false)
    , mTimeoutRaised(false)
    , mClientState(kStateDisconnected)
    , mSubscribeQueue(HandleSubscribeTimeout, this)
    , mRegisterQueue(HandleRegisterTimeout, this)
    , mUnsubscribeQueue(HandleUnsubscribeTimeout, this)
    , mPublishQos1Queue(HandlePublishQos1Timeout, this)
    , mPublishQos2PublishQueue(HandlePublishQos2PublishTimeout, this)
    , mPublishQos2PubrelQueue(HandlePublishQos2PubrelTimeout, this)
    , mPublishQos2PubrecQueue(HandlePublishQos2PubrecTimeout, this)
    , mConnectedCallback(nullptr)
    , mConnectContext(nullptr)
    , mPublishReceivedCallback(nullptr)
    , mPublishReceivedContext(nullptr)
    , mAdvertiseCallback(nullptr)
    , mAdvertiseContext(nullptr)
    , mSearchGwCallback(nullptr)
    , mSearchGwContext(nullptr)
    , mDisconnectedCallback(nullptr)
    , mDisconnectedContext(nullptr)
    , mRegisterReceivedCallback(nullptr)
    , mRegisterReceivedContext(nullptr)
{
    ;
}

MqttsnClient::~MqttsnClient()
{
    mSocket.Close();
    OnDisconnected();
}

void MqttsnClient::HandleUdpReceive(void *aContext, otMessage *aMessage, const otMessageInfo *aMessageInfo)
{
    MqttsnClient* client = static_cast<MqttsnClient*>(aContext);
    Message &message = *static_cast<Message *>(aMessage);
    const Ip6::MessageInfo &messageInfo = *static_cast<const Ip6::MessageInfo *>(aMessageInfo);

    // Read message content
    uint16_t offset = message.GetOffset();
    uint16_t length = message.GetLength() - message.GetOffset();

    unsigned char data[MAX_PACKET_SIZE];

    if (length > MAX_PACKET_SIZE)
    {
        return;
    }

    if (!data)
    {
        return;
    }
    message.Read(offset, length, data);

    // Dump packet
    PRINTF("UDP message received:\r\n");
    for (int32_t i = 0; i < length; i++)
    {
        if (i > 0)
        {
            PRINTF(" ");
        }
        PRINTF("%02X", data[i]);
    }
    PRINTF("\r\n");

    // Determine message type
    MessageType messageType;
    if (MessageBase::DeserializeMessageType(data, length, &messageType))
    {
        return;
    }
    PRINTF("Message type: %d\r\n", messageType);

    // TODO: Refactor switch to use separate handle functions
    // Handle received message type
    switch (messageType)
    {
    // CONACK message
    case kTypeConnack:
    {
        // Check source IPv6 address
        if (!client->VerifyGatewayAddress(messageInfo))
        {
            break;
        }
        ConnackMessage connackMessage;
        if (connackMessage.Deserialize(data, length) != OT_ERROR_NONE)
        {
            break;
        }
        client->mClientState = kStateActive;
        client->mGwTimeout = 0;
        if (client->mConnectedCallback)
        {
            client->mConnectedCallback(connackMessage.GetReturnCode(), client->mConnectContext);
        }
    }
        break;
    // SUBACK message
    case kTypeSuback:
    {
        // Client must be in active state
        if (client->mClientState != kStateActive)
        {
            break;
        }
        // Check source IPv6 address
        if (!client->VerifyGatewayAddress(messageInfo))
        {
            break;
        }
        SubackMessage subackMessage;
        if (subackMessage.Deserialize(data, length) != OT_ERROR_NONE)
        {
            break;
        }

        // Find waiting message with corresponding ID
        MessageMetadata<SubscribeCallbackFunc> metadata;
        Message* subscribeMessage = client->mSubscribeQueue.Find(subackMessage.GetMessageId(), metadata);
        if (!subscribeMessage)
        {
            break;
        }
        // Invoke callback and dequeue message
        if (metadata.mCallback)
        {
            metadata.mCallback(subackMessage.GetReturnCode(), subackMessage.GetTopicId(),
                subackMessage.GetQos(), metadata.mContext);
        }
        client->mSubscribeQueue.Dequeue(*subscribeMessage);
    }
        break;
    // PUBLISH message
    case kTypePublish:
    {
        // Client must be in active or awake state to receive published messages
        if (client->mClientState != kStateActive && client->mClientState != kStateAwake)
        {
            break;
        }
        // Check source IPv6 address
        if (!client->VerifyGatewayAddress(messageInfo))
        {
            break;
        }
        PublishMessage publishMessage;
        if (publishMessage.Deserialize(data, length) != OT_ERROR_NONE)
        {
            break;
        }

        // Filter duplicate QoS level 2 messages
        if (publishMessage.GetQos() == kQos2)
        {
            MessageMetadata<void*> metadata;
            Message* pubrecMessage = client->mPublishQos2PubrecQueue.Find(publishMessage.GetMessageId(), metadata);
            if (pubrecMessage)
            {
                break;
            }
        }

        ReturnCode code = kCodeRejectedTopicId;
        if (client->mPublishReceivedCallback)
        {
            // Invoke callback
            code = client->mPublishReceivedCallback(publishMessage.GetPayload(), publishMessage.GetPayloadLength(),
                publishMessage.GetTopicIdType(), publishMessage.GetTopicId(), publishMessage.GetShortTopicName(),
                client->mPublishReceivedContext);
        }

        // Handle QoS
        if (publishMessage.GetQos() == kQos0 || publishMessage.GetQos() == kQosm1)
        {
            // On QoS level 0 or -1 do nothing
            break;
        }
        else if (publishMessage.GetQos() == kQos1)
        {
            // On QoS level 1  send PUBACK response
            int32_t packetLength = -1;
            Message* responseMessage = nullptr;
            unsigned char buffer[MAX_PACKET_SIZE];
            PubackMessage pubackMessage(code, publishMessage.GetTopicId(), publishMessage.GetMessageId());
            if (pubackMessage.Serialize(buffer, MAX_PACKET_SIZE, &packetLength) != OT_ERROR_NONE)
            {
                break;
            }
            if (client->NewMessage(&responseMessage, buffer, packetLength) != OT_ERROR_NONE ||
                client->SendMessage(*responseMessage) != OT_ERROR_NONE)
            {
                break;
            }
        }
        else if (publishMessage.GetQos() == kQos2)
        {
            // On QoS level 2 send PUBREC message and wait for PUBREL
            int32_t packetLength = -1;
            Message* responseMessage = nullptr;
            unsigned char buffer[MAX_PACKET_SIZE];
            PubrecMessage pubrecMessage(publishMessage.GetMessageId());
            if (pubrecMessage.Serialize(buffer, MAX_PACKET_SIZE, &packetLength) != OT_ERROR_NONE)
            {
                break;
            }
            if (client->NewMessage(&responseMessage, buffer, packetLength) != OT_ERROR_NONE ||
                client->SendMessage(*responseMessage) != OT_ERROR_NONE)
            {
                break;
            }

            // Add message to waiting queue, message with same messageId will not be processed until PUBREL message received
            if (client->mPublishQos2PubrecQueue.EnqueueCopy(*responseMessage, responseMessage->GetLength(), MessageMetadata<void*>(
                client->mConfig.GetAddress(), client->mConfig.GetPort(), publishMessage.GetMessageId(), TimerMilli::GetNow(),
                client->mConfig.GetRetransmissionTimeout() * 1000, NULL, NULL)) != OT_ERROR_NONE)
            {
                break;
            }
        }
    }
        break;
    // ADVERTISE message
    case kTypeAdvertise:
    {
        AdvertiseMessage advertiseMessage;
        if (advertiseMessage.Deserialize(data, length) != OT_ERROR_NONE)
        {
            break;
        }
        if (client->mAdvertiseCallback)
        {
            client->mAdvertiseCallback(messageInfo.GetPeerAddr(), advertiseMessage.GetGatewayId(),
                advertiseMessage.GetDuration(), client->mAdvertiseContext);
        }
    }
        break;
    // GWINFO message
    case kTypeGwInfo:
    {
        GwInfoMessage gwInfoMessage;
        if (gwInfoMessage.Deserialize(data, length) != OT_ERROR_NONE)
        {
            break;
        }
        if (client->mSearchGwCallback)
        {
            Ip6::Address address = (gwInfoMessage.GetHasAddress()) ? gwInfoMessage.GetAddress()
                : messageInfo.GetPeerAddr();
            client->mSearchGwCallback(address, gwInfoMessage.GetGatewayId(), client->mSearchGwContext);
        }
    }
        break;
    // Regack message
    case kTypeRegack:
    {
        // Client state must be active
        if (client->mClientState != kStateActive)
        {
            break;
        }
        // Check source IPv6 address
        if (!client->VerifyGatewayAddress(messageInfo))
        {
            break;
        }

        RegackMessage regackMessage;
        if (regackMessage.Deserialize(data, length) != OT_ERROR_NONE)
        {
            break;
        }
        // Find waiting message with corresponding ID
        MessageMetadata<RegisterCallbackFunc> metadata;
        Message* registerMessage = client->mRegisterQueue.Find(regackMessage.GetMessageId(), metadata);
        if (!registerMessage)
        {
            break;
        }
        // Invoke callback and dequeue message
        if (metadata.mCallback)
        {
            metadata.mCallback(regackMessage.GetReturnCode(), regackMessage.GetTopicId(), metadata.mContext);
        }
        client->mRegisterQueue.Dequeue(*registerMessage);
    }
        break;
    // REGISTER message
    case kTypeRegister:
    {
        int32_t packetLength = -1;
        Message* responseMessage = nullptr;
        unsigned char buffer[MAX_PACKET_SIZE];

        // Client state must be active
        if (client->mClientState != kStateActive)
        {
            break;
        }
        if (!client->VerifyGatewayAddress(messageInfo))
        {
            break;
        }

        RegisterMessage registerMessage;
        if (registerMessage.Deserialize(data, length) != OT_ERROR_NONE)
        {
            break;
        }

        // Invoke register callback
        ReturnCode code = kCodeRejectedTopicId;
        if (client->mRegisterReceivedCallback)
        {
            code = client->mRegisterReceivedCallback(registerMessage.GetTopicId(), registerMessage.GetTopicName(), client->mRegisterReceivedContext);
        }

        // Send REGACK response message
        RegackMessage regackMessage(code, registerMessage.GetTopicId(), registerMessage.GetMessageId());
        if (regackMessage.Serialize(buffer, MAX_PACKET_SIZE, &packetLength) != OT_ERROR_NONE)
        {
            break;
        }
        if (client->NewMessage(&responseMessage, buffer, packetLength) != OT_ERROR_NONE ||
            client->SendMessage(*responseMessage) != OT_ERROR_NONE)
        {
            break;
        }
    }
    // PUBACK message
    case kTypePuback:
    {
        // Client state must be active
        if (client->mClientState != kStateActive)
        {
            break;
        }
        // Check source IPv6 address.
        if (!client->VerifyGatewayAddress(messageInfo))
        {
            break;
        }

        PubackMessage pubackMessage;
        if (pubackMessage.Deserialize(data, length) != OT_ERROR_NONE)
        {
            break;
        }

        // Process QoS level 1 message
        // Find message waiting for acknowledge
        MessageMetadata<PublishCallbackFunc> metadata;
        Message* publishMessage = client->mPublishQos1Queue.Find(pubackMessage.GetMessageId(), metadata);
        if (publishMessage)
        {
            // Invoke confirmation callback
            if (metadata.mCallback)
            {
                metadata.mCallback(pubackMessage.GetReturnCode(), metadata.mContext);
            }
            // Dequeue waiting message
            client->mPublishQos1Queue.Dequeue(*publishMessage);
            break;
        }
        // May be QoS level 2 message error response
        publishMessage = client->mPublishQos2PublishQueue.Find(pubackMessage.GetMessageId(), metadata);
        if (publishMessage)
        {
            // Invoke confirmation callback
            if (metadata.mCallback)
            {
                metadata.mCallback(pubackMessage.GetReturnCode(), metadata.mContext);
            }
            // Dequeue waiting message
            client->mPublishQos2PublishQueue.Dequeue(*publishMessage);
            break;
        }

        // May be QoS level 0 message error response - it is not handled
    }
        break;
    // PUBREC message
    case kTypePubrec:
    {
        int32_t packetLength = -1;
        unsigned char buffer[MAX_PACKET_SIZE];

        // Client state must be active
        if (client->mClientState != kStateActive)
        {
            break;
        }
        // Check source IPv6 address
        if (!client->VerifyGatewayAddress(messageInfo))
        {
            break;
        }

        PubrecMessage pubrecMessage;
        if (pubrecMessage.Deserialize(data, length) != OT_ERROR_NONE)
        {
            break;
        }
        // Process QoS level 2 message
        // Find message waiting for receive acknowledge
        MessageMetadata<PublishCallbackFunc> metadata;
        Message* publishMessage = client->mPublishQos2PublishQueue.Find(pubrecMessage.GetMessageId(), metadata);
        if (!publishMessage)
        {
            break;
        }

        // Send PUBREL message
        PubrelMessage pubrelMessage(metadata.mMessageId);
        if (pubrelMessage.Serialize(buffer, MAX_PACKET_SIZE, &packetLength) != OT_ERROR_NONE)
        {
            break;
        }
        Message* responseMessage = nullptr;
        if (client->NewMessage(&responseMessage, buffer, packetLength) != OT_ERROR_NONE ||
            client->SendMessage(*responseMessage) != OT_ERROR_NONE)
        {
            break;
        }
        // Enqueue PUBREL message and wait for PUBCOMP
        if (client->mPublishQos2PubrelQueue.EnqueueCopy(*responseMessage, responseMessage->GetLength(),
            MessageMetadata<PublishCallbackFunc>(client->mConfig.GetAddress(), client->mConfig.GetPort(), metadata.mMessageId,
                TimerMilli::GetNow(), client->mConfig.GetRetransmissionTimeout() * 1000, metadata.mCallback, client)) != OT_ERROR_NONE)
        {
            break;
        }

        // Dequeue waiting PUBLISH message
        client->mPublishQos2PublishQueue.Dequeue(*publishMessage);
    }
    // PUBREL message
    case kTypePubrel:
    {
        int32_t packetLength = -1;
        unsigned char buffer[MAX_PACKET_SIZE];

        // Client state must be active
        if (client->mClientState != kStateActive)
        {
            break;
        }
        // Check source IPv6 address
        if (!client->VerifyGatewayAddress(messageInfo))
        {
            break;
        }

        PubrelMessage pubrelMessage;
        if (pubrelMessage.Deserialize(data, length) != OT_ERROR_NONE)
        {
            break;
        }
        // Process QoS level 2 PUBREL message
        // Find PUBREC message waiting for receive acknowledge
        MessageMetadata<void*> metadata;
        Message* pubrecMessage = client->mPublishQos2PubrecQueue.Find(pubrelMessage.GetMessageId(), metadata);
        if (!pubrecMessage)
        {
            break;
        }
        // Send PUBCOMP message
        PubcompMessage pubcompMessage(metadata.mMessageId);
        if (pubcompMessage.Serialize(buffer, MAX_PACKET_SIZE, &packetLength) != OT_ERROR_NONE)
        {
            break;
        }
        Message* responseMessage = nullptr;
        if (client->NewMessage(&responseMessage, buffer, packetLength) != OT_ERROR_NONE ||
            client->SendMessage(*responseMessage) != OT_ERROR_NONE)
        {
            break;
        }

        // Dequeue waiting message
        client->mPublishQos2PubrecQueue.Dequeue(*pubrecMessage);
    }
    // PUBCOMP message
    case kTypePubcomp:
    {
        // Client state must be active
        if (client->mClientState != kStateActive)
        {
            break;
        }
        // Check source IPv6 address
        if (!client->VerifyGatewayAddress(messageInfo))
        {
            break;
        }

        PubcompMessage pubcompMessage;
        if (pubcompMessage.Deserialize(data, length) != OT_ERROR_NONE)
        {
            break;
        }
        // Process QoS level 2 PUBCOMP message
        // Find PUBREL message waiting for receive acknowledge
        MessageMetadata<PublishCallbackFunc> metadata;
        Message* pubrelMessage = client->mPublishQos2PubrelQueue.Find(pubcompMessage.GetMessageId(), metadata);
        if (!pubrelMessage)
        {
            break;
        }
        // Invoke confirmation callback
        if (metadata.mCallback)
        {
            metadata.mCallback(kCodeAccepted, metadata.mContext);
        }
        // Dequeue waiting message
        client->mPublishQos2PubrelQueue.Dequeue(*pubrelMessage);
    }
    // UNSUBACK message
    case kTypeUnsuback:
    {
        // Client state must be active
        if (client->mClientState != kStateActive)
        {
            break;
        }
        // Check source IPv6 address
        if (!client->VerifyGatewayAddress(messageInfo))
        {
            break;
        }

        UnsubackMessage unsubackMessage;
        if (unsubackMessage.Deserialize(data, length) != OT_ERROR_NONE)
        {
            break;
        }
        // Find unsubscription message waiting for confirmation
        MessageMetadata<UnsubscribeCallbackFunc> metadata;
        Message* unsubscribeMessage = client->mUnsubscribeQueue.Find(unsubackMessage.GetMessageId(), metadata);
        if (!unsubscribeMessage)
        {
            break;
        }
        // Invoke unsubscribe confirmation callback
        if (metadata.mCallback)
        {
            metadata.mCallback(kCodeAccepted, metadata.mContext);
        }
        // Dequeue waiting message
        client->mUnsubscribeQueue.Dequeue(*unsubscribeMessage);
    }
        break;
    // PINGREQ message
    case kTypePingreq:
    {
        Message* responseMessage = nullptr;
        int32_t packetLength = -1;
        unsigned char buffer[MAX_PACKET_SIZE];

        // Client state must be active
        if (client->mClientState != kStateActive)
        {
            break;
        }

        PingreqMessage pingreqMessage;
        if (pingreqMessage.Deserialize(data, length) != OT_ERROR_NONE)
        {
            break;
        }

        // Send PINGRESP message
        PingrespMessage pingrespMessage;
        if (pingrespMessage.Serialize(buffer, MAX_PACKET_SIZE, &packetLength) != OT_ERROR_NONE)
        {
            break;
        }
        if (client->NewMessage(&responseMessage, buffer, packetLength) != OT_ERROR_NONE ||
            client->SendMessage(*responseMessage, messageInfo.GetPeerAddr(), client->mConfig.GetPort()) != OT_ERROR_NONE)
        {
            break;
        }
    }
        break;
    // PINGRESP message
    case kTypePingresp:
    {
        // Check source IPv6 address
        if (!client->VerifyGatewayAddress(messageInfo))
        {
            break;
        }
        PingrespMessage pingrespMessage;
        if (pingrespMessage.Deserialize(data, length) != OT_ERROR_NONE)
        {
            break;
        }

        // Reset client timeout counter
        client->mGwTimeout = 0;
        // If the client is awake PINRESP message put it into sleep again
        if (client->mClientState == kStateAwake)
        {
            client->mClientState = kStateAsleep;
            if (client->mDisconnectedCallback)
            {
                client->mDisconnectedCallback(kAsleep, client->mDisconnectedContext);
            }
        }
    }
        break;
    // DISCONNECT message
    case kTypeDisconnect:
    {

        DisconnectMessage disconnectMessage;
        if (disconnectMessage.Deserialize(data, length))
        {
            break;
        }

        DisconnectType reason = kServer;

        // Handle disconnection behavior depending on client state
        switch (client->mClientState)
        {
        case kStateActive:
        case kStateAwake:
        case kStateAsleep:
            if (client->mDisconnectRequested)
            {
                // Regular disconnect
                client->mClientState = kStateDisconnected;
                reason = kServer;
            }
            else if (client->mSleepRequested)
            {
                // Sleep state was requested - go asleep
                client->mClientState = kStateAsleep;
                reason = kAsleep;
            }
            else
            {
                // Disconnected by gateway
                client->mClientState = kStateDisconnected;
                reason = kServer;
            }
            break;
        default:
            break;
        }
        // Check source IPv6 address
        if (!client->VerifyGatewayAddress(messageInfo))
        {
            break;
        }
        client->OnDisconnected();

        // Invoke disconnected callback
        if (client->mDisconnectedCallback)
        {
            client->mDisconnectedCallback(reason, client->mDisconnectedContext);
        }
    }
        break;
    default:
        break;
    }
}

otError MqttsnClient::Start(uint16_t aPort)
{
    otError error = OT_ERROR_NONE;
    Ip6::SockAddr sockaddr;
    sockaddr.mPort = aPort;

    // Open UDP socket
    SuccessOrExit(error = mSocket.Open(MqttsnClient::HandleUdpReceive, this));
    // Start listening on configured port
    SuccessOrExit(error = mSocket.Bind(sockaddr));

exit:
    return error;
}

otError MqttsnClient::Stop()
{
    otError error = mSocket.Close();
    // Disconnect client if it is not disconnected already
    mClientState = kStateDisconnected;
    if (mClientState != kStateDisconnected && mClientState != kStateLost)
    {
        OnDisconnected();
        if (mDisconnectedCallback)
        {
            mDisconnectedCallback(kClient, mDisconnectedContext);
        }
    }
    return error;
}

otError MqttsnClient::Process()
{
    otError error = OT_ERROR_NONE;

    uint32_t now = TimerMilli::GetNow();

    // Process keep alive and send periodical PINGREQ message
    if (mClientState == kStateActive && mPingReqTime != 0 && mPingReqTime <= now)
    {
        SuccessOrExit(error = PingGateway());
        mGwTimeout = TimerMilli::GetNow() + mConfig.GetRetransmissionTimeout() * 1000;
    }

    // Set timeout flag when communication timed out
    if (mGwTimeout != 0 && mGwTimeout <= now)
    {
        mTimeoutRaised = true;
        goto exit;
    }

    // Handle pending messages timeouts
    SuccessOrExit(error = mSubscribeQueue.HandleTimer());
    SuccessOrExit(error = mRegisterQueue.HandleTimer());
    SuccessOrExit(error = mUnsubscribeQueue.HandleTimer());
    SuccessOrExit(error = mPublishQos1Queue.HandleTimer());
    SuccessOrExit(error = mPublishQos2PublishQueue.HandleTimer());
    SuccessOrExit(error = mPublishQos2PubrelQueue.HandleTimer());

exit:
    // Handle timeout
    if (mTimeoutRaised)
    {
        mClientState = kStateLost;
        OnDisconnected();
        if (mDisconnectedCallback)
        {
            mDisconnectedCallback(kTimeout, mDisconnectedContext);
        }
    }

    return error;
}

otError MqttsnClient::Connect(MqttsnConfig &aConfig)
{
    otError error = OT_ERROR_NONE;
    int32_t length = -1;
    Message* message = nullptr;
    ConnectMessage connectMessage(mConfig.GetCleanSession(), false, mConfig.GetKeepAlive(), mConfig.GetClientId().AsCString());
    unsigned char buffer[MAX_PACKET_SIZE];

    // Cannot connect in active state (already connected)
    if (mClientState == kStateActive)
    {
        error = OT_ERROR_INVALID_STATE;
        goto exit;
    }
    mConfig = aConfig;

    // Serialize and send CONNECT message
    SuccessOrExit(error = connectMessage.Serialize(buffer, MAX_PACKET_SIZE, &length));
    SuccessOrExit(error = NewMessage(&message, buffer, length));
    SuccessOrExit(error = SendMessage(*message));

    mDisconnectRequested = false;
    mSleepRequested = false;
    // Set timeout time
    mGwTimeout = TimerMilli::GetNow() + mConfig.GetRetransmissionTimeout() * 1000;
    // Set next keepalive PINGREQ time
    mPingReqTime = TimerMilli::GetNow() + mConfig.GetKeepAlive() * 700;

exit:
    return error;
}

otError MqttsnClient::Subscribe(const char* aTopicName, bool aIsShortTopicName, Qos aQos, SubscribeCallbackFunc aCallback, void* aContext)
{
    otError error = OT_ERROR_NONE;
    int32_t length = -1;
    Ip6::MessageInfo messageInfo;
    Message *message = nullptr;
    int32_t topicNameLength = strlen(aTopicName);
    SubscribeMessage subscribeMessage;
    // Topic length must be 1 or 2
    VerifyOrExit(topicNameLength > 0, error = OT_ERROR_INVALID_ARGS);
    VerifyOrExit(!aIsShortTopicName || length <= 2, error = OT_ERROR_INVALID_ARGS);
    subscribeMessage = aIsShortTopicName ?
        SubscribeMessage(false, aQos, mMessageId, kShortTopicName, 0, aTopicName, "")
        : SubscribeMessage(false, aQos, mMessageId, kTopicName, 0, "", aTopicName);
    unsigned char buffer[MAX_PACKET_SIZE];

    // Client state must be active
    if (mClientState != kStateActive)
    {
        error = OT_ERROR_INVALID_STATE;
        goto exit;
    }

    // Topic subscription is possible only for QoS levels 1, 2, 3
    if (aQos != kQos0 || aQos != kQos1 || aQos != kQos2)
    {
        error = OT_ERROR_INVALID_ARGS;
        goto exit;
    }

    // Serialize and send SUBSCRIBE message
    SuccessOrExit(error = subscribeMessage.Serialize(buffer, MAX_PACKET_SIZE, &length));
    SuccessOrExit(error = NewMessage(&message, buffer, length));
    SuccessOrExit(error = SendMessage(*message));

    // Enqueue message to waiting queue - waiting for SUBACK
    SuccessOrExit(error = mSubscribeQueue.EnqueueCopy(*message, message->GetLength(),
        MessageMetadata<SubscribeCallbackFunc>(mConfig.GetAddress(), mConfig.GetPort(), mMessageId, TimerMilli::GetNow(),
            mConfig.GetRetransmissionTimeout() * 1000, aCallback, aContext)));
    mMessageId++;

exit:
    return error;
}

otError MqttsnClient::Subscribe(TopicId aTopicId, Qos aQos, SubscribeCallbackFunc aCallback, void* aContext)
{
    otError error = OT_ERROR_NONE;
    int32_t length = -1;
    Ip6::MessageInfo messageInfo;
    Message *message = nullptr;
    SubscribeMessage subscribeMessage(false, aQos, mMessageId, kShortTopicName, aTopicId, "", "");
    unsigned char buffer[MAX_PACKET_SIZE];

    // Client state must be active
    if (mClientState != kStateActive)
    {
        error = OT_ERROR_INVALID_STATE;
        goto exit;
    }

    // Topic subscription is possible onlz for QoS levels 1, 2, 3
    if (aQos != kQos0 || aQos != kQos1 || aQos != kQos2)
    {
        error = OT_ERROR_INVALID_ARGS;
        goto exit;
    }

    // Serialize and send SUBSCRIBE message
    SuccessOrExit(error = subscribeMessage.Serialize(buffer, MAX_PACKET_SIZE, &length));
    SuccessOrExit(error = NewMessage(&message, buffer, length));
    SuccessOrExit(error = SendMessage(*message));

    // Enqueue message to waiting queue - waiting for SUBACK
    SuccessOrExit(error = mSubscribeQueue.EnqueueCopy(*message, message->GetLength(),
        MessageMetadata<SubscribeCallbackFunc>(mConfig.GetAddress(), mConfig.GetPort(), mMessageId, TimerMilli::GetNow(),
            mConfig.GetRetransmissionTimeout() * 1000, aCallback, aContext)));
    mMessageId++;

exit:
    return error;
}

otError MqttsnClient::Register(const char* aTopicName, RegisterCallbackFunc aCallback, void* aContext)
{
    otError error = OT_ERROR_NONE;
    int32_t length = -1;
    Message* message = nullptr;
    RegisterMessage registerMessage(0, mMessageId, aTopicName);
    unsigned char buffer[MAX_PACKET_SIZE];

    // Client state must be active
    if (mClientState != kStateActive)
    {
        error = OT_ERROR_INVALID_STATE;
        goto exit;
    }

    // Serialize and send REGISTER message
    SuccessOrExit(error = registerMessage.Serialize(buffer, MAX_PACKET_SIZE, &length));
    SuccessOrExit(error = NewMessage(&message, buffer, length));
    SuccessOrExit(error = SendMessage(*message));
    // Enqueue message to waiting queue - waiting for REGACK
    SuccessOrExit(error = mRegisterQueue.EnqueueCopy(*message, message->GetLength(),
        MessageMetadata<RegisterCallbackFunc>(mConfig.GetAddress(), mConfig.GetPort(), mMessageId, TimerMilli::GetNow(),
            mConfig.GetRetransmissionTimeout() * 1000, aCallback, aContext)));
    mMessageId++;

exit:
    return error;
}

otError MqttsnClient::Publish(const uint8_t* aData, int32_t aLenght, Qos aQos, const char* aShortTopicName, PublishCallbackFunc aCallback, void* aContext)
{
    otError error = OT_ERROR_NONE;
    int32_t length = -1;
    Message* message = nullptr;
    int32_t topicNameLength = strlen(aShortTopicName);
    PublishMessage publishMessage;
    // Topic length must be 1 or 2
    VerifyOrExit(topicNameLength > 0 && topicNameLength <= 2, error = OT_ERROR_INVALID_ARGS);
    publishMessage = PublishMessage(false, false, aQos, mMessageId, kShortTopicName, 0, aShortTopicName, aData, aLenght);
    unsigned char buffer[MAX_PACKET_SIZE];

    // Client state must be active
    if (mClientState != kStateActive)
    {
        error = OT_ERROR_INVALID_STATE;
        goto exit;
    }

    // QoS levels 2 and -1 not implemented yet
    if (aQos != Qos::kQos0 && aQos != Qos::kQos1 && aQos != Qos::kQos2)
    {
        error = OT_ERROR_NOT_IMPLEMENTED;
        goto exit;
    }

    // Serialize and send PUBLISH message
    SuccessOrExit(error = publishMessage.Serialize(buffer, MAX_PACKET_SIZE, &length));
    SuccessOrExit(error = NewMessage(&message, buffer, length));
    SuccessOrExit(error = SendMessage(*message));
    if (aQos == Qos::kQos1)
    {
        // If QoS level 1 enqueue message to waiting queue - waiting for PUBACK
        SuccessOrExit(error = mPublishQos1Queue.EnqueueCopy(*message, message->GetLength(),
            MessageMetadata<PublishCallbackFunc>(mConfig.GetAddress(), mConfig.GetPort(), mMessageId, TimerMilli::GetNow(),
                mConfig.GetRetransmissionTimeout() * 1000, aCallback, aContext)));
    }
    if (aQos == Qos::kQos2)
    {
        // If QoS level 1 enqueue message to waiting queue - waiting for PUBREC
        SuccessOrExit(error = mPublishQos2PublishQueue.EnqueueCopy(*message, message->GetLength(),
            MessageMetadata<PublishCallbackFunc>(mConfig.GetAddress(), mConfig.GetPort(), mMessageId, TimerMilli::GetNow(),
                mConfig.GetRetransmissionTimeout() * 1000, aCallback, aContext)));
    }
    mMessageId++;

exit:
    return error;
}

otError MqttsnClient::Publish(const uint8_t* aData, int32_t aLength, Qos aQos, TopicId aTopicId, PublishCallbackFunc aCallback, void* aContext)
{
    otError error = OT_ERROR_NONE;
    int32_t length = -1;
    Message* message = nullptr;
    PublishMessage publishMessage(false, false, aQos, mMessageId, kTopicId, aTopicId, "", aData, aLength);
    unsigned char buffer[MAX_PACKET_SIZE];

    // Client state must be active
    if (mClientState != kStateActive)
    {
        error = OT_ERROR_INVALID_STATE;
        goto exit;
    }

    // QoS levels 2 and -1 not implemented yet
    if (aQos != Qos::kQos0 && aQos != Qos::kQos1 && aQos != Qos::kQos2)
    {
        error = OT_ERROR_NOT_IMPLEMENTED;
        goto exit;
    }

    // Serialize and send PUBLISH message
    SuccessOrExit(error = publishMessage.Serialize(buffer, MAX_PACKET_SIZE, &length));
    SuccessOrExit(error = NewMessage(&message, buffer, length));
    SuccessOrExit(error = SendMessage(*message));
    if (aQos == Qos::kQos1)
    {
        // If QoS level 1 enqueue message to waiting queue - waiting for PUBACK
        SuccessOrExit(error = mPublishQos1Queue.EnqueueCopy(*message, message->GetLength(),
            MessageMetadata<PublishCallbackFunc>(mConfig.GetAddress(), mConfig.GetPort(), mMessageId, TimerMilli::GetNow(),
                mConfig.GetRetransmissionTimeout() * 1000, aCallback, aContext)));
    }
    if (aQos == Qos::kQos2)
    {
        // If QoS level 1 enqueue message to waiting queue - waiting for PUBREC
        SuccessOrExit(error = mPublishQos2PublishQueue.EnqueueCopy(*message, message->GetLength(),
            MessageMetadata<PublishCallbackFunc>(mConfig.GetAddress(), mConfig.GetPort(), mMessageId, TimerMilli::GetNow(),
                mConfig.GetRetransmissionTimeout() * 1000, aCallback, aContext)));
    }
    mMessageId++;

exit:
    return error;
}

otError MqttsnClient::Unsubscribe(const char* aShortTopicName, UnsubscribeCallbackFunc aCallback, void* aContext)
{
    otError error = OT_ERROR_NONE;
    int32_t length = -1;
    Message* message = nullptr;
    int32_t topicNameLength = strlen(aShortTopicName);
    UnsubscribeMessage unsubscribeMessage;
    // Topic length must be 1 or 2
    VerifyOrExit(topicNameLength > 0 && topicNameLength <= 2, error = OT_ERROR_INVALID_ARGS);
    unsubscribeMessage = UnsubscribeMessage(mMessageId, kShortTopicName, 0, aShortTopicName);
    unsigned char buffer[MAX_PACKET_SIZE];

    // Client state must be active
    if (mClientState != kStateActive)
    {
        error = OT_ERROR_INVALID_STATE;
        goto exit;
    }

    // Serialize and send UNSUBSCRIBE message
    SuccessOrExit(error = unsubscribeMessage.Serialize(buffer, MAX_PACKET_SIZE, &length));
    SuccessOrExit(error = NewMessage(&message, buffer, length));
    SuccessOrExit(error = SendMessage(*message));
    // Enqueue message to waiting queue - waiting for UNSUBACK
    SuccessOrExit(error = mUnsubscribeQueue.EnqueueCopy(*message, message->GetLength(),
        MessageMetadata<UnsubscribeCallbackFunc>(mConfig.GetAddress(), mConfig.GetPort(), mMessageId, TimerMilli::GetNow(),
            mConfig.GetRetransmissionTimeout() * 1000, aCallback, aContext)));
    mMessageId++;

exit:
    return error;
}

otError MqttsnClient::Unsubscribe(TopicId aTopicId, UnsubscribeCallbackFunc aCallback, void* aContext)
{
    otError error = OT_ERROR_NONE;
    int32_t length = -1;
    Message* message = nullptr;
    UnsubscribeMessage unsubscribeMessage(mMessageId, kTopicId, aTopicId, "");
    unsigned char buffer[MAX_PACKET_SIZE];

    // Client state must be active
    if (mClientState != kStateActive)
    {
        error = OT_ERROR_INVALID_STATE;
        goto exit;
    }

    // Serialize and send UNSUBSCRIBE message
    SuccessOrExit(error = unsubscribeMessage.Serialize(buffer, MAX_PACKET_SIZE, &length));
    SuccessOrExit(error = NewMessage(&message, buffer, length));
    SuccessOrExit(error = SendMessage(*message));
    // Enqueue message to waiting queue - waiting for UNSUBACK
    SuccessOrExit(error = mUnsubscribeQueue.EnqueueCopy(*message, message->GetLength(),
        MessageMetadata<UnsubscribeCallbackFunc>(mConfig.GetAddress(), mConfig.GetPort(), mMessageId, TimerMilli::GetNow(),
            mConfig.GetRetransmissionTimeout() * 1000, aCallback, aContext)));
    mMessageId++;

exit:
    return error;
}

otError MqttsnClient::Disconnect()
{
    otError error = OT_ERROR_NONE;
    int32_t length = -1;
    Message* message = nullptr;
    DisconnectMessage disconnectMessage(0);
    unsigned char buffer[MAX_PACKET_SIZE];

    // Client must be connected
    if (mClientState != kStateActive && mClientState != kStateAwake
        && mClientState != kStateAsleep)
    {
        error = OT_ERROR_INVALID_STATE;
        goto exit;
    }

    // Serialize and send DISCONNECT message
    SuccessOrExit(error = disconnectMessage.Serialize(buffer, MAX_PACKET_SIZE, &length));
    SuccessOrExit(error = NewMessage(&message, buffer, length));
    SuccessOrExit(error = SendMessage(*message));

    // Set flag for regular disconnect request and wait for DISCONNECT message from gateway
    mDisconnectRequested = true;
    // Set timeout time
    mGwTimeout = TimerMilli::GetNow() + mConfig.GetRetransmissionTimeout() * 1000;

exit:
    return error;
}

otError MqttsnClient::Sleep(uint16_t aDuration)
{
    otError error = OT_ERROR_NONE;
    int32_t length = -1;
    Message* message = nullptr;
    DisconnectMessage disconnectMessage(aDuration);
    unsigned char buffer[MAX_PACKET_SIZE];

    // Client must be connected
    if (mClientState != kStateActive && mClientState != kStateAwake && mClientState != kStateAsleep)
    {
        error = OT_ERROR_INVALID_STATE;
        goto exit;
    }

    // Serialize and send DISCONNECT message
    SuccessOrExit(error = disconnectMessage.Serialize(buffer, MAX_PACKET_SIZE, &length));
    SuccessOrExit(error = NewMessage(&message, buffer, length));
    SuccessOrExit(error = SendMessage(*message));

    // Set flag for sleep request and wait for DISCONNECT message from gateway
    mSleepRequested = true;
    // Set timeout time
    mGwTimeout = TimerMilli::GetNow() + mConfig.GetRetransmissionTimeout() * 1000;

exit:
    return error;
}

otError MqttsnClient::Awake(uint32_t aTimeout)
{
    otError error = OT_ERROR_NONE;
    // Awake is possible only when the client is in
    if (mClientState != kStateAwake && mClientState != kStateAsleep)
    {
        error = OT_ERROR_INVALID_STATE;
        goto exit;
    }

    // Send PINGEQ message
    SuccessOrExit(error = PingGateway());

    // Set awake state and wait for any PUBLISH messages
    mClientState = kStateAwake;
    // Set timeout time - PINGRESP message must be delivered within this time
    mGwTimeout = TimerMilli::GetNow() + aTimeout;
exit:
    return error;
}

otError MqttsnClient::SearchGateway(const Ip6::Address &aMulticastAddress, uint16_t aPort, uint8_t aRadius)
{
    otError error = OT_ERROR_NONE;
    int32_t length = -1;
    Message* message = nullptr;
    SearchGwMessage searchGwMessage(aRadius);
    unsigned char buffer[MAX_PACKET_SIZE];

    // Serialize and send SEARCHGW message
    SuccessOrExit(error = searchGwMessage.Serialize(buffer, MAX_PACKET_SIZE, &length));
    SuccessOrExit(error = NewMessage(&message, buffer, length));
    SuccessOrExit(error = SendMessage(*message, aMulticastAddress, aPort, aRadius));

exit:
    return error;
}

ClientState MqttsnClient::GetState()
{
    return mClientState;
}

otError MqttsnClient::SetConnectedCallback(ConnectedCallbackFunc aCallback, void* aContext)
{
    mConnectedCallback = aCallback;
    mConnectContext = aContext;
    return OT_ERROR_NONE;
}

otError MqttsnClient::SetPublishReceivedCallback(PublishReceivedCallbackFunc aCallback, void* aContext)
{
    mPublishReceivedCallback = aCallback;
    mPublishReceivedContext = aContext;
    return OT_ERROR_NONE;
}

otError MqttsnClient::SetAdvertiseCallback(AdvertiseCallbackFunc aCallback, void* aContext)
{
    mAdvertiseCallback = aCallback;
    mAdvertiseContext = aContext;
    return OT_ERROR_NONE;
}

otError MqttsnClient::SetSearchGwCallback(SearchGwCallbackFunc aCallback, void* aContext)
{
    mSearchGwCallback = aCallback;
    mSearchGwContext = aContext;
    return OT_ERROR_NONE;
}

otError MqttsnClient::SetDisconnectedCallback(DisconnectedCallbackFunc aCallback, void* aContext)
{
    mDisconnectedCallback = aCallback;
    mDisconnectedContext = aContext;
    return OT_ERROR_NONE;
}

otError MqttsnClient::SetRegisterReceivedCallback(RegisterReceivedCallbackFunc aCallback, void* aContext)
{
    mRegisterReceivedCallback = aCallback;
    mRegisterReceivedContext = aContext;
    return OT_ERROR_NONE;
}

otError MqttsnClient::NewMessage(Message **aMessage, unsigned char* aBuffer, int32_t aLength)
{
    otError error = OT_ERROR_NONE;
    Message *message = nullptr;

    VerifyOrExit((message = mSocket.NewMessage(0)) != nullptr, error = OT_ERROR_NO_BUFS);
    SuccessOrExit(error = message->Append(aBuffer, aLength));
    *aMessage = message;

exit:
    if (error != OT_ERROR_NONE && message != nullptr)
    {
        message->Free();
    }
    return error;
}

otError MqttsnClient::SendMessage(Message &aMessage)
{
    return SendMessage(aMessage, mConfig.GetAddress(), mConfig.GetPort());
}

otError MqttsnClient::SendMessage(Message &aMessage, const Ip6::Address &aAddress, uint16_t aPort)
{
    return SendMessage(aMessage, aAddress, aPort, 0);
}

otError MqttsnClient::SendMessage(Message &aMessage, const Ip6::Address &aAddress, uint16_t aPort, uint8_t aHopLimit)
{
    otError error = OT_ERROR_NONE;
    Ip6::MessageInfo messageInfo;

    messageInfo.SetHopLimit(aHopLimit);
    messageInfo.SetPeerAddr(aAddress);
    messageInfo.SetPeerPort(aPort);
    messageInfo.SetInterfaceId(OT_NETIF_INTERFACE_ID_THREAD);

    PRINTF("Sending message to %s[:%u]\r\n", messageInfo.GetPeerAddr().ToString().AsCString(), messageInfo.GetPeerPort());
    SuccessOrExit(error = mSocket.SendTo(aMessage, messageInfo));

    if (mClientState == kStateActive)
    {
        mPingReqTime = TimerMilli::GetNow() + mConfig.GetKeepAlive() * 700;
    }

exit:
    if (error != OT_ERROR_NONE)
    {
        aMessage.Free();
    }
    return error;
}

otError MqttsnClient::PingGateway()
{
    otError error = OT_ERROR_NONE;
    int32_t length = -1;
    Message* message = nullptr;
    PingreqMessage pingreqMessage(mConfig.GetClientId().AsCString());
    unsigned char buffer[MAX_PACKET_SIZE];

    if (mClientState != kStateActive && mClientState != kStateAwake)
    {
        error = OT_ERROR_INVALID_STATE;
        goto exit;
    }

    // Serialize and send PINGREQ message
    SuccessOrExit(error = pingreqMessage.Serialize(buffer, MAX_PACKET_SIZE, &length));
    SuccessOrExit(error = NewMessage(&message, buffer, length));
    SuccessOrExit(error = SendMessage(*message));

exit:
    return error;
}

void MqttsnClient::OnDisconnected()
{
    mDisconnectRequested = false;
    mSleepRequested = false;
    mTimeoutRaised = false;
    mGwTimeout = 0;
    mPingReqTime = 0;

    mSubscribeQueue.ForceTimeout();
    mRegisterQueue.ForceTimeout();
    mUnsubscribeQueue.ForceTimeout();
    mPublishQos1Queue.ForceTimeout();
    mPublishQos2PublishQueue.ForceTimeout();
    mPublishQos2PubrelQueue.ForceTimeout();
}

bool MqttsnClient::VerifyGatewayAddress(const Ip6::MessageInfo &aMessageInfo)
{
    return aMessageInfo.GetPeerAddr() == mConfig.GetAddress()
        && aMessageInfo.GetPeerPort() == mConfig.GetPort();
}

void MqttsnClient::HandleSubscribeTimeout(const MessageMetadata<SubscribeCallbackFunc> &aMetadata, void* aContext)
{
    MqttsnClient* client = static_cast<MqttsnClient*>(aContext);
    client->mTimeoutRaised = true;
    aMetadata.mCallback(kCodeTimeout, 0, kQos0, aMetadata.mContext);
}

void MqttsnClient::HandleRegisterTimeout(const MessageMetadata<RegisterCallbackFunc> &aMetadata, void* aContext)
{
    MqttsnClient* client = static_cast<MqttsnClient*>(aContext);
    client->mTimeoutRaised = true;
    aMetadata.mCallback(kCodeTimeout, 0, aMetadata.mContext);
}

void MqttsnClient::HandleUnsubscribeTimeout(const MessageMetadata<UnsubscribeCallbackFunc> &aMetadata, void* aContext)
{
    MqttsnClient* client = static_cast<MqttsnClient*>(aContext);
    client->mTimeoutRaised = true;
    aMetadata.mCallback(kCodeTimeout, aMetadata.mContext);
}

void MqttsnClient::HandlePublishQos1Timeout(const MessageMetadata<PublishCallbackFunc> &aMetadata, void* aContext)
{
    MqttsnClient* client = static_cast<MqttsnClient*>(aContext);
    client->mTimeoutRaised = true;
    aMetadata.mCallback(kCodeTimeout, aMetadata.mContext);
}

void MqttsnClient::HandlePublishQos2PublishTimeout(const MessageMetadata<PublishCallbackFunc> &aMetadata, void* aContext)
{
    MqttsnClient* client = static_cast<MqttsnClient*>(aContext);
    client->mTimeoutRaised = true;
    aMetadata.mCallback(kCodeTimeout, aMetadata.mContext);
}

void MqttsnClient::HandlePublishQos2PubrelTimeout(const MessageMetadata<PublishCallbackFunc> &aMetadata, void* aContext)
{
    MqttsnClient* client = static_cast<MqttsnClient*>(aContext);
    client->mTimeoutRaised = true;
    aMetadata.mCallback(kCodeTimeout, aMetadata.mContext);
}

void MqttsnClient::HandlePublishQos2PubrecTimeout(const MessageMetadata<void*> &aMetadata, void* aContext)
{
    OT_UNUSED_VARIABLE(aMetadata);
    OT_UNUSED_VARIABLE(aContext);
}

}

}
