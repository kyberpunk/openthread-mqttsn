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

#ifndef MQTTSN_CLIENT_HPP_
#define MQTTSN_CLIENT_HPP_

#include "common/locator.hpp"
#include "common/instance.hpp"
#include "net/ip6_address.hpp"
#include "net/udp6.hpp"
#include "openthread/error.h"

/**
 * @file
 *   This file includes interface for MQTT-SN protocol v1.2 client.
 */

namespace ot {

namespace Mqttsn {

/**
 * MQTT-SN message return code.
 */
enum ReturnCode
{
    kCodeAccepted = 0,
    kCodeRejectedCongestion = 1,
    kCodeRejectedTopicId = 2,
    kCodeRejectedNotSupported = 3,
    /**
     * Pending message timed out. this value is not returned by gateway.
     */
    kCodeTimeout = -1,
};

/**
 * MQTT-SN quality of service level.
 */
enum Qos
{
    kQos0 = 0x0,
    kQos1 = 0x1,
    kQos2 = 0x2,
    kQosm1 = 0x3
};

/**
 * Disconnected state reason.
 */
enum DisconnectType
{
    /**
     * Client was disconnected by gateway/broker.
     */
    kServer,
    /**
     * Disconnection was invoked by client.
     */
    kClient,
    /**
     * Client changed state to asleep
     */
    kAsleep,
    /**
     * Communication timeout.
     */
    kTimeout
};

/**
 * Client lifecycle states.
 */
enum ClientState
{
    /**
     * Client is not connected to gateway.
     */
    kStateDisconnected,
    /**
     * Client is connected to gateway and currently alive.
     */
    kStateActive,
    /**
     * Client is in sleeping state.
     */
    kStateAsleep,
    /**
     * Client is awaken from sleep.
     */
    kStateAwake,
    /**
     * Client connection is lost due to communication error.
     */
    kStateLost,
};

enum
{
    /**
     * Client ID maximal length.
     */
    kCliendIdStringMax = 24,
    /**
     * Long topic name maximal length (with null terminator).
     */
    kMaxTopicNameLength = 50,
    /**
     * Short topic maximal length (with null terminator).
     */
    kShortTopicNameLength = 3
};

/**
 * MQTT-SN topic identificator type.
 */
enum TopicIdType
{
    /**
     * Predefined topic ID.
     */
    kTopicId,
    /**
     * Two character short topic name.
     */
    kShortTopicName,
    /**
     * Long topic name.
     */
    kTopicName
};

/**
 * Topic ID type.
 */
typedef uint16_t TopicId;

/**
 * Short topic name string.
 */
typedef String<kShortTopicNameLength> ShortTopicNameString;

/**
 * Long topic name string.
 */
typedef String<kMaxTopicNameLength> TopicNameString;

/**
 * Client ID string.
 */
typedef String<kCliendIdStringMax> ClientIdString;

template <typename CallbackType>
class WaitingMessagesQueue;

/**
 * Message metadata which are stored in waiting messages queue.
 */
template <typename CallbackType>
class MessageMetadata
{
    friend class MqttsnClient;
    friend class WaitingMessagesQueue<CallbackType>;

public:
    /**
     * Default constructor for the object.
     *
     */
    MessageMetadata(void);

    /**
     * This constructor initializes the object with specific values.
     *
     * @param[in]  aDestinationAddress     Reference of message destination IPv6 address.
     * @param[in]  aDestinationPort        Message destination port.
     * @param[in]  aMessageId              MQTT-SN Message ID.
     * @param[in]  aTimestamp              Time stamp of message in milliseconds for timeout evaluation.
     * @param[in]  aRetransmissionTimeout  Time in millisecond after which message is message timeout invoked.
     * @param[in]  aCallback               Function pointer for handling message timeout.
     * @param[in]  aContext                Pointer to callback passed to timeout callback.
     */
    MessageMetadata(const Ip6::Address &aDestinationAddress, uint16_t aDestinationPort, uint16_t aMessageId, uint32_t aTimestamp, uint32_t aRetransmissionTimeout, CallbackType aCallback, void* aContext);

    /**
     * Append metadata to the message.
     *
     * @param[in]  aMessage  A reference to the message.
     *
     * @retval OT_ERROR_NONE     Successfully appended the bytes.
     * @retval OT_ERROR_NO_BUFS  Insufficient available buffers to grow the message.
     *
     */
    otError AppendTo(Message &aMessage) const;

    /**
     * Read metadata from the message.
     *
     * @param[in]  aMessage  A reference to the message.
     *
     * @returns The number of bytes read.
     *
     */
    uint16_t ReadFrom(Message &aMessage);

    /**
     * Get metadata length in bytes.
     *
     * @returns The number of bytes.
     */
    uint16_t GetLength(void) const;

private:
    /**
     * Reference of message destination IPv6 address.
     */
    Ip6::Address mDestinationAddress;
    /**
     * Message destination port.
     */
    uint16_t mDestinationPort;
    /**
     * MQTT-SN Message ID.
     */
    uint16_t mMessageId;
    /**
     * Time stamp of message in milliseconds for timeout evaluation.
     */
    uint32_t mTimestamp;
    /**
     * Time in millisecond after which message is message timeout invoked.
     */
    uint32_t mRetransmissionTimeout;
    /**
     * Message retransmission count.
     */
    uint8_t mRetransmissionCount;
    /**
     * Function pointer for handling message timeout.
     */
    CallbackType mCallback;
    /**
     * Pointer to callback passed to timeout callback.
     */
    void* mContext;
};

/**
 * Class represents the queue which contains messages waiting for acknowledgements from gateway.
 */
template <typename CallbackType>
class WaitingMessagesQueue
{
public:
    /**
     * Declaration of function pointer which is used as timeout callback.
     */
    typedef void (*TimeoutCallbackFunc)(const MessageMetadata<CallbackType> &aMetadata, void* aContext);

    /**
     * This constructor initializes the object with specific values.
     *
     * @param[in]  aTimeoutCallback  Function pointer to callback which is invoked on message timeout.
     * @param[in]  aTimeoutContext   Pointer to context passed to timeout callback.
     */
    WaitingMessagesQueue(TimeoutCallbackFunc aTimeoutCallback, void* aTimeoutContext);

    /**
     * Default object destructor.
     *
     */
    ~WaitingMessagesQueue(void);

    /**
     * Copy message data and enqueue message to waiting queue.
     *
     * @param[in]  aMessage   Reference to message object to be enqueued.
     * @param[in]  aLength    Message length.
     * @param[in]  aMetadata  Reference to message metadata.
     *
     * @retval OT_ERROR_NONE     Successfully enqueued the message.
     * @retval OT_ERROR_NO_BUFS  Insufficient available buffers to copy or enqueue the message.
     */
    otError EnqueueCopy(const Message &aMessage, uint16_t aLength, const MessageMetadata<CallbackType> &aMetadata);

    /**
     * Dequeue specific message from waiting queue.
     *
     * @param[in]  aMessage   Reference to message object to be dequeued.
     *
     * @retval OT_ERROR_NONE       Successfully dequeued the message.
     * @retval OT_ERROR_NOT_FOUND  Message was not found in waiting queue.
     */
    otError Dequeue(Message &aMessage);

    /**
     * Find message by message ID and read message metadata.
     *
     * @param[in]  aMessageId  Message ID.
     * @param[out] aMetadata   Reference to initialized metadata object.
     *
     * @returns  If the message is found the message ID is returned or null otherwise.
     */
    Message* Find(uint16_t aMessageId, MessageMetadata<CallbackType> &aMetadata);

    /**
     * Evaluate queued messages timeout and retransmission.
     *
     * @retval OT_ERROR_NONE  Timeouts were successfully processed.
     */
    otError HandleTimer(void);

    /**
     * Force waiting messages timeout, invoke callback and empty queue.
     */
    void ForceTimeout(void);

private:
    MessageQueue mQueue;
    TimeoutCallbackFunc mTimeoutCallback;
    void* mTimeoutContext;
};

class MqttsnConfig
{
public:

    MqttsnConfig(void)
        : mAddress()
        , mPort()
        , mClientId()
        , mKeepAlive(30)
        , mCleanSession()
        , mRetransmissionTimeout(10)
    {
        ;
    }

    const Ip6::Address &GetAddress()
    {
        return mAddress;
    }

    void SetAddress(const Ip6::Address &aAddress)
    {
        mAddress = aAddress;
    }

    uint16_t GetPort()
    {
        return mPort;
    }

    void SetPort(uint16_t aPort)
    {
        mPort = aPort;
    }

    const ClientIdString &GetClientId()
    {
        return mClientId;
    }

    void SetClientId(const char* aClientId)
    {
        mClientId.Set("%s", aClientId);
    }

    int16_t GetKeepAlive()
    {
        return mKeepAlive;
    }

    void SetKeepAlive(int16_t aDuration)
    {
        mKeepAlive = aDuration;
    }

    bool GetCleanSession()
    {
        return mCleanSession;
    }

    void SetCleanSession(bool aCleanSession)
    {
        mCleanSession = aCleanSession;
    }

    uint32_t GetRetransmissionTimeout()
    {
        return mRetransmissionTimeout;
    }

    void SetRetransmissionTimeout(uint32_t aTimeout)
    {
        mRetransmissionTimeout = aTimeout;
    }

private:
    Ip6::Address mAddress;
    uint16_t mPort;
    ClientIdString mClientId;
    uint16_t mKeepAlive;
    bool mCleanSession;
    uint32_t mRetransmissionTimeout;
};

class MqttsnClient: public InstanceLocator
{
public:

    typedef void (*ConnectCallbackFunc)(ReturnCode aCode, void* aContext);

    typedef void (*SubscribeCallbackFunc)(ReturnCode aCode, TopicId topicId, Qos aQos, void* aContext);

    typedef ReturnCode (*PublishReceivedCallbackFunc)(const uint8_t* aPayload, int32_t aPayloadLength, TopicIdType aTopicIdType, TopicId aTopicId, ShortTopicNameString aShortTopicName, void* aContext);

    typedef void (*AdvertiseCallbackFunc)(const Ip6::Address &aAddress, uint8_t aGatewayId, uint32_t aDuration, void* aContext);

    typedef void (*SearchGwCallbackFunc)(const Ip6::Address &aAddress, uint8_t aGatewayId, void* aContext);

    typedef void (*RegisterCallbackFunc)(ReturnCode aCode, TopicId aTopicId, void* aContext);

    typedef ReturnCode (*RegisterReceivedCallbackFunc)(TopicId aTopicId, const TopicNameString &aTopicName, void* aContext);

    typedef void (*PublishCallbackFunc)(ReturnCode aCode, TopicId aTopicId, void* aContext);

    typedef void (*UnsubscribeCallbackFunc)(ReturnCode aCode, void* aContext);

    typedef void (*DisconnectedCallbackFunc)(DisconnectType aType, void* aContext);

    MqttsnClient(Instance &aInstance);

    ~MqttsnClient(void);

    otError Start(uint16_t aPort);

    otError Stop(void);

    otError Process(void);

    otError Connect(MqttsnConfig &aConfig);

    otError Subscribe(const char* aTopicName, bool aIsShortTopicName, Qos aQos, SubscribeCallbackFunc aCallback, void* aContext);

    otError Subscribe(TopicId aTopicId, Qos aQos, SubscribeCallbackFunc aCallback, void* aContext);

    otError Register(const char* aTopicName, RegisterCallbackFunc aCallback, void* aContext);

    otError Publish(const uint8_t* aData, int32_t aLenght, Qos aQos, const char* aShortTopicName, PublishCallbackFunc aCallback, void* aContext);

    otError Publish(const uint8_t* aData, int32_t aLenght, Qos aQos, TopicId aTopicId, PublishCallbackFunc aCallback, void* aContext);

    otError Unsubscribe(const char* aShortTopicName, UnsubscribeCallbackFunc aCallback, void* aContext);

    otError Unsubscribe(TopicId aTopicId, UnsubscribeCallbackFunc aCallback, void* aContext);

    otError Disconnect(void);

    otError Sleep(uint16_t aDuration);

    otError Awake(uint32_t aTimeout);

    otError SearchGateway(const Ip6::Address &aMulticastAddress, uint16_t aPort, uint8_t aRadius);

    ClientState GetState(ClientState aState);

    otError SetConnectedCallback(ConnectCallbackFunc aCallback, void* aContext);

    otError SetPublishReceivedCallback(PublishReceivedCallbackFunc aCallback, void* aContext);

    otError SetAdvertiseCallback(AdvertiseCallbackFunc aCallback, void* aContext);

    otError SetSearchGwCallback(SearchGwCallbackFunc aCallback, void* aContext);

    otError SetDisconnectedCallback(DisconnectedCallbackFunc aCallback, void* aContext);

    otError SetRegisterReceivedCallback(RegisterReceivedCallbackFunc aCallback, void* aContext);

private:
    static void HandleUdpReceive(void *aContext, otMessage *aMessage, const otMessageInfo *aMessageInfo);

    otError NewMessage(Message **aMessage, unsigned char* aBuffer, int32_t aLength);

    otError SendMessage(Message &aMessage);

    otError SendMessage(Message &aMessage, const Ip6::Address &aAddress, uint16_t aPort);

    otError SendMessage(Message &aMessage, const Ip6::Address &aAddress, uint16_t aPort, uint8_t aHopLimit);

    otError PingGateway(void);

    void OnDisconnected(void);

    bool VerifyGatewayAddress(const Ip6::MessageInfo &aMessageInfo);

    static void HandleSubscribeTimeout(const MessageMetadata<SubscribeCallbackFunc> &aMetadata, void* aContext);

    static void HandleRegisterTimeout(const MessageMetadata<RegisterCallbackFunc> &aMetadata, void* aContext);

    static void HandleUnsubscribeTimeout(const MessageMetadata<UnsubscribeCallbackFunc> &aMetadata, void* aContext);

    static void HandlePublishTimeout(const MessageMetadata<PublishCallbackFunc> &aMetadata, void* aContext);

    Ip6::UdpSocket mSocket;
    MqttsnConfig mConfig;
    uint16_t mMessageId;
    uint32_t mPingReqTime;
    uint32_t mGwTimeout;
    bool mDisconnectRequested;
    bool mSleepRequested;
    bool mTimeoutRaised;
    ClientState mClientState;
    WaitingMessagesQueue<SubscribeCallbackFunc> mSubscribeQueue;
    WaitingMessagesQueue<RegisterCallbackFunc> mRegisterQueue;
    WaitingMessagesQueue<UnsubscribeCallbackFunc> mUnsubscribeQueue;
    WaitingMessagesQueue<PublishCallbackFunc> mPublishQos1Queue;
    ConnectCallbackFunc mConnectCallback;
    void* mConnectContext;
    PublishReceivedCallbackFunc mPublishReceivedCallback;
    void* mPublishReceivedContext;
    AdvertiseCallbackFunc mAdvertiseCallback;
    void* mAdvertiseContext;
    SearchGwCallbackFunc mSearchGwCallback;
    void* mSearchGwContext;
    DisconnectedCallbackFunc mDisconnectedCallback;
    void* mDisconnectedContext;
    RegisterReceivedCallbackFunc mRegisterReceivedCallback;
    void* mRegisterReceivedContext;
};

}
}

#endif /* MQTTSN_CLIENT_HPP_ */
