#include "avp/platform/platform_dcc_identity_recovered.h"

#include <stdbool.h>


typedef struct {
    bool initialized;
    uint8_t value;
} PlatformDCCByteCache;

static PlatformDCCQuery platform_dcc_query;
static PlatformDCCByteCache high_byte_cache;
static PlatformDCCByteCache low_byte_cache;

void platform_dcc_identity_bind_query(PlatformDCCQuery query) {
    platform_dcc_query = query;
}

void platform_dcc_identity_reset_cache(void) {
    high_byte_cache = (PlatformDCCByteCache){0};
    low_byte_cache = (PlatformDCCByteCache){0};
}

static uint64_t platform_dcc_query_value(void) {
    if (platform_dcc_query != 0) {
        return platform_dcc_query();
    }
#if defined(__aarch64__)
    register uint64_t value __asm__("x0");
    __asm__ volatile("svc #4" : "=r"(value));
    return value;
#else
    __builtin_trap();
#endif
}

uint8_t platform_dcc_query_high_byte(void) {
    const uint64_t value = platform_dcc_query_value();
    high_byte_cache.value = (uint8_t)(value >> 8);
    high_byte_cache.initialized = true;
    return high_byte_cache.value;
}

uint8_t platform_dcc_get_high_byte(void) {
    return high_byte_cache.initialized
        ? high_byte_cache.value
        : platform_dcc_query_high_byte();
}

uint8_t platform_dcc_query_low_byte(void) {
    const uint64_t value = platform_dcc_query_value();
    low_byte_cache.value = (uint8_t)value;
    low_byte_cache.initialized = true;
    return low_byte_cache.value;
}

uint8_t platform_dcc_get_low_byte(void) {
    return low_byte_cache.initialized
        ? low_byte_cache.value
        : platform_dcc_query_low_byte();
}
