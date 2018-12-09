#include <stdio.h>

#include "common/instance.hpp"
#include "openthread/platform/logging.h"
#include "openthread/platform/uart.h"
#include "openthread/instance.h"
#include "openthread-system.h"
#include "openthread/platform/alarm-milli.h"
#include "utils/slaac_address.hpp"

#include "board.h"
#include "fsl_debug_console.h"

// TODO: Implement log output with OT platform implementation
// TODO: Implement connect retry

#include "mqttsn_client.hpp"

#define NETWORK_NAME "OTBR4444"
#define PANID 0x4444
#define EXTPANID {0x33, 0x33, 0x33, 0x33, 0x44, 0x44, 0x44, 0x44}
#define DEFAULT_CHANNEL 15
#define MASTER_KEY {0x33, 0x33, 0x44, 0x44, 0x33, 0x33, 0x44, 0x44, 0x33, 0x33, 0x44, 0x44, 0x33, 0x33, 0x44, 0x44}

#define GATEWAY_PORT 10000
//#define GATEWAY_ADDRESS "2018:ff9b::c0a8:2d"
#define GATEWAY_ADDRESS "2018:ff9b::ac12:8"
#define DEFAULT_TOPIC "test"
#define CONNECTION_TIMEOUT 3000

#define CLIENT_ID "THREAD"
#define CLIENT_PORT 11111

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
static uint32_t connectionTimeoutTime = 0;
static otNetifAddress slaacAddresses[OPENTHREAD_CONFIG_NUM_SLAAC_ADDRESSES];

static void MqttsnConnectedCallback(ot::Mqttsn::ReturnCode code, void* context) {
	if (code == ot::Mqttsn::ReturnCode::MQTTSN_CODE_ACCEPTED) {
		PRINTF("Successfully connected.\r\n");
		state = STATE_MQTT_CONNECTED;
	} else {
		PRINTF("Connection failed with code: %d.\r\n", code);
		state = STATE_THREAD_STARTED;
	}
}

static void MqttsnDisconnectedCallback(void* context) {
	PRINTF("Client disconnected.\r\n");
	state = STATE_THREAD_STARTED;
}

static void MqttsnReceived(const uint8_t* payload, int32_t payloadLength, ot::Mqttsn::Qos qos, ot::Mqttsn::TopicId topicId, void* context) {
	PRINTF("Message received from topic %d with QoS %d:\r\n", topicId, qos);
	for (int i = 0; i < payloadLength; i++) {
		PRINTF("%c", static_cast<int8_t>(payload[i]));
	}
	PRINTF("\r\n");
}

static void MqttsnConnect(ot::Instance &instance) {
	auto config = ot::Mqttsn::MqttsnConfig();
	config.SetClientId(CLIENT_ID);
	config.SetDuration(3600);
	config.SetCleanSession(true);
	config.SetPort(GATEWAY_PORT);
	auto address = ot::Ip6::Address();
	address.FromString(GATEWAY_ADDRESS);
	config.SetAddress(address);
	client->SetConnectedCallback(MqttsnConnectedCallback, nullptr);
	client->SetDisconnectedCallback(MqttsnDisconnectedCallback, nullptr);
	client->SetPublishReceivedCallback(MqttsnReceived, nullptr);

	otError error = OT_ERROR_NONE;
	if ((error = client->Connect(config)) == OT_ERROR_NONE) {
		PRINTF("Connecting to MQTTSN broker.\r\n");
	} else {
		PRINTF("Connection failed with error: %d.\r\n", error);
	}
}

static void MqttsnSubscribeCallback(ot::Mqttsn::ReturnCode code, ot::Mqttsn::TopicId topicId ,void* context) {
	if (code == ot::Mqttsn::ReturnCode::MQTTSN_CODE_ACCEPTED) {
		PRINTF("Successfully subscribed to topic: %d.\r\n", topicId);
		state = STATE_MQTT_RUNNING;

		std::string data = "hello";
		client->Publish(reinterpret_cast<const uint8_t *>(data.c_str()), data.length(), ot::Mqttsn::Qos::MQTTSN_QOS0, topicId);
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
			PRINTF("Thread started. Role: %d.\r\n", role);
			state = STATE_THREAD_STARTED;
		}
		break;
	case STATE_THREAD_STARTED:
		MqttsnConnect(instance);
		connectionTimeoutTime = otPlatAlarmMilliGetNow() + CONNECTION_TIMEOUT;
		state = STATE_MQTT_CONNECTING;
		break;
	case STATE_MQTT_CONNECTING:
		if (connectionTimeoutTime < otPlatAlarmMilliGetNow()) {
			role = instance.GetThreadNetif().GetMle().GetRole();
			PRINTF("Connection timeout. Role: %d\r\n", role);
			auto address = instance.GetThreadNetif().GetUnicastAddresses();
			while (address != nullptr) {
				PRINTF("%s\r\n", address->GetAddress().ToString().AsCString());
				address = address->GetNext();
			}
			state = STATE_THREAD_STARTED;
		}
		break;
	case STATE_MQTT_CONNECTED:
		MqttsnSubscribe();
		state = STATE_MQTT_RUNNING;
		break;
	default:
		break;
	}
}

void HandleNetifStateChanged(otChangedFlags aFlags, void *aContext) {
	ot::Instance &instance = *static_cast<ot::Instance *>(aContext);
	VerifyOrExit((aFlags & OT_CHANGED_THREAD_NETDATA) != 0);

	ot::Utils::Slaac::UpdateAddresses(&instance, slaacAddresses, sizeof(slaacAddresses), ot::Utils::Slaac::CreateRandomIid, nullptr);

exit:
	return;
}

int main(int argc, char *argv[]) {
	otError error = OT_ERROR_NONE;
	uint16_t acquisitionId = 0;

	memset(slaacAddresses, 0, sizeof(slaacAddresses));
	otSysInit(argc, argv);
	BOARD_InitDebugConsole();

	ot::Instance &instance = ot::Instance::InitSingle();
	instance.GetNotifier().RegisterCallback(HandleNetifStateChanged, &instance);
    state = STATE_INITIALIZED;

    // Set default network settings
    ot::ThreadNetif &netif = instance.GetThreadNetif();
    SuccessOrExit(error = netif.GetMac().SetNetworkName(NETWORK_NAME));
    SuccessOrExit(error = netif.GetMac().SetExtendedPanId({EXTPANID}));
    SuccessOrExit(error = netif.GetMac().SetPanId(PANID));
    SuccessOrExit(error = netif.GetMac().AcquireRadioChannel(&acquisitionId));
    SuccessOrExit(error = netif.GetMac().SetRadioChannel(acquisitionId, DEFAULT_CHANNEL));
    SuccessOrExit(error = netif.GetKeyManager().SetMasterKey({MASTER_KEY}));
    netif.GetActiveDataset().Clear();
    netif.GetPendingDataset().Clear();

    SuccessOrExit(error = netif.Up());
    SuccessOrExit(error = netif.GetMle().Start(true, false));

    client = new ot::Mqttsn::MqttsnClient(instance);
    SuccessOrExit(error = client->Start(CLIENT_PORT));
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
    VPRINTF(aFormat, ap);
    va_end(ap);
}

extern "C" void otPlatUartReceived(const uint8_t *aBuf, uint16_t aBufLength) {
    ;
}

extern "C" void otPlatUartSendDone(void) {
    ;
}
