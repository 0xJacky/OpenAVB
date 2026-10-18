#pragma once

#include <stdint.h>


enum {
    PREFLIGHT_PROBE_DELAY_MICROSECONDS = 20000,
    PREFLIGHT_PROBE_STAGE = 0x50464c54,
};

uint32_t preflight_probe_registry_lookup_count(void);
