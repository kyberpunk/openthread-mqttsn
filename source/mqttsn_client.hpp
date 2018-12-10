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

enum ReturnCode {
	MQTTSN_CODE_ACCEPTED = 0,
	MQTTSN_CODE_REJECTED_CONGESTION = 1,
	MQTTSN_CODE_REJECTED_TOPIC_ID = 2,
	MQTTSN_CODE_REJECTED_NOT_SUPPORTED = 3
};

enum Qos {
	MQTTSN_QOS0 = 0x0,
	MQTTSN_QOS1 = 0x1,
	MQTTSN_QOS2 = 0x2,
	MQTTSN_QOSm1 = 0x3
};

typedef uint16_t TopicId;

class MqttsnConfig {
public:
	MqttsnConfig(void)
		: mAddress()
		, mPort()
		, mClientId()
		, mKeepAlive(30)
		, mCleanSession() {
		;
	}

	const Ip6::Address &GetAddress() {
		return mAddress;
	}

	void SetAddress(Ip6::Address &address) {
		mAddress = address;
	}

	uint16_t GetPort() {
		return mPort;
	}

	void SetPort(uint16_t port) {
		mPort = port;
	}

	const std::string &GetClientId() {
		return mClientId;
	}

	void SetClientId(const std::string &clientId) {
		mClientId = clientId;
	}

	int16_t GetKeepAlive() {
		return mKeepAlive;
	}

	void SetKeepAlive(int16_t duration) {
		mKeepAlive = duration;
	}

	bool GetCleanSession() {
		return mCleanSession;
	}

	void SetCleanSession(bool cleanSession) {
		mCleanSession = cleanSession;
	}

private:
	Ip6::Address mAddress;
	uint16_t mPort;
	std::string mClientId;
	uint16_t mKeepAlive;
	bool mCleanSession;

};

class MqttsnClient : public InstanceLocator
{
public:
	typedef void (*ConnectCallbackFunc)(ReturnCode code, void* context);

	typedef void (*SubscribeCallbackFunc)(ReturnCode code, TopicId topicId, void* context);

	typedef void (*PublishReceivedCallbackFunc)(const uint8_t* payload, int32_t payloadLength, Qos qos, TopicId topicId, void* context);

	typedef void (*AdvertiseCallbackFunc)(const Ip6::Address &address, uint16_t port, uint8_t gatewayId, uint32_t duration, void* context);

	typedef void (*SearchGwCallbackFunc)(const Ip6::Address &address, uint8_t gatewayId, void* context);

	typedef void (*RegisterCallbackFunc)(ReturnCode code, TopicId topicId, void* context);

	typedef void (*PublishedCallbackFunc)(ReturnCode code, TopicId topicId, void* context);

	typedef void (*UnsubscribedCallbackFunc)(void* context);

	typedef void (*DisconnectedCallbackFunc)(void* context);

	MqttsnClient(Instance &aInstance);

	otError Start(uint16_t port);

	otError Stop(void);

	otError Process(void);

	otError Connect(MqttsnConfig &config);

	// TODO: Overload for other topic types
	otError Subscribe(const std::string &topic, Qos qos);

	otError Register(const std::string &topic);

	otError Publish(const uint8_t* data, int32_t lenght, Qos qos, TopicId topicId);

	otError Unsubscribe(TopicId topicId);

	otError Disconnect(void);

	otError Sleep(void);

	otError SetConnectedCallback(ConnectCallbackFunc callback, void* context);

	otError SetSubscribeCallback(SubscribeCallbackFunc callback, void* context);

	otError SetPublishReceivedCallback(PublishReceivedCallbackFunc callback, void* context);

	otError SetAdvertiseCallback(AdvertiseCallbackFunc callback, void* context);

	otError SetSearchGwCallback(SearchGwCallbackFunc callback, void* context);

	otError SetRegisterCallback(RegisterCallbackFunc callback, void* context);

	otError SetPublishedCallback(PublishedCallbackFunc callback, void* context);

	otError SetUnsubscribedCallback(UnsubscribedCallbackFunc callback, void* context);

	otError SetDisconnectedCallback(DisconnectedCallbackFunc callback, void* context);

private:
	static void HandleUdpReceive(void *aContext, otMessage *aMessage, const otMessageInfo *aMessageInfo);

	otError SendMessage(unsigned char* buffer, int32_t length);

	otError SendMessage(unsigned char* buffer, int32_t length, const Ip6::Address &address, uint16_t port);

	otError PingGateway(void);

	Ip6::UdpSocket mSocket;
	bool mIsConnected;
	MqttsnConfig mConfig;
	uint16_t mPacketId;
	bool mIsSleeping;
	uint32_t mPingReqTime;
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
