#ifndef MQTTSN_CLIENT_HPP_
#define MQTTSN_CLIENT_HPP_

#include <string>

#include "common/locator.hpp"
#include "common/instance.hpp"
#include "net/ip6_address.hpp"
#include "net/udp6.hpp"
#include "openthread/error.h"

// TODO: Implement message queue and message specific callback

namespace ot {

namespace Mqttsn {

enum ReturnCode
{
    MQTTSN_CODE_ACCEPTED = 0,
    MQTTSN_CODE_REJECTED_CONGESTION = 1,
    MQTTSN_CODE_REJECTED_TOPIC_ID = 2,
    MQTTSN_CODE_REJECTED_NOT_SUPPORTED = 3
};

enum Qos
{
    MQTTSN_QOS0 = 0x0,
    MQTTSN_QOS1 = 0x1,
    MQTTSN_QOS2 = 0x2,
    MQTTSN_QOSm1 = 0x3
};

enum DisconnectType
{
    MQTTSN_DISCONNECT_SERVER,
    MQTTSN_DISCONNECT_CLIENT,
    MQTTSN_DISCONNECT_ASLEEP,
    MQTTSN_DISCONNECT_TIMEOUT
};

enum ClientState
{
    MQTTSN_STATE_DISCONNECTED,
    MQTTSN_STATE_ACTIVE,
    MQTTSN_STATE_ASLEEP,
    MQTTSN_STATE_AWAKE,
    MQTTSN_STATE_LOST,
};

typedef uint16_t TopicId;

class MqttsnConfig
{
public:
    MqttsnConfig(void)
        : mAddress()
        , mPort()
        , mClientId()
        , mKeepAlive(30)
        , mCleanSession()
        , mGatewayTimeout(10)
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

    const std::string &GetClientId()
    {
        return mClientId;
    }

    void SetClientId(const std::string &aClientId)
    {
        mClientId = aClientId;
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

    uint32_t GetGatewayTimeout()
    {
        return mGatewayTimeout;
    }

    void SetGatewayTimeout(uint32_t aTimeout)
    {
        mGatewayTimeout = aTimeout;
    }

private:
    Ip6::Address mAddress;
    uint16_t mPort;
    std::string mClientId;
    uint16_t mKeepAlive;
    bool mCleanSession;
    uint32_t mGatewayTimeout;
};

class MqttsnClient: public InstanceLocator
{
public:
    typedef void (*ConnectCallbackFunc)(ReturnCode aCode, void* aContext);

    typedef void (*SubscribeCallbackFunc)(ReturnCode aCode, TopicId topicId, void* aContext);

    typedef void (*PublishReceivedCallbackFunc)(const uint8_t* aPayload, int32_t aPayloadLength, Qos aQos, TopicId aTopicId, void* aContext);

    typedef void (*AdvertiseCallbackFunc)(const Ip6::Address &aAddress, uint16_t aPort, uint8_t aGatewayId, uint32_t aDuration, void* aContext);

    typedef void (*SearchGwCallbackFunc)(const Ip6::Address &aAddress, uint16_t aPort, uint8_t aGatewayId, void* aContext);

    typedef void (*RegisterCallbackFunc)(ReturnCode aCode, TopicId aTopicId, void* aContext);

    typedef void (*PublishedCallbackFunc)(ReturnCode aCode, TopicId aTopicId, void* aContext);

    typedef void (*UnsubscribedCallbackFunc)(void* aContext);

    typedef void (*DisconnectedCallbackFunc)(DisconnectType aType, void* aContext);

    MqttsnClient(Instance &aInstance);

    otError Start(uint16_t aPort);

    otError Stop(void);

    otError Process(void);

    otError Connect(MqttsnConfig &aConfig);

    // TODO: Overload for other topic types
    otError Subscribe(const std::string &aTopic, Qos aQos);

    otError Register(const std::string &aTopic);

    otError Publish(const uint8_t* aData, int32_t aLenght, Qos aQos, TopicId aTopicId);

    otError Unsubscribe(TopicId aTopicId);

    otError Disconnect(void);

    otError Sleep(uint16_t aDuration);

    otError Awake(uint32_t aTimeout);

    otError SearchGateway(const Ip6::Address &aMulticastAddress, uint16_t aPort, uint8_t aRadius);

    ClientState GetState(ClientState aState);

    otError SetConnectedCallback(ConnectCallbackFunc aCallback, void* aContext);

    otError SetSubscribeCallback(SubscribeCallbackFunc aCallback, void* aContext);

    otError SetPublishReceivedCallback(PublishReceivedCallbackFunc aCallback, void* aContext);

    otError SetAdvertiseCallback(AdvertiseCallbackFunc aCallback, void* aContext);

    otError SetSearchGwCallback(SearchGwCallbackFunc aCallback, void* aContext);

    otError SetRegisterCallback(RegisterCallbackFunc aCallback, void* aContext);

    otError SetPublishedCallback(PublishedCallbackFunc aCallback, void* aContext);

    otError SetUnsubscribedCallback(UnsubscribedCallbackFunc aCallback, void* aContext);

    otError SetDisconnectedCallback(DisconnectedCallbackFunc aCallback, void* aContext);

private:
    static void HandleUdpReceive(void *aContext, otMessage *aMessage, const otMessageInfo *aMessageInfo);

    otError SendMessage(unsigned char* aBuffer, int32_t aLength);

    otError SendMessage(unsigned char* aBuffer, int32_t aLength, const Ip6::Address &aAddress, uint16_t aPort);

    otError SendMessage(unsigned char* aBuffer, int32_t aLength, const Ip6::Address &aAddress, uint16_t aPort, uint8_t aHopLimit);

    otError PingGateway(void);

    void OnDisconnected(void);

    bool VerifyGatewayAddress(const Ip6::MessageInfo &aMessageInfo);

    Ip6::UdpSocket mSocket;
    MqttsnConfig mConfig;
    uint16_t mPacketId;
    uint32_t mPingReqTime;
    uint32_t mGwTimeout;
    bool mDisconnectRequested;
    bool mSleepRequested;
    ClientState mClientState;
    ConnectCallbackFunc mConnectCallback;
    void* mConnectContext;
    SubscribeCallbackFunc mSubscribeCallback;
    void* mSubscribeContext;
    PublishReceivedCallbackFunc mPublishReceivedCallback;
    void* mPublishReceivedContext;
    AdvertiseCallbackFunc mAdvertiseCallback;
    void* mAdvertiseContext;
    SearchGwCallbackFunc mSearchGwCallback;
    void* mSearchGwContext;
    RegisterCallbackFunc mRegisterCallback;
    void* mRegisterContext;
    PublishedCallbackFunc mPublishedCallback;
    void* mPublishedContext;
    UnsubscribedCallbackFunc mUnsubscribedCallback;
    void* mUnsubscribedContext;
    DisconnectedCallbackFunc mDisconnectedCallback;
    void* mDisconnectedContext;
};

}
}

#endif /* MQTTSN_CLIENT_HPP_ */
