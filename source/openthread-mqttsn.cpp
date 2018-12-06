#include <stdio.h>

#include "common/instance.hpp"
#include "openthread/platform/logging.h"
#include "openthread/platform/uart.h"
#include "openthread/instance.h"
#include "openthread-system.h"
#include "board.h"
#include "fsl_debug_console.h"

// TODO: Implement log output with OT platform implementation

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

static ApplicationState state = STATE_STARTED;
static ot::Mqttsn::MqttsnClient* client = nullptr;

static void MqttsnConnectCallback(ot::Mqttsn::ReturnCode code, void* context) {
	if (code == ot::Mqttsn::ReturnCode::MQTTSN_CODE_ACCEPTED) {
		PRINTF("Successfully connected.\r\n");
		state = STATE_MQTT_CONNECTED;
	} else {
		PRINTF("Connection failed with code: %d.\r\n", code);
		state = STATE_THREAD_STARTED;
	}
}

static void MqttsnReceived(const uint8_t* payload, int32_t payloadLength, ot::Mqttsn::Qos qos, ot::Mqttsn::TopicId topicId, void* context) {
	PRINTF("Message received from topic %d with QoS %d:\r\n", topicId, qos);
	for (int i = 0; i < payloadLength; i++) {
		PRINTF("%c", static_cast<int8_t>(payload[i]));
	}
	PRINTF("\r\n");
}

static void MqttsnConnect(ot::Instance &instance) {
	client = new ot::Mqttsn::MqttsnClient(instance);
	auto config = ot::Mqttsn::MqttsnConfig();
	config.SetClientId("TEST");
	config.SetDuration(3600);
	config.SetCleanSession(true);
	config.SetPort(GATEWAY_PORT);
	auto address = ot::Ip6::Address();
	address.FromString(GATEWAY_ADDRESS);
	config.SetAddress(address);

	client->SetConnectCallback(MqttsnConnectCallback, nullptr);
	client->SetPublishReceivedCallback(MqttsnReceived, nullptr);
	client->Connect(config);
	PRINTF("Connecting to MQTTSN broker.\r\n");
}

static void MqttsnSubscribeCallback(ot::Mqttsn::ReturnCode code, ot::Mqttsn::TopicId topicId ,void* context) {
	if (code == ot::Mqttsn::ReturnCode::MQTTSN_CODE_ACCEPTED) {
		PRINTF("Successfully subscribed to topic: %d.\r\n", topicId);
		state = STATE_MQTT_RUNNING;
	} else {
		PRINTF("Subscription failed with code: %d.\r\n", code);
		state = STATE_MQTT_CONNECTED;
	}
}

static void MqttsnSubscribe() {
	client->SetSubscribeCallback(MqttsnSubscribeCallback, nullptr);
	client->Subscribe(DEFAULT_TOPIC, ot::Mqttsn::Qos::MQTTSN_QOS0);
	PRINTF("Subscribing to topic: %s\r\n", DEFAULT_TOPIC);
}

static void ProcessWorker(ot::Instance &instance) {
	otDeviceRole role;
	switch (state) {
	case STATE_THREAD_STARTING:
		role = instance.GetThreadNetif().GetMle().GetRole();
		if (role == OT_DEVICE_ROLE_CHILD || role == OT_DEVICE_ROLE_LEADER || role == OT_DEVICE_ROLE_ROUTER) {
			PRINTF("Thread started.\r\n");
			state = STATE_THREAD_STARTED;
		}
		break;
	case STATE_THREAD_STARTED:
		MqttsnConnect(instance);
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

	ot::Instance &instance = ot::Instance::InitSingle();
	ot::ThreadNetif &netif = instance.GetThreadNetif();
    SuccessOrExit(error = netif.Up());
    state = STATE_INITIALIZED;

    // Set default network settings
    SuccessOrExit(error = netif.GetMac().SetNetworkName(NETWORK_NAME));
    SuccessOrExit(error = netif.GetMac().SetExtendedPanId({EXTPANID}));
    SuccessOrExit(error = netif.GetMac().SetPanId(PANID));
    SuccessOrExit(error = netif.GetMac().AcquireRadioChannel(&acquisitionId));
    SuccessOrExit(error = netif.GetMac().SetRadioChannel(acquisitionId, DEFAULT_CHANNEL));
    SuccessOrExit(error = netif.GetKeyManager().SetMasterKey({MASTER_KEY}));
    netif.GetActiveDataset().Clear();
    netif.GetPendingDataset().Clear();

    SuccessOrExit(error = netif.GetMle().Start(true, false));
    state = STATE_THREAD_STARTING;
    PRINTF("Thread starting.\r\n");

    while (true) {
    	instance.GetTaskletScheduler().ProcessQueuedTasklets();
    	otSysProcessDrivers(&instance);
    	ProcessWorker(instance);
    }
    return 0;

exit:
	PRINTF("Initialization failed with error: %d\r\n", error);
	return 1;
}

extern "C" void otPlatLog(otLogLevel aLogLevel, otLogRegion aLogRegion, const char *aFormat, ...)
{
    OT_UNUSED_VARIABLE(aLogLevel);
    OT_UNUSED_VARIABLE(aLogRegion);
    OT_UNUSED_VARIABLE(aFormat);

    va_list ap;
    va_start(ap, aFormat);
    PRINTF(aFormat, ap);
    va_end(ap);
}

extern "C" void otPlatUartReceived(const uint8_t *aBuf, uint16_t aBufLength) {
    ;
}

extern "C" void otPlatUartSendDone(void) {
    ;
}
