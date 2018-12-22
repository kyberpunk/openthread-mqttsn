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
#include "MQTTSNPacket.h"
#include "MQTTSNConnect.h"
#include "MQTTSNSearch.h"
#include "fsl_debug_console.h"

#define MAX_PACKET_SIZE 255
#define KEEP_ALIVE_DELAY 5
#define MQTTSN_MIN_PACKET_LENGTH 2

namespace ot {

namespace Mqttsn {
// TODO: Implement QoS and DUP behavior
// TODO: Implement OT logging

template <typename CallbackType>
MessageMetadata<CallbackType>::MessageMetadata()
{
    ;
}

template <typename CallbackType>
MessageMetadata<CallbackType>::MessageMetadata(const Ip6::Address &aDestinationAddress, uint16_t aDestinationPort, uint16_t aPacketId, uint32_t aTimestamp, uint32_t aRetransmissionTimeout, CallbackType aCallback, void* aContext)
    : mDestinationAddress(aDestinationAddress)
    , mDestinationPort(aDestinationPort)
    , mPacketId(aPacketId)
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
Message* WaitingMessagesQueue<CallbackType>::Find(uint16_t aPacketId, MessageMetadata<CallbackType> &aMetadata)
{
    Message* message = mQueue.GetHead();
    while (message)
    {
        aMetadata.ReadFrom(*message);
        if (aPacketId == aMetadata.mPacketId)
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
        if (metadata.mTimestamp + metadata.mRetransmissionTimeout <= TimerMilli::GetNow())
        {
            if (mTimeoutCallback)
            {
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
    , mPacketId(1)
    , mPingReqTime(0)
    , mGwTimeout(0)
    , mDisconnectRequested(false)
    , mSleepRequested(false)
    , mTimeoutRaised(false)
    , mClientState(kStateDisconnected)
    , mSubscribeQueue(HandleSubscribeTimeout, this)
    , mRegisterQueue(HandleRegisterTimeout, this)
    , mUnsubscribeQueue(HandleUnsubscribeTimeout, this)
    , mPublishQos1Queue(HandlePublishTimeout, this)
    , mConnectCallback(nullptr)
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

static int32_t PacketDecode(unsigned char* aData, size_t aLength)
{
    int lenlen = 0;
    int datalen = 0;

    if (aLength < MQTTSN_MIN_PACKET_LENGTH)
    {
        return MQTTSNPACKET_READ_ERROR;
    }

    lenlen = MQTTSNPacket_decode(aData, aLength, &datalen);
    if (datalen != static_cast<int>(aLength))
    {
        return MQTTSNPACKET_READ_ERROR;
    }
    return aData[lenlen]; // return the packet type
}

void MqttsnClient::HandleUdpReceive(void *aContext, otMessage *aMessage, const otMessageInfo *aMessageInfo)
{
    MqttsnClient* client = static_cast<MqttsnClient*>(aContext);
    Message &message = *static_cast<Message *>(aMessage);
    const Ip6::MessageInfo &messageInfo = *static_cast<const Ip6::MessageInfo *>(aMessageInfo);

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

    int32_t decodedPacketType = PacketDecode(data, length);
    if (decodedPacketType == MQTTSNPACKET_READ_ERROR)
    {
        return;
    }
    PRINTF("Packet type: %d\r\n", decodedPacketType);

    // TODO: Refactor switch to use separate handle functions
    switch (decodedPacketType)
    {
    case MQTTSN_CONNACK:
    {
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
        if (client->mConnectCallback)
        {
            client->mConnectCallback(connackMessage.GetReturnCode(), client->mConnectContext);
        }
    }
        break;
    case MQTTSN_SUBACK:
    {
        if (client->mClientState != kStateActive)
        {
            break;
        }
        if (!client->VerifyGatewayAddress(messageInfo))
        {
            break;
        }
        SubackMessage subackMessage;
        if (subackMessage.Deserialize(data, length) != OT_ERROR_NONE)
        {
            break;
        }

        MessageMetadata<SubscribeCallbackFunc> metadata;
        Message* message = client->mSubscribeQueue.Find(subackMessage.GetMessageId(), metadata);
        if (!message)
        {
            break;
        }
        if (metadata.mCallback)
        {
            metadata.mCallback(subackMessage.GetReturnCode(), subackMessage.GetTopicId(),
                subackMessage.GetQos(), metadata.mContext);
        }
        client->mSubscribeQueue.Dequeue(*message);
    }
        break;
    case MQTTSN_PUBLISH:
    {
        if (client->mClientState != kStateActive && client->mClientState != kStateAwake)
        {
            break;
        }
        if (!client->VerifyGatewayAddress(messageInfo))
        {
            break;
        }
        PublishMessage publishMessage;
        if (publishMessage.Deserialize(data, length) != OT_ERROR_NONE)
        {
            break;
        }
        ReturnCode code = kCodeRejectedTopicId;
        if (client->mPublishReceivedCallback)
        {
            code = client->mPublishReceivedCallback(publishMessage.GetPayload(), publishMessage.GetPayloadLength(),
                publishMessage.GetTopicId(), client->mPublishReceivedContext);
        }

        if (publishMessage.GetQos() == kQos0)
        {
            // do nothing
            break;
        }
        else if (publishMessage.GetQos() == kQos1)
        {
            int32_t packetLength = -1;
            Message* message = nullptr;
            unsigned char buffer[MAX_PACKET_SIZE];
            PubackMessage pubackMessage(code, publishMessage.GetTopicId(), publishMessage.GetMessageId());
            if (pubackMessage.Serialize(buffer, MAX_PACKET_SIZE, &packetLength) != OT_ERROR_NONE)
            {
                break;
            }
            if (client->NewMessage(&message, buffer, packetLength) != OT_ERROR_NONE ||
                client->SendMessage(*message) != OT_ERROR_NONE)
            {
                break;
            }
        }
        else
        {
            // not supported yet
            break;
        }
    }
        break;
    case MQTTSN_ADVERTISE:
    {
        unsigned char gatewayId = 0;
        unsigned short duration = 0;
        if (MQTTSNDeserialize_advertise(&gatewayId, &duration, data, length) != 1)
        {
            break;
        }
        if (client->mAdvertiseCallback)
        {
            client->mAdvertiseCallback(messageInfo.GetPeerAddr(), static_cast<uint8_t>(gatewayId),
                static_cast<uint32_t>(duration), client->mAdvertiseContext);
        }
    }
        break;
    case MQTTSN_GWINFO:
    {
        unsigned char gatewayId = 0;
        unsigned short addressLength = 1;
        unsigned char* addressText = nullptr;
        if (MQTTSNDeserialize_gwinfo(&gatewayId, &addressLength, &addressText, data, length) != 1)
        {
            break;
        }
        if (client->mSearchGwCallback)
        {
            Ip6::Address address;
            if (addressLength > 0)
            {
                char addressBuffer[40];
                memset(addressBuffer, 0 ,sizeof(addressBuffer));
                memcpy(addressBuffer, addressText, addressLength);
                address.FromString(addressBuffer);
            }
            else
            {
                address = messageInfo.GetPeerAddr();
            }
            client->mSearchGwCallback(address, gatewayId, client->mSearchGwContext);
        }
    }
        break;
    case MQTTSN_REGACK:
    {
        if (client->mClientState != kStateActive)
        {
            break;
        }
        if (!client->VerifyGatewayAddress(messageInfo))
        {
            break;
        }

        RegackMessage regackMessage;
        if (regackMessage.Deserialize(data, length) != OT_ERROR_NONE)
        {
            break;
        }
        MessageMetadata<RegisterCallbackFunc> metadata;
        Message* message = client->mRegisterQueue.Find(regackMessage.GetMessageId(), metadata);
        if (!message)
        {
            break;
        }
        if (metadata.mCallback)
        {
            metadata.mCallback(regackMessage.GetReturnCode(), regackMessage.GetTopicId(), metadata.mContext);
        }
        client->mRegisterQueue.Dequeue(*message);
    }
        break;
    case MQTTSN_REGISTER:
    {
        int32_t packetLength = -1;
        Message* message = nullptr;
        unsigned char buffer[MAX_PACKET_SIZE];

        if (client->mClientState != kStateActive)
        {
            break;
        }
        if (!client->VerifyGatewayAddress(messageInfo))
        {
            break;
        }

        unsigned short topicId;
        unsigned short packetId;
        MQTTSNString topicName;
        memset(&topicName, 0, sizeof(topicName));
        if (MQTTSNDeserialize_register(&topicId, &packetId, &topicName, data, length) != 1)
        {
            break;
        }
        ReturnCode code = kCodeRejectedTopicId;
        if (client->mRegisterReceivedCallback)
        {
            TopicNameString topicNameString("%.*s", topicName.lenstring, topicName.cstring);
            code = client->mRegisterReceivedCallback(static_cast<TopicId>(topicId), topicNameString, client->mRegisterReceivedContext);
        }

        packetLength = MQTTSNSerialize_regack(buffer, MAX_PACKET_SIZE, topicId, packetId, static_cast<unsigned char>(code));
        if (packetLength <= 0)
        {
            break;
        }
        if (client->NewMessage(&message, buffer, packetLength) != OT_ERROR_NONE ||
            client->SendMessage(*message) != OT_ERROR_NONE)
        {
            break;
        }
    }
    case MQTTSN_PUBACK:
    {
        if (client->mClientState != kStateActive)
        {
            break;
        }
        if (!client->VerifyGatewayAddress(messageInfo))
        {
            break;
        }

        PubackMessage pubackMessage;
        if (pubackMessage.Deserialize(data, length) != OT_ERROR_NONE)
        {
            break;
        }
        // Process Qos 1 message
        MessageMetadata<PublishCallbackFunc> metadata;
        Message* message = client->mPublishQos1Queue.Find(pubackMessage.GetMessageId(), metadata);
        if (!message)
        {
            break;
        }
        if (metadata.mCallback)
        {
            metadata.mCallback(pubackMessage.GetReturnCode(), pubackMessage.GetTopicId(), metadata.mContext);
        }
        client->mPublishQos1Queue.Dequeue(*message);
    }
        break;
    case MQTTSN_UNSUBACK:
    {
        if (client->mClientState != kStateActive)
        {
            break;
        }
        if (!client->VerifyGatewayAddress(messageInfo))
        {
            break;
        }

        unsigned short packetId;
        if (MQTTSNDeserialize_unsuback(&packetId, data, length) != 1)
        {
            break;
        }
        MessageMetadata<UnsubscribeCallbackFunc> metadata;
        Message* message = client->mUnsubscribeQueue.Find(packetId, metadata);
        if (!message)
        {
            break;
        }
        if (metadata.mCallback)
        {
            metadata.mCallback(kCodeAccepted, metadata.mContext);
        }
        client->mUnsubscribeQueue.Dequeue(*message);
    }
        break;
    case MQTTSN_PINGREQ:
    {
        MQTTSNString clientId;
        memset(&clientId, 0, sizeof(clientId));
        Message* message = nullptr;
        int32_t packetLength = -1;
        unsigned char buffer[MAX_PACKET_SIZE];

        if (client->mClientState != kStateActive)
        {
            break;
        }

        if (MQTTSNDeserialize_pingreq(&clientId, data, length) != 1)
        {
            break;
        }


        packetLength = MQTTSNSerialize_pingresp(buffer, MAX_PACKET_SIZE);
        if (packetLength <= 0)
        {
            break;
        }
        if (client->NewMessage(&message, buffer, packetLength) != OT_ERROR_NONE ||
            client->SendMessage(*message, messageInfo.GetPeerAddr(), client->mConfig.GetPort()) != OT_ERROR_NONE)
        {
            break;
        }
    }
        break;
    case MQTTSN_PINGRESP:
    {
        if (MQTTSNDeserialize_pingresp(data, length) != 1)
        {
            break;
        }
        if (!client->VerifyGatewayAddress(messageInfo))
        {
            break;
        }

        client->mGwTimeout = 0;
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
    case MQTTSN_DISCONNECT:
    {

        int duration;
        if (MQTTSNDeserialize_disconnect(&duration, data, length) != 1)
        {
            break;
        }
        client->OnDisconnected();

        DisconnectType reason = kServer;
        switch (client->mClientState)
        {
        case kStateActive:
        case kStateAwake:
        case kStateAsleep:
            if (client->mDisconnectRequested)
            {
                client->mClientState = kStateDisconnected;
                reason = kServer;
            }
            else if (client->mSleepRequested)
            {
                client->mClientState = kStateAsleep;
                reason = kAsleep;
            }
            else
            {
                client->mClientState = kStateDisconnected;
                reason = kServer;
            }
            break;
        default:
            break;
        }
        if (!client->VerifyGatewayAddress(messageInfo))
        {
            break;
        }

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

    SuccessOrExit(error = mSocket.Open(MqttsnClient::HandleUdpReceive, this));
    SuccessOrExit(error = mSocket.Bind(sockaddr));

exit:
    return error;
}

otError MqttsnClient::Stop()
{
    return mSocket.Close();
    if (mClientState != kStateDisconnected && mClientState != kStateLost)
    {
        mClientState = kStateDisconnected;
        OnDisconnected();
        if (mDisconnectedCallback)
        {
            mDisconnectedCallback(kClient, mDisconnectedContext);
        }
    }
}

otError MqttsnClient::Process()
{
    otError error = OT_ERROR_NONE;

    uint32_t now = TimerMilli::GetNow();

    // Process pingreq
    if (mClientState != kStateActive && mPingReqTime != 0 && mPingReqTime <= now)
    {
        SuccessOrExit(error = PingGateway());
        mGwTimeout = TimerMilli::GetNow() + mConfig.GetRetransmissionTimeout() * 1000;
    }

    // Handle timeout
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

exit:
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

    if (mClientState == kStateActive)
    {
        error = OT_ERROR_INVALID_STATE;
        goto exit;
    }
    mConfig = aConfig;

    SuccessOrExit(error = connectMessage.Serialize(buffer, MAX_PACKET_SIZE, &length));
    SuccessOrExit(error = NewMessage(&message, buffer, length));
    SuccessOrExit(error = SendMessage(*message));

    mDisconnectRequested = false;
    mSleepRequested = false;
    mGwTimeout = TimerMilli::GetNow() + mConfig.GetRetransmissionTimeout() * 1000;
    mPingReqTime = TimerMilli::GetNow() + mConfig.GetKeepAlive() * 1000;

exit:
    return error;
}

otError MqttsnClient::Subscribe(const char* aTopicName, Qos aQos, SubscribeCallbackFunc aCallback, void* aContext)
{
    otError error = OT_ERROR_NONE;
    int32_t length = -1;
    Ip6::MessageInfo messageInfo;
    Message *message = nullptr;
    SubscribeMessage subscribeMessage(false, aQos, mPacketId, aTopicName);
    unsigned char buffer[MAX_PACKET_SIZE];

    if (mClientState != kStateActive)
    {
        error = OT_ERROR_INVALID_STATE;
        goto exit;
    }

    if (aQos != kQos0 && aQos != kQos1)
    {
        error = OT_ERROR_NOT_IMPLEMENTED;
        goto exit;
    }

    SuccessOrExit(error = subscribeMessage.Serialize(buffer, MAX_PACKET_SIZE, &length));
    SuccessOrExit(error = NewMessage(&message, buffer, length));
    SuccessOrExit(error = SendMessage(*message));
    SuccessOrExit(error = mSubscribeQueue.EnqueueCopy(*message, message->GetLength(),
        MessageMetadata<SubscribeCallbackFunc>(mConfig.GetAddress(), mConfig.GetPort(), mPacketId, TimerMilli::GetNow(),
            mConfig.GetRetransmissionTimeout() * 1000, aCallback, aContext)));
    mPacketId++;

exit:
    return error;
}

otError MqttsnClient::Register(const char* aTopicName, RegisterCallbackFunc aCallback, void* aContext)
{
    otError error = OT_ERROR_NONE;
    int32_t length = -1;
    Message* message = nullptr;
    RegisterMessage registerMessage(0, mPacketId, aTopicName);
    unsigned char buffer[MAX_PACKET_SIZE];

    if (mClientState != kStateActive)
    {
        error = OT_ERROR_INVALID_STATE;
        goto exit;
    }
    SuccessOrExit(error = registerMessage.Serialize(buffer, MAX_PACKET_SIZE, &length));
    SuccessOrExit(error = NewMessage(&message, buffer, length));
    SuccessOrExit(error = SendMessage(*message));
    SuccessOrExit(error = mRegisterQueue.EnqueueCopy(*message, message->GetLength(),
        MessageMetadata<RegisterCallbackFunc>(mConfig.GetAddress(), mConfig.GetPort(), mPacketId, TimerMilli::GetNow(),
            mConfig.GetRetransmissionTimeout() * 1000, aCallback, aContext)));
    mPacketId++;

exit:
    return error;
}

otError MqttsnClient::Publish(const uint8_t* aData, int32_t aLength, Qos aQos, TopicId aTopicId, PublishCallbackFunc aCallback, void* aContext)
{
    otError error = OT_ERROR_NONE;
    int32_t length = -1;
    Message* message = nullptr;
    PublishMessage publishMessage(false, false, aQos, mPacketId, aTopicId, aData, aLength);
    unsigned char buffer[MAX_PACKET_SIZE];

    if (mClientState != kStateActive)
    {
        error = OT_ERROR_INVALID_STATE;
        goto exit;
    }

    if (aQos != Qos::kQos0 && aQos != Qos::kQos1)
    {
        error = OT_ERROR_NOT_IMPLEMENTED;
        goto exit;
    }

    SuccessOrExit(error = publishMessage.Serialize(buffer, MAX_PACKET_SIZE, &length));
    SuccessOrExit(error = NewMessage(&message, buffer, length));
    SuccessOrExit(error = SendMessage(*message));
    if (aQos == Qos::kQos1)
    {
        SuccessOrExit(error = mPublishQos1Queue.EnqueueCopy(*message, message->GetLength(),
            MessageMetadata<PublishCallbackFunc>(mConfig.GetAddress(), mConfig.GetPort(), mPacketId, TimerMilli::GetNow(),
                mConfig.GetRetransmissionTimeout() * 1000, aCallback, aContext)));
    }
    mPacketId++;

exit:
    return error;
}

otError MqttsnClient::Unsubscribe(TopicId aTopicId, UnsubscribeCallbackFunc aCallback, void* aContext)
{
    otError error = OT_ERROR_NONE;
    int32_t length = -1;
    Message* message = nullptr;
    unsigned char buffer[MAX_PACKET_SIZE];

    if (mClientState != kStateActive)
    {
        error = OT_ERROR_INVALID_STATE;
        goto exit;
    }

    MQTTSN_topicid topic;
    topic.type = MQTTSN_TOPIC_TYPE_NORMAL;
    topic.data.id = static_cast<unsigned short>(aTopicId);
    length = MQTTSNSerialize_unsubscribe(buffer, MAX_PACKET_SIZE, mPacketId, &topic);
    if (length <= 0)
    {
        error = OT_ERROR_FAILED;
        goto exit;
    }
    SuccessOrExit(error = NewMessage(&message, buffer, length));
    SuccessOrExit(error = SendMessage(*message));
    SuccessOrExit(error = mUnsubscribeQueue.EnqueueCopy(*message, message->GetLength(),
        MessageMetadata<UnsubscribeCallbackFunc>(mConfig.GetAddress(), mConfig.GetPort(), mPacketId, TimerMilli::GetNow(),
            mConfig.GetRetransmissionTimeout() * 1000, aCallback, aContext)));
    mPacketId++;

exit:
    return error;
}

otError MqttsnClient::Disconnect()
{
    otError error = OT_ERROR_NONE;
    int32_t length = -1;
    Message* message = nullptr;
    unsigned char buffer[MAX_PACKET_SIZE];

    if (mClientState != kStateActive && mClientState != kStateAwake
        && mClientState != kStateAsleep)
    {
        error = OT_ERROR_INVALID_STATE;
        goto exit;
    }

    length = MQTTSNSerialize_disconnect(buffer, MAX_PACKET_SIZE, 0);
    if (length <= 0)
    {
        error = OT_ERROR_FAILED;
        goto exit;
    }
    SuccessOrExit(error = NewMessage(&message, buffer, length));
    SuccessOrExit(error = SendMessage(*message));

    mDisconnectRequested = true;
    mGwTimeout = TimerMilli::GetNow() + mConfig.GetRetransmissionTimeout() * 1000;

exit:
    return error;
}

otError MqttsnClient::Sleep(uint16_t aDuration)
{
    otError error = OT_ERROR_NONE;
    int32_t length = -1;
    Message* message = nullptr;
    unsigned char buffer[MAX_PACKET_SIZE];

    if (mClientState != kStateActive && mClientState != kStateAwake && mClientState != kStateAsleep)
    {
        error = OT_ERROR_INVALID_STATE;
        goto exit;
    }

    length = MQTTSNSerialize_disconnect(buffer, MAX_PACKET_SIZE, aDuration);
    if (length <= 0)
    {
        error = OT_ERROR_FAILED;
        goto exit;
    }

    SuccessOrExit(error = NewMessage(&message, buffer, length));
    SuccessOrExit(error = SendMessage(*message));

    mSleepRequested = true;
    mGwTimeout = TimerMilli::GetNow() + mConfig.GetRetransmissionTimeout() * 1000;

exit:
    return error;
}

otError MqttsnClient::Awake(uint32_t aTimeout)
{
    otError error = OT_ERROR_NONE;
    if (mClientState != kStateAwake && mClientState != kStateAsleep)
    {
        error = OT_ERROR_INVALID_STATE;
        goto exit;
    }

    SuccessOrExit(error = PingGateway());

    mClientState = kStateAwake;
    mGwTimeout = TimerMilli::GetNow() + aTimeout * 1000;
exit:
    return error;
}

otError MqttsnClient::SearchGateway(const Ip6::Address &aMulticastAddress, uint16_t aPort, uint8_t aRadius)
{
    otError error = OT_ERROR_NONE;
    int32_t length = -1;
    Message* message = nullptr;
    unsigned char buffer[MAX_PACKET_SIZE];

    length = MQTTSNSerialize_searchgw(buffer, MAX_PACKET_SIZE, aRadius);
    if (length <= 0)
    {
        error = OT_ERROR_FAILED;
        goto exit;
    }

    SuccessOrExit(error = NewMessage(&message, buffer, length));
    SuccessOrExit(error = SendMessage(*message, aMulticastAddress, aPort, aRadius));

exit:
    return error;
}

ClientState MqttsnClient::GetState(ClientState aState)
{
    return mClientState;
}

otError MqttsnClient::SetConnectedCallback(ConnectCallbackFunc aCallback, void* aContext)
{
    mConnectCallback = aCallback;
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
        mPingReqTime = TimerMilli::GetNow() + (mConfig.GetKeepAlive() - KEEP_ALIVE_DELAY) * 1000;
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
    unsigned char buffer[MAX_PACKET_SIZE];

    if (mClientState != kStateActive && mClientState != kStateAwake)
    {
        error = OT_ERROR_INVALID_STATE;
        goto exit;
    }

    MQTTSNString clientId;
    memset(&clientId, 0, sizeof(clientId));
    clientId.cstring = const_cast<char *>(mConfig.GetClientId().AsCString());
    length = MQTTSNSerialize_pingreq(buffer, MAX_PACKET_SIZE, clientId);

    if (length <= 0)
    {
        error = OT_ERROR_FAILED;
        goto exit;
    }
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
}

bool MqttsnClient::VerifyGatewayAddress(const Ip6::MessageInfo &aMessageInfo)
{
    return aMessageInfo.GetPeerAddr() == mConfig.GetAddress()
        && aMessageInfo.GetPeerPort() == mConfig.GetPort();
}

void MqttsnClient::HandleSubscribeTimeout(const MessageMetadata<SubscribeCallbackFunc> &aMetadata, void* aContext)
{
    OT_UNUSED_VARIABLE(aContext);

    MqttsnClient* client = static_cast<MqttsnClient*>(aContext);
    client->mTimeoutRaised = true;
    aMetadata.mCallback(kCodeTimeout, 0, kQos0, aMetadata.mContext);
}

void MqttsnClient::HandleRegisterTimeout(const MessageMetadata<RegisterCallbackFunc> &aMetadata, void* aContext)
{
    OT_UNUSED_VARIABLE(aContext);

    MqttsnClient* client = static_cast<MqttsnClient*>(aContext);
    client->mTimeoutRaised = true;
    aMetadata.mCallback(kCodeTimeout, 0, aMetadata.mContext);
}

void MqttsnClient::HandleUnsubscribeTimeout(const MessageMetadata<UnsubscribeCallbackFunc> &aMetadata, void* aContext)
{
    OT_UNUSED_VARIABLE(aContext);

    MqttsnClient* client = static_cast<MqttsnClient*>(aContext);
    client->mTimeoutRaised = true;
    aMetadata.mCallback(kCodeTimeout, aMetadata.mContext);
}

void MqttsnClient::HandlePublishTimeout(const MessageMetadata<PublishCallbackFunc> &aMetadata, void* aContext)
{
    OT_UNUSED_VARIABLE(aContext);

    MqttsnClient* client = static_cast<MqttsnClient*>(aContext);
    client->mTimeoutRaised = true;
    aMetadata.mCallback(kCodeTimeout, 0, aMetadata.mContext);
}

}
}
