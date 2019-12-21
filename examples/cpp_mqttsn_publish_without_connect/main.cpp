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
#include <string.h>

#include "common/instance.hpp"
#include "openthread/instance.h"
#include "openthread/mqttsn.h"
#include "openthread-system.h"
#include "utils/slaac_address.hpp"

#include "mqttsn/mqttsn_client.hpp"

#define NETWORK_NAME "OTBR4444"
#define PANID 0x4444
#define EXTPANID {0x33, 0x33, 0x33, 0x33, 0x44, 0x44, 0x44, 0x44}
#define DEFAULT_CHANNEL 15
#define MASTER_KEY {0x33, 0x33, 0x44, 0x44, 0x33, 0x33, 0x44, 0x44, 0x33, 0x33, 0x44, 0x44, 0x33, 0x33, 0x44, 0x44}

#define GATEWAY_PORT 10000
#define GATEWAY_ADDRESS "2018:ff9b::ac12:8"

#define CLIENT_ID "THREAD"
#define CLIENT_PORT 10000

#define TOPIC_NAME "se"

using namespace ot::Mqttsn;

static MqttsnClient* sClient = NULL;

static const uint8_t sExpanId[] = EXTPANID;
static const uint8_t sMasterKey[] = MASTER_KEY;

static void Publish()
{
    // Publish data with QoS level -1
    // No connection establishment is needed
    // Short topic name is used since it is not possible to register without connection
    ot::Ip6::Address address;
    address.FromString(GATEWAY_ADDRESS);
    const char* data = "{\"temperature\":24.0}";
    int32_t length = strlen(data);
    sClient->PublishQosm1(reinterpret_cast<const uint8_t*>(data), length, TOPIC_NAME, address, GATEWAY_PORT);
}

static void StateChanged(otChangedFlags aFlags, void *aContext)
{
    ot::Instance &instance = *reinterpret_cast<ot::Instance*>(aContext);
    // when thread role changed
    if (aFlags & OT_CHANGED_THREAD_ROLE)
    {
        otDeviceRole role = instance.Get<ot::Mle::MleRouter>().GetRole();
        // If role changed to any of active roles then publish
        if (role == OT_DEVICE_ROLE_CHILD || role == OT_DEVICE_ROLE_LEADER || role == OT_DEVICE_ROLE_ROUTER)
        {
            Publish();
        }
    }
}

int main(int aArgc, char *aArgv[])
{
    otError error = OT_ERROR_NONE;
    ot::Mac::ExtendedPanId extendedPanid;
    ot::MasterKey masterKey;

    otSysInit(aArgc, aArgv);
    ot::Instance &instance = ot::Instance::InitSingle();
    sClient = &instance.Get<MqttsnClient>();
    ot::ThreadNetif &netif = instance.Get<ot::ThreadNetif>();
    ot::Mac::Mac &mac = instance.Get<ot::Mac::Mac>();

    // Set default network settings
    // Set network name
    SuccessOrExit(error = mac.SetNetworkName(NETWORK_NAME));
    // Set extended PANID
    memcpy(extendedPanid.m8, sExpanId, sizeof(sExpanId));
    mac.SetExtendedPanId(extendedPanid);
    // Set PANID
    mac.SetPanId(PANID);
    // Set channel
    SuccessOrExit(error = mac.SetPanChannel(DEFAULT_CHANNEL));
    // Set masterkey
    memcpy(masterKey.m8, sMasterKey, sizeof(sMasterKey));
    SuccessOrExit(error = instance.Get<ot::KeyManager>().SetMasterKey(masterKey));

    instance.Get<ot::MeshCoP::ActiveDataset>().Clear();
    instance.Get<ot::MeshCoP::PendingDataset>().Clear();
    // Register notifier callback to receive thread role changed events
    instance.Get<ot::Notifier>().RegisterCallback(StateChanged, &instance);

    // Start thread network
    instance.Get<ot::Utils::Slaac>().Enable();
    netif.Up();
    SuccessOrExit(error = instance.Get<ot::Mle::MleRouter>().Start(false));

    // Start MQTT-SN client
    SuccessOrExit(error = sClient->Start(CLIENT_PORT));

    while (true)
    {
        instance.Get<ot::TaskletScheduler>().ProcessQueuedTasklets();
        otSysProcessDrivers(&instance);
    }
    return 0;

exit:
    return 1;
}

extern "C" void otPlatLog(otLogLevel aLogLevel, otLogRegion aLogRegion, const char *aFormat, ...)
{
    OT_UNUSED_VARIABLE(aLogLevel);
    OT_UNUSED_VARIABLE(aLogRegion);
    OT_UNUSED_VARIABLE(aFormat);
}
