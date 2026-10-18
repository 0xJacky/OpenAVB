#include "avp/platform/platform.h"
#include "avp/runtime/exception/pvpanic.h"
#include "avp/runtime/support/stack_guard.h"

#include <stdint.h>


enum {
    STACK_PROTECTOR_PROBE_STAGE = 0x53535046,
};

__attribute__((noinline))
static uint8_t cross_guard_epoch_in_protected_frame(void) {
    volatile uint8_t protected_buffer[8] = {0};
    runtime_stage_stack_guard(runtime_stack_guard() ^ UINT64_C(1));
    runtime_install_stack_guard();
    return protected_buffer[0];
}

void firmware_main(void) {
    pvpanic_initialize();
    platform_stage(STACK_PROTECTOR_PROBE_STAGE);
    platform_progress(0, 1);
    platform_progress(1, cross_guard_epoch_in_protected_frame());
    pvpanic_trigger();
}
