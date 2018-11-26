#include <stdio.h>

#include "common/instance.hpp"
#include "openthread/platform/logging.h"
#include "openthread/platform/uart.h"
#include "openthread/instance.h"
#include "openthread-system.h"
#include "board.h"
#include "smart_socket_platform.h"
#include "smart_socket_config.h"
#include "button_abstraction.h"

#include "mqttsn_client.hpp"

#define GATEWAY_PORT 10000
#define GATEWAY_ADDRESS ""
#define DEFAULT_TOPIC "test"

enum ApplicationState {
	STATE_STARTED,
	STATE_INITIALIZED,
	STATE_JOINER,
	STATE_THREAD_STARTING,
	STATE_THREAD_STARTED,
	STATE_MQTT_CONNECTING,
	STATE_MQTT_CONNECTED,
	STATE_MQTT_RUNNING
};

static ot::Instance* instance = nullptr;
static ApplicationState state = STATE_STARTED;
static ot::Mqttsn::MqttsnClient* client = nullptr;

static void JoinerCallback(otError aError, void *aContext) {
	switch (aError) {
	case OT_ERROR_NONE:
		socket_platform_log(SOCKET_LOG_INFO, "Joiner success.");
		instance->GetThreadNetif().GetMle().Start(true, false);
		instance->GetSettings().SaveThreadAutoStart(true);
		state = STATE_THREAD_STARTED;
		break;
	case OT_ERROR_SECURITY:
		socket_platform_log(SOCKET_LOG_WARN, "Joiner failed: SECURITY.");
		state = STATE_INITIALIZED;
		break;
	case OT_ERROR_NOT_FOUND:
		socket_platform_log(SOCKET_LOG_WARN, "Joiner failed: NOT FOUND.");
		state = STATE_INITIALIZED;
		break;
	default:
		socket_platform_log(SOCKET_LOG_WARN, "Joiner failed: OTHER - %d.", aError);
		state = STATE_INITIALIZED;
		break;
	}
}

static void ShortPressCallback() {
	if (state == STATE_THREAD_STARTED) {
		instance->GetThreadNetif().GetMle().Stop(false);
	}
	state = STATE_JOINER;
	instance->GetThreadNetif().GetJoiner().Start(PSKD, nullptr, PACKAGE_NAME,
			OPENTHREAD_CONFIG_PLATFORM_INFO, PACKAGE_VERSION, nullptr,
			JoinerCallback, nullptr);
}

static void MqttsnConnectCallback(ot::Mqttsn::ReturnCode code, void* context) {
	if (code == ot::Mqttsn::ReturnCode::MQTTSN_CODE_ACCEPTED) {
		socket_platform_log(SOCKET_LOG_INFO, "Successfully connected.");
		state = STATE_MQTT_CONNECTED;
	} else {
		socket_platform_log(SOCKET_LOG_WARN, "Connection failed with code: %d.", code);
		state = STATE_THREAD_STARTED;
	}
}

static void MqttsnConnect() {
	client = new ot::Mqttsn::MqttsnClient(*instance);

	auto config = ot::Mqttsn::MqttsnConfig();
	config.SetClientId("TEST");
	config.SetDuration(3600);
	config.SetCleanSession(true);
	config.SetPort(GATEWAY_PORT);
	auto address = ot::Ip6::Address();
	address.FromString(GATEWAY_ADDRESS);
	config.SetAddress(address);

	client->SetConnectCallback(MqttsnConnectCallback, nullptr);
	client->Connect(config);
	socket_platform_log(SOCKET_LOG_INFO, "Connecting to MQTTSN broker.");
}

static void MqttsnSubscribeCallback(ot::Mqttsn::ReturnCode code, void* context) {
	if (code == ot::Mqttsn::ReturnCode::MQTTSN_CODE_ACCEPTED) {
		socket_platform_log(SOCKET_LOG_INFO, "Successfully subscribed.");
		state = STATE_MQTT_CONNECTED;
	} else {
		socket_platform_log(SOCKET_LOG_WARN, "Subscription failed with code: %d.", code);
		state = STATE_THREAD_STARTED;
	}
}

static void MqttsnSubscribe() {
	client->SetSubscribeCallback(MqttsnSubscribeCallback, nullptr);
	client->Subscribe(DEFAULT_TOPIC);
	socket_platform_log(SOCKET_LOG_INFO, "Subscribing to topic: %s", SOCKET_LOG_INFO);
}

static void ProcessWorker() {
	otDeviceRole role;
	switch (state) {
	case STATE_THREAD_STARTING:
		role = instance->GetThreadNetif().GetMle().GetRole();
		if (role == OT_DEVICE_ROLE_CHILD || role == OT_DEVICE_ROLE_LEADER || role == OT_DEVICE_ROLE_ROUTER) {
			state = STATE_THREAD_STARTED;
		}
		break;
	case STATE_THREAD_STARTED:
		MqttsnConnect();
		state = STATE_MQTT_CONNECTING;
		break;
	case STATE_MQTT_CONNECTED:
		MqttsnSubscribe();
		state = STATE_MQTT_RUNNING;
		break;
	default:
		break;
	}
}

int main(int argc, char *argv[]) {
	otSysInit(argc, argv);
	socket_platform_init();
	button_init();
	BOARD_InitDebugConsole();

    instance = &ot::Instance::InitSingle();
    instance->GetThreadNetif().Up();

    button_short_press_handler(ShortPressCallback);
    state = STATE_INITIALIZED;

    uint8_t autoStart = false;
    instance->GetSettings().ReadThreadAutoStart(autoStart);
    if (autoStart) {
    	state = STATE_THREAD_STARTING;
    }

    while (true) {
    	instance->GetTaskletScheduler().ProcessQueuedTasklets();
    	otSysProcessDrivers(instance);
    	ProcessWorker();
    }
}

void otPlatLog(otLogLevel aLogLevel, otLogRegion aLogRegion, const char *aFormat, ...)
{
    OT_UNUSED_VARIABLE(aLogLevel);
    OT_UNUSED_VARIABLE(aLogRegion);
    OT_UNUSED_VARIABLE(aFormat);

    va_list ap;
    va_start(ap, aFormat);
    vprintf(aFormat, ap);
    va_end(ap);
}

extern "C" void otPlatUartReceived(const uint8_t *aBuf, uint16_t aBufLength) {
    ;
}

extern "C" void otPlatUartSendDone(void) {
    ;
}

uint32_t socket_get_millis() {
	return otPlatAlarmMilliGetNow();
}
