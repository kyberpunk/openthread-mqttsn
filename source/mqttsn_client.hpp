#ifndef MQTTSN_CLIENT_HPP_
#define MQTTSN_CLIENT_HPP_

#include <string>

#include "common/locator.hpp"
#include "common/instance.hpp"
#include "net/ip6_address.hpp"
#include "net/udp6.hpp"
#include "openthread/error.h"

namespace ot {

namespace Mqttsn {

enum ReturnCode {
	MQTTSN_CODE_ACCEPTED = 0,
	MQTTSN_CODE_REJECTED_CONGESTION = 1,
	MQTTSN_CODE_REJECTED_TOPIC_ID = 2,
	MQTTSN_CODE_REJECTED_NOT_SUPPORTED = 3
};

enum EventType {
	MQTTSN_NONE = 0,
	MQTTSN_CONNECTED = 1
};

typedef uint16_t TopicId;

class MqttsnConfig {
public:
	MqttsnConfig(void)
		: mAddress()
		, mPort()
		, mClientId()
		, mDuration()
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

	int16_t GetDuration() {
		return mDuration;
	}

	void SetDuration(int16_t duration) {
		mDuration = duration;
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
	int16_t mDuration;
	bool mCleanSession;
};

class MqttsnClient : public InstanceLocator
{
public:
	typedef void (*ConnectCallbackFunc)(ReturnCode code, void* context);

	typedef void (*SubscribeCallbackFunc)(ReturnCode code, void* context);

	typedef void (*PublishReceivedCallbackFunc)(const uint8_t* payload, int32_t payloadLength, void* context);

	typedef void (*AdvertiseCallbackFunc)(const Ip6::Address &address, uint16_t port, uint8_t gatewayId, uint32_t duration, void* context);

	typedef void (*SearchGwCallbackFunc)(const Ip6::Address &address, uint8_t gatewayId, void* context);

	typedef void (*RegisterCallbackFunc)(ReturnCode code, TopicId topicId, void* context);

	MqttsnClient(Instance &aInstance);

	otError Start(uint16_t port);

	otError Stop(void);

	otError Connect(MqttsnConfig &config);

	otError Subscribe(const std::string &topic);

	otError Register(const std::string &topic);

	otError SetConnectCallback(ConnectCallbackFunc callback, void* context);

	otError SetSubscribeCallback(SubscribeCallbackFunc callback, void* context);

	otError SetPublishReceivedCallback(PublishReceivedCallbackFunc callback, void* context);

	otError SetAdvertiseCallback(AdvertiseCallbackFunc callback, void* context);

	otError SetSearchGwCallback(SearchGwCallbackFunc callback, void* context);

	otError SetRegisterCallback(RegisterCallbackFunc callback, void* context);

private:
	static void HandleUdpReceive(void *aContext, otMessage *aMessage, const otMessageInfo *aMessageInfo);

	otError SendMessage(unsigned char* buffer, int32_t length);

	otError SendMessage(unsigned char* buffer, int32_t length, const Ip6::Address &address, uint16_t port);

	Ip6::UdpSocket mSocket;
	bool mIsConnected;
	MqttsnConfig mConfig;
	int32_t mPacketId;
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
};

}
}

#endif /* MQTTSN_CLIENT_HPP_ */
