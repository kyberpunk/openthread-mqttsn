#include <stdio.h>

#include "common/instance.hpp"
#include "openthread/platform/logging.h"
#include "openthread/platform/uart.h"
#include "openthread/instance.h"
#include "openthread-system.h"
#include "board.h"

#include "mqttsn_client.hpp"

#define NETWORK_NAME "OpenThreadDemo"
#define PANID 0x1234
#define EXTPANID {0x11, 0x11, 0x11, 0x11, 0x22, 0x22, 0x22, 0x22}
#define DEFAULT_CHANNEL 15
#define MASTER_KEY {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff}

#define GATEWAY_PORT 10000
#define GATEWAY_ADDRESS ""
#define DEFAULT_TOPIC "test"

enum ApplicationState {
	STATE_STARTED,
	STATE_INITIALIZED,
	STATE_THREAD_STARTING,
	STATE_THREAD_STARTED,
	STATE_MQTT_CONNECTING,
	STATE_MQTT_CONNECTED,
	STATE_MQTT_RUNNING
};

static ot::Instance* instance = nullptr;
static ApplicationState state = STATE_STARTED;
static ot::Mqttsn::MqttsnClient* client = nullptr;

static void MqttsnConnectCallback(ot::Mqttsn::ReturnCode code, void* context) {
	if (code == ot::Mqttsn::ReturnCode::MQTTSN_CODE_ACCEPTED) {
		printf("Successfully connected.\r\n");
		state = STATE_MQTT_CONNECTED;
	} else {
		printf("Connection failed with code: %d.\r\n", code);
		state = STATE_THREAD_STARTED;
	}
}

static void MqttsnReceived(const uint8_t* payload, int32_t payloadLength, void* context) {
	printf("Message received:\r\n");
	for (int i = 0; i < payloadLength; i++) {
		printf("%c", static_cast<int8_t>(payload[i]));
	}
	printf("\r\n");
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
	client->SetDataReceivedCallback(MqttsnReceived, nullptr);
	client->Connect(config);
	printf("Connecting to MQTTSN broker.\r\n");
}

static void MqttsnSubscribeCallback(ot::Mqttsn::ReturnCode code, void* context) {
	if (code == ot::Mqttsn::ReturnCode::MQTTSN_CODE_ACCEPTED) {
		printf("Successfully subscribed.\r\n");
		state = STATE_MQTT_RUNNING;
	} else {
		printf("Subscription failed with code: %d.\r\n", code);
		state = STATE_MQTT_CONNECTED;
	}
}

static void MqttsnSubscribe() {
	client->SetSubscribeCallback(MqttsnSubscribeCallback, nullptr);
	client->Subscribe(DEFAULT_TOPIC);
	printf("Subscribing to topic: %s\r\n", DEFAULT_TOPIC);
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
	otError error = OT_ERROR_NONE;
	uint16_t acquisitionId = 0;

	otSysInit(argc, argv);
	BOARD_InitDebugConsole();

    instance = &ot::Instance::InitSingle();
    SuccessOrExit(error = instance->GetThreadNetif().Up());
    state = STATE_INITIALIZED;

    // Set default network settings
    SuccessOrExit(error = instance->GetThreadNetif().GetMac().SetNetworkName(NETWORK_NAME));
    SuccessOrExit(error = instance->GetThreadNetif().GetMac().SetExtendedPanId({EXTPANID}));
    SuccessOrExit(error = instance->GetThreadNetif().GetMac().SetPanId(PANID));
    SuccessOrExit(error = instance->GetThreadNetif().GetMac().AcquireRadioChannel(&acquisitionId));
    SuccessOrExit(error = instance->GetThreadNetif().GetMac().SetRadioChannel(acquisitionId, DEFAULT_CHANNEL));
    SuccessOrExit(error = instance->GetThreadNetif().GetKeyManager().SetMasterKey({MASTER_KEY}));
    instance->GetThreadNetif().GetActiveDataset().Clear();
    instance->GetThreadNetif().GetPendingDataset().Clear();

    SuccessOrExit(error = instance->GetThreadNetif().GetMle().Start(true, false));
    state = STATE_THREAD_STARTING;

    while (true) {
    	instance->GetTaskletScheduler().ProcessQueuedTasklets();
    	otSysProcessDrivers(instance);
    	ProcessWorker();
    }
    return 0;

exit:
	printf("Initialization failed with error: %d\r\n", error);
	return 1;
}

extern "C" void otPlatLog(otLogLevel aLogLevel, otLogRegion aLogRegion, const char *aFormat, ...)
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
