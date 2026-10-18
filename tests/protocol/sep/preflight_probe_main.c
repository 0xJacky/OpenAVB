#include "avp/platform/platform.h"
#include "avp/runtime/exception/pvpanic.h"
#include "avp/protocol/sep/sep.h"
#include "avp/protocol/sep/sep_protocol.h"
#include "avp/runtime/support/time.h"
#include "preflight_probe.h"


void firmware_main(void) {
    pvpanic_initialize();
    sep_initialize();

    const uint64_t start = avp_time_microseconds();
    if (!sep_execute_protocol_command(
            SEP_COMMAND_PREFLIGHT_PROBE,
            0,
            0,
            0,
            0,
            0
        ) || !sep_execute_protocol_command(
            SEP_COMMAND_PREFLIGHT_PROBE,
            0,
            0,
            0,
            0,
            0
        )) {
        pvpanic_trigger();
    }
    const uint64_t elapsed = avp_time_microseconds() - start;
    platform_progress(0, (uint32_t)elapsed);
    platform_progress(1, preflight_probe_registry_lookup_count());
    platform_progress(2, 2);
    platform_stage(PREFLIGHT_PROBE_STAGE);
    pvpanic_trigger();
}
