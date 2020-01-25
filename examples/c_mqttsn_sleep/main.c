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

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "openthread/instance.h"
#include "openthread/thread.h"
#include "openthread/tasklet.h"
#include "openthread/ip6.h"
#include "openthread/mqttsn.h"
#include "openthread/dataset.h"
#include "openthread/link.h"
#include "openthread/platform/alarm-milli.h"

#define NETWORK_NAME "OTBR4444"
#define PANID 0x4444
#define EXTPANID {0x33, 0x33, 0x33, 0x33, 0x44, 0x44, 0x44, 0x44}
#define DEFAULT_CHANNEL 15
#define MASTER_KEY {0x33, 0x33, 0x44, 0x44, 0x33, 0x33, 0x44, 0x44, 0x33, 0x33, 0x44, 0x44, 0x33, 0x33, 0x44, 0x44}

#define GATEWAY_PORT 10000
#define GATEWAY_ADDRESS "2018:ff9b::ac12:8"

#define CLIENT_ID "THREAD"
#define CLIENT_PORT 10000

// Duration in ms how long will MQTT-SN client stay in sleep mode
#define SLEEP_DURATION_MS 60000
// Maximal awake time
#define AWAKE_TIMEOUT_MS 2000

#define TOPIC_NAME "sensors"

static const uint8_t sExpanId[] = EXTPANID;
static const uint8_t sMasterKey[] = MASTER_KEY;

static uint32_t sNextAwakeAt = 0xffffffff;

static otMqttsnReturnCode HandlePublishReceived(const uint8_t* aPayload, int32_t aPayloadLength, otMqttsnTopicIdType aTopicIdType, otMqttsnTopicId aTopicId, const char* aShortTopicName, void* aContext)
{
    OT_UNUSED_VARIABLE(aPayload);
    OT_UNUSED_VARIABLE(aPayloadLength);
    OT_UNUSED_VARIABLE(aTopicIdType);
    OT_UNUSED_VARIABLE(aTopicId);
    OT_UNUSED_VARIABLE(aShortTopicName);
    OT_UNUSED_VARIABLE(aContext);
    // Handle received message from subscribed topic when awaken from sleep

    return kCodeAccepted;
}

static void HandleDisconnected(otMqttsnDisconnectType aType, void* aContext)
{
    OT_UNUSED_VARIABLE(aType);
    OT_UNUSED_VARIABLE(aContext);
    // Handle disconnect

    if (aType == kDisconnectAsleep && sNextAwakeAt == 0xffffffff)
    {
        // Handle asleep event
        sNextAwakeAt = otPlatAlarmMilliGetNow() + SLEEP_DURATION_MS;
    }
}

static void HandleSubscribed(otMqttsnReturnCode aCode, otMqttsnTopicId aTopicId, otMqttsnQos aQos, void* aContext)
{
    OT_UNUSED_VARIABLE(aCode);
    OT_UNUSED_VARIABLE(aTopicId);
    OT_UNUSED_VARIABLE(aQos);
    // Handle subscribed event

    otInstance *instance = (otInstance *)aContext;
    // Go to sleep and schedule awake timer
    // Add some time because of crystal precision deviation
    otMqttsnSleep(instance, SLEEP_DURATION_MS + 5000);
}

static void HandleConnected(otMqttsnReturnCode aCode, void* aContext)
{
    // Handle connected
    otInstance *instance = (otInstance *)aContext;
    if (aCode == kCodeAccepted)
    {
        // Set callback for received messages
        otMqttsnSetPublishReceivedHandler(instance, HandlePublishReceived, instance);
        // Obtain target topic ID
        otMqttsnSubscribe(instance, TOPIC_NAME, kQos1, HandleSubscribed, instance);
    }
}

static void MqttsnConnect(otInstance *instance)
{
    otIp6Address address;
    otIp6AddressFromString(GATEWAY_ADDRESS, &address);

    // Set MQTT-SN client configuration settings
    otMqttsnConfig config;
    config.mClientId = CLIENT_ID;
    config.mKeepAlive = 30;
    config.mCleanSession = true;
    config.mPort = GATEWAY_PORT;
    config.mAddress = &address;
    config.mRetransmissionCount = 3;
    config.mRetransmissionTimeout = 10;

    // Register connected callback
    otMqttsnSetConnectedHandler(instance, HandleConnected, (void *)instance);
    // Register disconnected callback
    otMqttsnSetDisconnectedHandler(instance, HandleDisconnected, instance);
    // Connect to the MQTT broker (gateway)
    otMqttsnConnect(instance, &config);
}

static void StateChanged(otChangedFlags aFlags, void *aContext)
{
    otInstance *instance = (otInstance *)aContext;
    // when thread role changed
    if (aFlags & OT_CHANGED_THREAD_ROLE)
    {
        otDeviceRole role = otThreadGetDeviceRole(instance);
        // If role changed to any of active roles and MQTT-SN client is not connected then connect
        if ((role == OT_DEVICE_ROLE_CHILD || role == OT_DEVICE_ROLE_ROUTER)
            && otMqttsnGetState(instance) == kStateDisconnected)
        {
            MqttsnConnect(instance);
        }
    }
}

int main(int aArgc, char *aArgv[])
{
    otError error = OT_ERROR_NONE;
    otExtendedPanId extendedPanid;
    otMasterKey masterKey;
    otInstance *instance;

    otSysInit(aArgc, aArgv);
    instance = otInstanceInitSingle();

    // Set default network settings
    // Set network name
    error = otThreadSetNetworkName(instance, NETWORK_NAME);
    // Set extended PANID
    memcpy(extendedPanid.m8, sExpanId, sizeof(sExpanId));
    error = otThreadSetExtendedPanId(instance, &extendedPanid);
    // Set PANID
    error = otLinkSetPanId(instance, PANID);
    // Set channel
    error = otLinkSetChannel(instance, DEFAULT_CHANNEL);
    // Set masterkey
    memcpy(masterKey.m8, sMasterKey, sizeof(sMasterKey));
    error = otThreadSetMasterKey(instance, &masterKey);

    // Register notifier callback to receive thread role changed events
    error = otSetStateChangedCallback(instance, StateChanged, instance);

    // Start thread network
    otIp6SetSlaacEnabled(instance, true);
    error = otIp6SetEnabled(instance, true);
    error = otThreadSetEnabled(instance, true);

    // Start MQTT-SN client
    error = otMqttsnStart(instance, CLIENT_PORT);

    while (true)
    {
        otTaskletsProcess(instance);
        otSysProcessDrivers(instance);
        // Awake when scheduled time passed
        if (otPlatAlarmMilliGetNow() > sNextAwakeAt)
        {
            otMqttsnAwake(instance, AWAKE_TIMEOUT_MS);
            sNextAwakeAt += SLEEP_DURATION_MS;
        }
    }
    return error;
}

void otPlatLog(otLogLevel aLogLevel, otLogRegion aLogRegion, const char *aFormat, ...)
{
    OT_UNUSED_VARIABLE(aLogLevel);
    OT_UNUSED_VARIABLE(aLogRegion);
    OT_UNUSED_VARIABLE(aFormat);
}
