#ifndef MQTTSN_CLIENT_HPP_
#define MQTTSN_CLIENT_HPP_

#include "common/locator.hpp"
#include "common/instance.hpp"
#include "net/ip6_address.hpp"
#include "net/udp6.hpp"
#include "openthread/error.h"

namespace ot {

namespace Mqttsn {

enum ReturnCode
{
    kCodeAccepted = 0,
    kCodeRejectedCongestion = 1,
    kCodeRejectedTopicId = 2,
    kCodeRejectedNotSupported = 3,
    kCodeTimeout = -1,
};

enum Qos
{
    kQos0 = 0x0,
    kQos1 = 0x1,
    kQos2 = 0x2,
    kQosm1 = 0x3
};

enum DisconnectType
{
    kServer,
    kClient,
    kAsleep,
    kTimeout
};

enum ClientState
{
    kStateDisconnected,
    kStateActive,
    kStateAsleep,
    kStateAwake,
    kStateLost,
};

typedef uint16_t TopicId;

template <typename CallbackType>
class WaitingMessagesQueue;

template <typename CallbackType>
class MessageMetadata
{
    friend class MqttsnClient;
    friend class WaitingMessagesQueue<CallbackType>;

public:
    MessageMetadata(void);

    MessageMetadata(const Ip6::Address &aDestinationAddress, uint16_t aDestinationPort, uint16_t aPacketId, uint32_t aTimestamp, uint32_t aRetransmissionTimeout, CallbackType aCallback, void* aContext);

    otError AppendTo(Message &aMessage) const;

    uint16_t ReadFrom(Message &aMessage);

    uint16_t GetLength(void) const;

private:
    Ip6::Address mDestinationAddress;
    uint16_t mDestinationPort;
    uint16_t mPacketId;
    uint32_t mTimestamp;
    uint32_t mRetransmissionTimeout;
    uint8_t mRetransmissionCount;
    CallbackType mCallback;
    void* mContext;
};

template <typename CallbackType>
class WaitingMessagesQueue
{
public:
    typedef void (*TimeoutCallbackFunc)(const MessageMetadata<CallbackType> &aMetadata, void* aContext);

    WaitingMessagesQueue(TimeoutCallbackFunc aTimeoutCallback, void* aTimeoutContext);

    ~WaitingMessagesQueue(void);

    otError EnqueueCopy(const Message &aMessage, uint16_t aLength, const MessageMetadata<CallbackType> &aMetadata);

    otError Dequeue(Message &aMessage);

    Message* Find(uint16_t aPacketId, MessageMetadata<CallbackType> &aMetadata);

    otError HandleTimer(void);

    void ForceTimeout(void);

private:
    MessageQueue mQueue;
    TimeoutCallbackFunc mTimeoutCallback;
    void* mTimeoutContext;
};

class MqttsnConfig
{
public:
    enum
    {
        kCliendIdStringMax = 24
    };

    typedef String<kCliendIdStringMax> ClientIdString;

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
    enum
    {
        kMaxTopicPayloadLength = 255,
        kMaxTopicNameLength = 50
    };

    typedef String<kMaxTopicNameLength> TopicNameString;

    typedef void (*ConnectCallbackFunc)(ReturnCode aCode, void* aContext);

    typedef void (*SubscribeCallbackFunc)(ReturnCode aCode, TopicId topicId, void* aContext);

    typedef ReturnCode (*PublishReceivedCallbackFunc)(const uint8_t* aPayload, int32_t aPayloadLength, TopicId aTopicId, void* aContext);

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

    // TODO: Overload for other topic types
    otError Subscribe(const char* aTopicName, Qos aQos, SubscribeCallbackFunc aCallback, void* aContext);

    otError Register(const char* aTopicName, RegisterCallbackFunc aCallback, void* aContext);

    otError Publish(const uint8_t* aData, int32_t aLenght, Qos aQos, TopicId aTopicId, PublishCallbackFunc aCallback, void* aContext);

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
    uint16_t mPacketId;
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
