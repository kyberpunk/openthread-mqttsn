#include <stdio.h>

#include "common/instance.hpp"
#include "openthread/platform/logging.h"
#include "openthread/platform/uart.h"
#include "openthread/instance.h"
#include "openthread-system.h"
#include "board.h"
#include "smart_socket_platform.h"

#include "mqttsn_client.hpp"

int main(int argc, char *argv[])
{
	otSysInit(argc, argv);
	socket_platform_init();
	BOARD_InitDebugConsole();

    ot::Instance instance = ot::Instance::InitSingle();
    ot::Mqttsn::MqttsnClient config = ot::Mqttsn::MqttsnClient(instance);

    while (true) {
    	instance.GetTaskletScheduler().ProcessQueuedTasklets();
    	otSysProcessDrivers(&instance);
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
