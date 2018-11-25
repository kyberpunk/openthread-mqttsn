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

typedef void (*ConnectCallbackFunc)(ReturnCode code);

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

	Ip6::Address &GetAddress() {
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

	std::string &GetClientId() {
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
	MqttsnClient(Instance &aInstance);

	otError Start(uint16_t port);

	otError Stop(void);

	otError Connect(MqttsnConfig &config);

	otError SetConnectCallback(ConnectCallbackFunc callback);

private:
	static void HandleUdpReceive(void *aContext, otMessage *aMessage, const otMessageInfo *aMessageInfo);

	Ip6::UdpSocket mSocket;
	bool mIsConnected;
	MqttsnConfig mConfig;
	ConnectCallbackFunc mConnectCallback;
};

}
}

#endif /* MQTTSN_CLIENT_HPP_ */
