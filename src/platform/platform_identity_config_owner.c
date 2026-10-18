#include "avp/platform/platform_identity_config_owner.h"

#include "avp/platform/mmio.h"
#include "avp/platform/platform.h"


static PlatformIdentityConfigTransport bound_transport;

static uint32_t platform_identity_config_default_read32(
    void *context,
    uint32_t offset
) {
    (void)context;
    return mmio_read32(AVP_PLATFORM_BASE + offset);
}

static PlatformIdentityConfigTransport platform_identity_config_transport(
    void
) {
    if (bound_transport.read32 != 0) {
        return bound_transport;
    }
    return (PlatformIdentityConfigTransport){
        .read32 = platform_identity_config_default_read32,
    };
}

static uint32_t platform_identity_optional_boolean(uint32_t value) {
    return value == 0
        ? PLATFORM_IDENTITY_OPTIONAL_FALSE
        : PLATFORM_IDENTITY_OPTIONAL_TRUE;
}

void platform_identity_config_bind_transport(
    PlatformIdentityConfigTransport transport
) {
    if (transport.read32 == 0) {
        __builtin_trap();
    }
    bound_transport = transport;
}

void platform_identity_config_reset_transport(void) {
    bound_transport = (PlatformIdentityConfigTransport){0};
}

uint32_t platform_read_config_u32_shared(uint32_t offset) {
    const PlatformIdentityConfigTransport transport =
        platform_identity_config_transport();
    return transport.read32(transport.context, offset);
}

uint32_t platform_read_production_mode_config_0x08(void) {
    return platform_identity_optional_boolean(
        platform_read_config_u32_shared(
            PLATFORM_IDENTITY_CONFIG_PRODUCTION_MODE
        )
    );
}

uint32_t platform_read_production_mode_config_0x08_clone(void) {
    return platform_read_production_mode_config_0x08();
}

uint32_t platform_read_production_mode_config_0x0c(void) {
    return platform_identity_optional_boolean(
        platform_read_config_u32_shared(
            PLATFORM_IDENTITY_CONFIG_PRODUCTION_MODE_SECONDARY
        )
    );
}

uint64_t platform_read_security_domain(void) {
    return ((uint64_t)platform_read_config_u32_shared(
        PLATFORM_IDENTITY_CONFIG_SECURITY_DOMAIN
    ) << 32) | 1;
}

uint32_t platform_read_config_0x10(void) {
    return platform_read_config_u32_shared(
        PLATFORM_IDENTITY_CONFIG_WORD_0X10
    );
}

uint32_t platform_read_security_epoch(void) {
    return PLATFORM_IDENTITY_SECURITY_EPOCH;
}

uint32_t platform_get_chip_id(void) {
    return PLATFORM_IDENTITY_CHIP_ID;
}

uint32_t platform_read_chip_revision(void) {
    return platform_read_config_u32_shared(
        PLATFORM_IDENTITY_CONFIG_CHIP_REVISION
    );
}

uint64_t platform_read_ecid(void) {
    const uint32_t high = platform_read_config_u32_shared(
        PLATFORM_IDENTITY_CONFIG_ECID_HIGH
    );
    const uint32_t low = platform_read_config_u32_shared(
        PLATFORM_IDENTITY_CONFIG_ECID_LOW
    );
    return ((uint64_t)high << 32) | low;
}

uint32_t platform_read_chip_production_fuse_config(void) {
    return PLATFORM_IDENTITY_OPTIONAL_FALSE;
}

uint32_t j_platform_read_production_mode_config_0x08(void) {
    return platform_read_production_mode_config_0x08();
}

uint32_t j_platform_read_production_mode_config_0x08_clone(void) {
    return platform_read_production_mode_config_0x08_clone();
}

uint64_t j_platform_read_security_domain(void) {
    return platform_read_security_domain();
}

uint32_t j_platform_read_security_epoch(void) {
    return platform_read_security_epoch();
}

uint32_t j_platform_read_security_epoch_clone(void) {
    return platform_read_security_epoch();
}

bool platform_identity_static_flag_default_false(void) {
    return false;
}

uint32_t j_platform_read_chip_production_fuse_config(void) {
    return platform_read_chip_production_fuse_config();
}

uint32_t j_platform_get_chip_id(void) {
    return platform_get_chip_id();
}

uint32_t j_platform_read_chip_revision(void) {
    return platform_read_chip_revision();
}

uint64_t j_platform_read_ecid(void) {
    return platform_read_ecid();
}

uint32_t j_platform_read_config_0x10(void) {
    return platform_read_config_0x10();
}
