#pragma once

#include <stdbool.h>
#include <stdint.h>


#define PLATFORM_IDENTITY_CONFIG_PUBLIC __attribute__((used, retain))

enum {
    PLATFORM_IDENTITY_CONFIG_PRODUCTION_MODE = 0x08,
    PLATFORM_IDENTITY_CONFIG_PRODUCTION_MODE_SECONDARY = 0x0c,
    PLATFORM_IDENTITY_CONFIG_WORD_0X10 = 0x10,
    PLATFORM_IDENTITY_CONFIG_CHIP_REVISION = 0x14,
    PLATFORM_IDENTITY_CONFIG_ECID_LOW = 0x18,
    PLATFORM_IDENTITY_CONFIG_ECID_HIGH = 0x1c,
    PLATFORM_IDENTITY_CONFIG_SECURITY_DOMAIN = 0x2c,
    PLATFORM_IDENTITY_OPTIONAL_FALSE = 0x0001,
    PLATFORM_IDENTITY_OPTIONAL_TRUE = 0x0101,
    PLATFORM_IDENTITY_CHIP_ID = 0xfe01,
    PLATFORM_IDENTITY_SECURITY_EPOCH = 1,
};

typedef uint32_t (*PlatformIdentityConfigRead32)(
    void *context,
    uint32_t offset
);

typedef struct {
    PlatformIdentityConfigRead32 read32;
    void *context;
} PlatformIdentityConfigTransport;

void platform_identity_config_bind_transport(
    PlatformIdentityConfigTransport transport
);
void platform_identity_config_reset_transport(void);

PLATFORM_IDENTITY_CONFIG_PUBLIC uint32_t
platform_read_production_mode_config_0x08(void);
PLATFORM_IDENTITY_CONFIG_PUBLIC uint32_t
platform_read_production_mode_config_0x08_clone(void);
PLATFORM_IDENTITY_CONFIG_PUBLIC uint32_t
platform_read_production_mode_config_0x0c(void);
PLATFORM_IDENTITY_CONFIG_PUBLIC uint64_t platform_read_security_domain(void);
PLATFORM_IDENTITY_CONFIG_PUBLIC uint32_t platform_read_config_0x10(void);
PLATFORM_IDENTITY_CONFIG_PUBLIC uint32_t platform_read_security_epoch(void);
PLATFORM_IDENTITY_CONFIG_PUBLIC uint32_t platform_get_chip_id(void);
PLATFORM_IDENTITY_CONFIG_PUBLIC uint32_t platform_read_chip_revision(void);
PLATFORM_IDENTITY_CONFIG_PUBLIC uint64_t platform_read_ecid(void);
PLATFORM_IDENTITY_CONFIG_PUBLIC uint32_t platform_read_config_u32_shared(
    uint32_t offset
);
PLATFORM_IDENTITY_CONFIG_PUBLIC uint32_t
platform_read_chip_production_fuse_config(void);

PLATFORM_IDENTITY_CONFIG_PUBLIC uint32_t
j_platform_read_production_mode_config_0x08(void);
PLATFORM_IDENTITY_CONFIG_PUBLIC uint32_t
j_platform_read_production_mode_config_0x08_clone(void);
PLATFORM_IDENTITY_CONFIG_PUBLIC uint64_t j_platform_read_security_domain(void);
PLATFORM_IDENTITY_CONFIG_PUBLIC uint32_t
j_platform_read_security_epoch(void);
PLATFORM_IDENTITY_CONFIG_PUBLIC uint32_t
j_platform_read_security_epoch_clone(void);
PLATFORM_IDENTITY_CONFIG_PUBLIC bool
platform_identity_static_flag_default_false(void);
PLATFORM_IDENTITY_CONFIG_PUBLIC uint32_t
j_platform_read_chip_production_fuse_config(void);
PLATFORM_IDENTITY_CONFIG_PUBLIC uint32_t j_platform_get_chip_id(void);
PLATFORM_IDENTITY_CONFIG_PUBLIC uint32_t j_platform_read_chip_revision(void);
PLATFORM_IDENTITY_CONFIG_PUBLIC uint64_t j_platform_read_ecid(void);
PLATFORM_IDENTITY_CONFIG_PUBLIC uint32_t j_platform_read_config_0x10(void);
