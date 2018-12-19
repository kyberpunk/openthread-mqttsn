/*
 *  Copyright (c) 2018, Vit Holasek
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

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

#define GATEWAY_SEARCH 1
#define GATEWAY_MULTICAST_PORT 10000
#define GATEWAY_MULTICAST_ADDRESS "ff03::2"
#define GATEWAY_MULTICAST_RADIUS 8

#define DEFAULT_TOPIC "topic"
#define SEND_TIMEOUT 3000

#define CLIENT_ID "THREAD"
#define CLIENT_PORT 10000

enum ApplicationState
{
    kStarted,
    kInitialized,
    kThreadStarting,
    kThreadStarted,
    kMqttSearchGw,
    kMqttConnecting,
    kMqttConnected,
    kMqttRunning
};

static ApplicationState sState = kStarted;
static ot::Mqttsn::MqttsnClient* sClient = nullptr;
static uint32_t sConnectionTimeoutTime = 0;
static otNetifAddress sSlaacAddresses[OPENTHREAD_CONFIG_NUM_SLAAC_ADDRESSES];
#if GATEWAY_SEARCH
static ot::Ip6::Address sGatewayAddress;
static uint32_t sSearchGwTimeoutTime = 0;
#endif

static void MqttsnConnectedCallback(ot::Mqttsn::ReturnCode aCode, void* aContext)
{
    OT_UNUSED_VARIABLE(aContext);

    if (aCode == ot::Mqttsn::kCodeAccepted)
    {
        PRINTF("Successfully connected.\r\n");
        sState = kMqttConnected;
    }
    else
    {
        PRINTF("Connection failed with code: %d.\r\n", aCode);
        sState = kThreadStarted;
    }
}

static void MqttsnDisconnectedCallback(ot::Mqttsn::DisconnectType aType, void* aContext)
{
    OT_UNUSED_VARIABLE(aContext);

    PRINTF("Client disconnected. Reason: %d.\r\n", aType);
    sState = kThreadStarted;
}

static ot::Mqttsn::ReturnCode MqttsnReceived(const uint8_t* aPayload, int32_t aPayloadLength, ot::Mqttsn::TopicId topicId, void* aContext)
{
    OT_UNUSED_VARIABLE(aContext);

    PRINTF("Message received from topic %d.\r\n", topicId);
    for (int i = 0; i < aPayloadLength; i++)
    {
        PRINTF("%c", static_cast<int8_t>(aPayload[i]));
    }
    PRINTF("\r\n");
    return ot::Mqttsn::kCodeAccepted;
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

static void MqttsnPublished(ot::Mqttsn::ReturnCode aCode, ot::Mqttsn::TopicId aTopicId, void* aContext)
{
    OT_UNUSED_VARIABLE(aContext);

    if (aCode == ot::Mqttsn::kCodeAccepted)
    {
        PRINTF("Successfully published to topic: %d.\r\n", aTopicId);
    }
    else
    {
        PRINTF("Publish failed with code: %d.\r\n", aCode);
    }
}

static void MqttsnSubscribeCallback(ot::Mqttsn::ReturnCode aCode, ot::Mqttsn::TopicId aTopicId, void* aContext)
{
    OT_UNUSED_VARIABLE(aContext);

    if (aCode == ot::Mqttsn::kCodeAccepted)
    {
        PRINTF("Successfully subscribed to topic: %d.\r\n", aTopicId);
        sState = kMqttRunning;

        // Test Qos 1 message
        char text[] = "hello";
        otError error = sClient->Publish(reinterpret_cast<unsigned char*>(text), sizeof(text), ot::Mqttsn::kQos1, aTopicId, MqttsnPublished, nullptr);
        if (error != OT_ERROR_NONE)
        {
            PRINTF("Publish failed with error: %d.\r\n", error);
        }
    }
    else
    {
        PRINTF("Subscription failed with code: %d.\r\n", aCode);
    }
}

static void MqttsnSubscribe()
{
    sClient->Subscribe(DEFAULT_TOPIC, ot::Mqttsn::Qos::kQos1, MqttsnSubscribeCallback, nullptr);
    PRINTF("Subscribing to topic: %s\r\n", DEFAULT_TOPIC);
}

#if GATEWAY_SEARCH
static void SearchGatewayCallback(const ot::Ip6::Address &aAddress, uint8_t aGatewayId, void* aContext)
{
    OT_UNUSED_VARIABLE(aContext);

    PRINTF("SearchGw found gateway with id: %u, %s\r\n", aGatewayId, aAddress.ToString().AsCString());
    sGatewayAddress = aAddress;
    MqttsnConnect(sGatewayAddress, GATEWAY_MULTICAST_PORT);
    sState = kMqttConnecting;
}

static void AdvertiseCallback(const ot::Ip6::Address &aAddress, uint8_t aGatewayId, uint32_t aDuration, void* aContext)
{
    OT_UNUSED_VARIABLE(aContext);

    PRINTF("Received gateway advertise with id: %u, %s\r\n", aGatewayId, aAddress.ToString().AsCString());
    sGatewayAddress = aAddress;
    MqttsnConnect(sGatewayAddress, GATEWAY_MULTICAST_PORT);
    sState = kMqttConnecting;
}

static void SearchGateway(const char* aMulticastAddress, uint16_t aPort)
{
    otError error = OT_ERROR_NONE;
    ot::Ip6::Address address;
    address.FromString(aMulticastAddress);
    if ((error = sClient->SearchGateway(address, aPort, GATEWAY_MULTICAST_RADIUS)) == OT_ERROR_NONE)
    {
        sSearchGwTimeoutTime = ot::TimerMilli::GetNow() + SEND_TIMEOUT;
        PRINTF("Searching gateway.\r\n");
    }
    else
    {
        PRINTF("Search gateway failed with error: %d.\r\n", error);
    }
    sState = kMqttSearchGw;
}
#endif

static void ProcessWorker(ot::Instance &aInstance)
{
    otDeviceRole role;
    switch (sState)
    {
    case kThreadStarting:
        role = aInstance.GetThreadNetif().GetMle().GetRole();
        if (role == OT_DEVICE_ROLE_CHILD || role == OT_DEVICE_ROLE_LEADER || role == OT_DEVICE_ROLE_ROUTER)
        {
            PRINTF("Thread started. Role: %d.\r\n", role);
            sState = kThreadStarted;
        }
        break;
    case kMqttConnecting:
        if (sConnectionTimeoutTime != 0 && sConnectionTimeoutTime < ot::TimerMilli::GetNow())
        {
            role = aInstance.GetThreadNetif().GetMle().GetRole();
            PRINTF("Connection timeout. Role: %d\r\n", role);
            sState = kThreadStarted;
        }
        break;
    case kMqttConnected:
        MqttsnSubscribe();
        sState = kMqttRunning;
        break;
    case kThreadStarted:
#if GATEWAY_SEARCH
        SearchGateway(GATEWAY_MULTICAST_ADDRESS, GATEWAY_MULTICAST_PORT);
#else
        ot::Ip6::Address address;
        address.FromString(GATEWAY_ADDRESS);
        MqttsnConnect(address, GATEWAY_PORT);
        sConnectionTimeoutTime = ot::TimerMilli::GetNow() + SEND_TIMEOUT;
        sState = kMqttConnecting;
#endif
        break;
#if GATEWAY_SEARCH
    case kMqttSearchGw:
        if (sSearchGwTimeoutTime != 0 && sSearchGwTimeoutTime < ot::TimerMilli::GetNow())
        {
            role = aInstance.GetThreadNetif().GetMle().GetRole();
            PRINTF("Connection timeout. Role: %d\r\n", role);
            sState = kThreadStarted;
        }
        break;
#endif
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
    sState = kInitialized;

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
    sState = kThreadStarting;
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
