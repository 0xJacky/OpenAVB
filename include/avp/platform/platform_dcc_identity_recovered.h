#pragma once

#include <stdint.h>


#define PLATFORM_DCC_IDENTITY_PUBLIC __attribute__((used, retain))

typedef uint64_t (*PlatformDCCQuery)(void);

void platform_dcc_identity_bind_query(PlatformDCCQuery query);
void platform_dcc_identity_reset_cache(void);

PLATFORM_DCC_IDENTITY_PUBLIC uint8_t platform_dcc_get_high_byte(void);
PLATFORM_DCC_IDENTITY_PUBLIC uint8_t platform_dcc_query_high_byte(void);
PLATFORM_DCC_IDENTITY_PUBLIC uint8_t platform_dcc_get_low_byte(void);
PLATFORM_DCC_IDENTITY_PUBLIC uint8_t platform_dcc_query_low_byte(void);
