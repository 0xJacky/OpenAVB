#include "avp/platform/platform.h"
#include "avp/runtime/exception/pvpanic.h"


enum {
    RUNTIME_SVC_INVALID_PROBE_STAGE = 0x53564349,
};

void firmware_main(void) {
    pvpanic_initialize();
    platform_stage(RUNTIME_SVC_INVALID_PROBE_STAGE);
    __asm__ volatile("svc #0xc4");
    platform_progress(0, 1);
    pvpanic_trigger();
}
