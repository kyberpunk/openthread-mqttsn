#include "openthread/platform/logging.h"
#include "openthread/instance.h"

int main(int argc, char *argv[])
{
    otInstance *instance;
}

void otPlatLog(otLogLevel aLogLevel, otLogRegion aLogRegion, const char *aFormat, ...)
{
    OT_UNUSED_VARIABLE(aLogLevel);
    OT_UNUSED_VARIABLE(aLogRegion);
    OT_UNUSED_VARIABLE(aFormat);

    va_list ap;
    va_start(ap, aFormat);
    // TODO: Log
    va_end(ap);
}
