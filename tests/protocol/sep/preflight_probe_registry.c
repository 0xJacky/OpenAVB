#include "avp/runtime/ipc/runtime_registry.h"
#include "avp/runtime/support/time.h"
#include "preflight_probe.h"


static uint32_t lookup_count;

bool runtime_registry_read_u64(
    uint32_t key,
    uint32_t type,
    uint64_t *value
) {
    if (key != AVP_RUNTIME_KEY_SEP_PREFLIGHT_DEADLINE ||
        type != AVP_RUNTIME_TYPE_ABSOLUTE_MICROSECOND_DEADLINE) {
        return false;
    }

    ++lookup_count;
    /* A relative test deadline proves the production absolute-time path waits. */
    *value = avp_time_microseconds() + PREFLIGHT_PROBE_DELAY_MICROSECONDS;
    return true;
}

uint32_t preflight_probe_registry_lookup_count(void) {
    return lookup_count;
}
