#include <stdio.h>

#include "common/instance.hpp"
#include "openthread/platform/logging.h"
#include "openthread/platform/uart.h"
#include "openthread/instance.h"
#include "openthread-system.h"
#include "utils/slaac_address.hpp"

#include "board.h"
#include "fsl_debug_console.h"

// TODO: Implement log output with OT platform implementation

#include "mqttsn_client.hpp"

#define NETWORK_NAME "OTBR4444"
#define PANID 0x4444
#define EXTPANID {0x33, 0x33, 0x33, 0x33, 0x44, 0x44, 0x44, 0x44}
#define DEFAULT_CHANNEL 15
#define MASTER_KEY {0x33, 0x33, 0x44, 0x44, 0x33, 0x33, 0x44, 0x44, 0x33, 0x33, 0x44, 0x44, 0x33, 0x33, 0x44, 0x44}

#define GATEWAY_PORT 10000
#define GATEWAY_ADDRESS "2018:ff9b::ac12:8"

#define GATEWAY_SEARCH 0
#define GATEWAY_MULTICAST_PORT 1883
#define GATEWAY_MULTICAST_ADDRESS "2018:ff9b::e101:101"
#define GATEWAY_MULTICAST_RADIUS 4

#define DEFAULT_TOPIC "test"
#define SEND_TIMEOUT 3000

#define CLIENT_ID "THREAD"
#define CLIENT_PORT 11111

enum ApplicationState
{
    STATE_STARTED,
    STATE_INITIALIZED,
    STATE_THREAD_STARTING,
    STATE_THREAD_STARTED,
    STATE_MQTT_SEARCHGW,
    STATE_MQTT_CONNECTING,
    STATE_MQTT_CONNECTED,
    STATE_MQTT_RUNNING
};

static ApplicationState sState = STATE_STARTED;
static ot::Mqttsn::MqttsnClient* sClient = nullptr;
static uint32_t sConnectionTimeoutTime = 0;
static otNetifAddress sSlaacAddresses[OPENTHREAD_CONFIG_NUM_SLAAC_ADDRESSES];
#if GATEWAY_SEARCH
static ot::Ip6::Address sGatewayAddress;
static uint16_t sGatewayPort;
static uint32_t sSearchGwTimeoutTime = 0;
#endif

static void MqttsnConnectedCallback(ot::Mqttsn::ReturnCode aCode, void* aContext)
{
    if (aCode == ot::Mqttsn::ReturnCode::MQTTSN_CODE_ACCEPTED)
    {
        PRINTF("Successfully connected.\r\n");
        sState = STATE_MQTT_CONNECTED;
    }
    else
    {
        PRINTF("Connection failed with code: %d.\r\n", aCode);
        sState = STATE_THREAD_STARTED;
    }
}

static void MqttsnDisconnectedCallback(ot::Mqttsn::DisconnectType aType, void* aContext)
{
    PRINTF("Client disconnected. Reason: %d.\r\n", aType);
    sState = STATE_THREAD_STARTED;
}

static void MqttsnReceived(const uint8_t* aPayload, int32_t aPayloadLength, ot::Mqttsn::Qos aQos, ot::Mqttsn::TopicId topicId, void* aContext)
{
    PRINTF("Message received from topic %d with QoS %d:\r\n", topicId, aQos);
    for (int i = 0; i < aPayloadLength; i++)
    {
        PRINTF("%c", static_cast<int8_t>(aPayload[i]));
    }
    PRINTF("\r\n");
}

static void MqttsnConnect(const ot::Ip6::Address &aAddress, uint16_t aPort)
{
    auto config = ot::Mqttsn::MqttsnConfig();
    config.SetClientId(CLIENT_ID);
    config.SetKeepAlive(30);
    config.SetCleanSession(true);
    config.SetPort(aPort);
    config.SetAddress(aAddress);
    sClient->SetConnectedCallback(MqttsnConnectedCallback, nullptr);
    sClient->SetDisconnectedCallback(MqttsnDisconnectedCallback, nullptr);
    sClient->SetPublishReceivedCallback(MqttsnReceived, nullptr);

    otError error = OT_ERROR_NONE;
    if ((error = sClient->Connect(config)) == OT_ERROR_NONE)
    {
        PRINTF("Connecting to MQTTSN broker.\r\n");
    }
    else
    {
        PRINTF("Connection failed with error: %d.\r\n", error);
    }
}

static void MqttsnSubscribeCallback(ot::Mqttsn::ReturnCode aCode, ot::Mqttsn::TopicId aTopicId, void* aContext)
{
    if (aCode == ot::Mqttsn::ReturnCode::MQTTSN_CODE_ACCEPTED)
    {
        PRINTF("Successfully subscribed to topic: %d.\r\n", aTopicId);
        sState = STATE_MQTT_RUNNING;

        std::string data = "hello";
        sClient->Publish(reinterpret_cast<const uint8_t *>(data.c_str()), data.length(), ot::Mqttsn::Qos::MQTTSN_QOS0, aTopicId);
    }
    else
    {
        PRINTF("Subscription failed with code: %d.\r\n", aCode);
        sState = STATE_MQTT_CONNECTED;
    }
}

static void MqttsnSubscribe()
{
    sClient->SetSubscribeCallback(MqttsnSubscribeCallback, nullptr);
    sClient->Subscribe(DEFAULT_TOPIC, ot::Mqttsn::Qos::MQTTSN_QOS0);
    PRINTF("Subscribing to topic: %s\r\n", DEFAULT_TOPIC);
}

#if GATEWAY_SEARCH
static void SearchGatewayCallback(const ot::Ip6::Address &aAddress, uint16_t aPort, uint8_t aGatewayId, void* aContext)
{
    PRINTF("SearchGw found gateway with id: %u, %s:%u\r\n", aGatewayId, aAddress.ToString().AsCString(), aPort);
    sGatewayAddress = aAddress;
    sGatewayPort = aPort;
    MqttsnConnect(sGatewayAddress, sGatewayPort);
    sState = STATE_MQTT_CONNECTING;
}

static void AdvertiseCallback(const ot::Ip6::Address &aAddress, uint16_t aPort, uint8_t aGatewayId, uint32_t aDuration, void* aContext)
{
    PRINTF("Received gateway advertise with id: %u, %s:%u\r\n", aGatewayId, aAddress.ToString().AsCString(), aPort);
    sGatewayAddress = aAddress;
    sGatewayPort = aPort;
    MqttsnConnect(sGatewayAddress, sGatewayPort);
    sState = STATE_MQTT_CONNECTING;
}

static void SearchGateway(const std::string &aMulticastAddress, uint16_t aPort)
{
    otError error = OT_ERROR_NONE;
    ot::Ip6::Address address;
    address.FromString(aMulticastAddress.c_str());
    if ((error = sClient->SearchGateway(address, aPort, GATEWAY_MULTICAST_RADIUS)) == OT_ERROR_NONE)
    {
        sSearchGwTimeoutTime = ot::TimerMilli::GetNow() + SEND_TIMEOUT;
        PRINTF("Searching gateway.\r\n");
    }
    else
    {
        PRINTF("Search gateway failed with error: %d.\r\n", error);
    }
    sState = STATE_MQTT_SEARCHGW;
}
#endif

static void ProcessWorker(ot::Instance &aInstance)
{
    otDeviceRole role;
    switch (sState)
    {
    case STATE_THREAD_STARTING:
        role = aInstance.GetThreadNetif().GetMle().GetRole();
        if (role == OT_DEVICE_ROLE_CHILD || role == OT_DEVICE_ROLE_LEADER || role == OT_DEVICE_ROLE_ROUTER)
        {
            PRINTF("Thread started. Role: %d.\r\n", role);
            sState = STATE_THREAD_STARTED;
        }
        break;
    case STATE_THREAD_STARTED:
        #if GATEWAY_SEARCH
        SearchGateway(GATEWAY_MULTICAST_ADDRESS, GATEWAY_MULTICAST_PORT);
#else
        ot::Ip6::Address address;
        address.FromString(GATEWAY_ADDRESS);
        MqttsnConnect(address, GATEWAY_PORT);
        sConnectionTimeoutTime = ot::TimerMilli::GetNow() + SEND_TIMEOUT;
        sState = STATE_MQTT_CONNECTING;
#endif
        break;
#if GATEWAY_SEARCH
    case STATE_MQTT_SEARCHGW:
        if (sConnectionTimeoutTime != 0 && sConnectionTimeoutTime < ot::TimerMilli::GetNow())
        {
            role = aInstance.GetThreadNetif().GetMle().GetRole();
            PRINTF("Connection timeout. Role: %d\r\n", role);
            sState = STATE_THREAD_STARTED;
        }
        break;
#endif
    case STATE_MQTT_CONNECTING:
        if (sConnectionTimeoutTime != 0 && sConnectionTimeoutTime < ot::TimerMilli::GetNow())
        {
            role = aInstance.GetThreadNetif().GetMle().GetRole();
            PRINTF("Connection timeout. Role: %d\r\n", role);
            sState = STATE_THREAD_STARTED;
        }
        break;
    case STATE_MQTT_CONNECTED:
        MqttsnSubscribe();
        sState = STATE_MQTT_RUNNING;
        break;
    default:
        break;
    }
}

void HandleNetifStateChanged(otChangedFlags aFlags, void *aContext)
{
    ot::Instance &instance = *static_cast<ot::Instance *>(aContext);
    VerifyOrExit((aFlags & OT_CHANGED_THREAD_NETDATA) != 0);

    ot::Utils::Slaac::UpdateAddresses(&instance, sSlaacAddresses, sizeof(sSlaacAddresses), ot::Utils::Slaac::CreateRandomIid, nullptr);

exit:
    return;
}

int main(int aArgc, char *aArgv[])
{
    otError error = OT_ERROR_NONE;
    uint16_t acquisitionId = 0;

    memset(sSlaacAddresses, 0, sizeof(sSlaacAddresses));
    otSysInit(aArgc, aArgv);
    BOARD_InitDebugConsole();

    ot::Instance &instance = ot::Instance::InitSingle();
    ot::Mqttsn::MqttsnClient client = ot::Mqttsn::MqttsnClient(instance);
    sClient = &client;
    instance.GetNotifier().RegisterCallback(HandleNetifStateChanged, &instance);
    sState = STATE_INITIALIZED;

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

    SuccessOrExit(error = sClient->Start(CLIENT_PORT));
#if GATEWAY_SEARCH
    SuccessOrExit(error = sClient->SetSearchGwCallback(SearchGatewayCallback, NULL));
    SuccessOrExit(error = sClient->SetAdvertiseCallback(AdvertiseCallback, NULL));
#endif
    sState = STATE_THREAD_STARTING;
    PRINTF("Thread starting.\r\n");

    while (true)
    {
        instance.GetTaskletScheduler().ProcessQueuedTasklets();
        otSysProcessDrivers(&instance);
        ProcessWorker(instance);
        SuccessOrExit(error = sClient->Process());
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

extern "C" void otPlatUartReceived(const uint8_t *aBuf, uint16_t aBufLength)
{
    ;
}

extern "C" void otPlatUartSendDone(void)
{
    ;
}
