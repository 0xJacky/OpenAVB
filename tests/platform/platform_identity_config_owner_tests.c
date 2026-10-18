#include "avp/platform/platform_identity_config_owner.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>


static struct {
    uint32_t words[12];
    uint32_t offsets[32];
    uint32_t read_count;
} probe;

static uint32_t read32(void *context, uint32_t offset) {
    assert(context == &probe);
    assert((offset & 3) == 0);
    assert(offset / 4 < 12);
    assert(probe.read_count < 32);
    probe.offsets[probe.read_count++] = offset;
    return probe.words[offset / 4];
}

static void bind_probe(void) {
    platform_identity_config_bind_transport(
        (PlatformIdentityConfigTransport){
            .read32 = read32,
            .context = &probe,
        }
    );
}

static void test_optional_boolean_accessors(void) {
    memset(&probe, 0, sizeof(probe));
    bind_probe();
    assert(platform_read_production_mode_config_0x08() ==
        PLATFORM_IDENTITY_OPTIONAL_FALSE);
    probe.words[PLATFORM_IDENTITY_CONFIG_PRODUCTION_MODE / 4] = 7;
    assert(platform_read_production_mode_config_0x08_clone() ==
        PLATFORM_IDENTITY_OPTIONAL_TRUE);
    assert(j_platform_read_production_mode_config_0x08() ==
        PLATFORM_IDENTITY_OPTIONAL_TRUE);
    assert(j_platform_read_production_mode_config_0x08_clone() ==
        PLATFORM_IDENTITY_OPTIONAL_TRUE);

    assert(platform_read_production_mode_config_0x0c() ==
        PLATFORM_IDENTITY_OPTIONAL_FALSE);
    probe.words[
        PLATFORM_IDENTITY_CONFIG_PRODUCTION_MODE_SECONDARY / 4
    ] = UINT32_MAX;
    assert(platform_read_production_mode_config_0x0c() ==
        PLATFORM_IDENTITY_OPTIONAL_TRUE);
    assert(platform_read_chip_production_fuse_config() ==
        PLATFORM_IDENTITY_OPTIONAL_FALSE);
    assert(j_platform_read_chip_production_fuse_config() ==
        PLATFORM_IDENTITY_OPTIONAL_FALSE);
}

static void test_identity_accessors_and_veneers(void) {
    memset(&probe, 0, sizeof(probe));
    probe.words[PLATFORM_IDENTITY_CONFIG_SECURITY_DOMAIN / 4] = 5;
    probe.words[PLATFORM_IDENTITY_CONFIG_WORD_0X10 / 4] = 0xaabbccdd;
    probe.words[PLATFORM_IDENTITY_CONFIG_CHIP_REVISION / 4] = 0x32;
    probe.words[PLATFORM_IDENTITY_CONFIG_ECID_LOW / 4] = 0x55667788;
    probe.words[PLATFORM_IDENTITY_CONFIG_ECID_HIGH / 4] = 0x11223344;
    bind_probe();

    assert(platform_read_security_domain() == UINT64_C(0x0000000500000001));
    assert(j_platform_read_security_domain() == UINT64_C(0x0000000500000001));
    assert(platform_read_config_0x10() == 0xaabbccdd);
    assert(j_platform_read_config_0x10() == 0xaabbccdd);
    assert(platform_read_security_epoch() == 1);
    assert(j_platform_read_security_epoch() == 1);
    assert(j_platform_read_security_epoch_clone() == 1);
    assert(platform_get_chip_id() == 0xfe01);
    assert(j_platform_get_chip_id() == 0xfe01);
    assert(platform_read_chip_revision() == 0x32);
    assert(j_platform_read_chip_revision() == 0x32);
    assert(platform_read_ecid() == UINT64_C(0x1122334455667788));
    assert(j_platform_read_ecid() == UINT64_C(0x1122334455667788));
    assert(!platform_identity_static_flag_default_false());
    assert(platform_read_config_u32_shared(
        PLATFORM_IDENTITY_CONFIG_CHIP_REVISION
    ) == 0x32);

    assert(probe.offsets[0] == PLATFORM_IDENTITY_CONFIG_SECURITY_DOMAIN);
    assert(probe.offsets[1] == PLATFORM_IDENTITY_CONFIG_SECURITY_DOMAIN);
    assert(probe.offsets[2] == PLATFORM_IDENTITY_CONFIG_WORD_0X10);
    assert(probe.offsets[3] == PLATFORM_IDENTITY_CONFIG_WORD_0X10);
    assert(probe.offsets[4] == PLATFORM_IDENTITY_CONFIG_CHIP_REVISION);
    assert(probe.offsets[5] == PLATFORM_IDENTITY_CONFIG_CHIP_REVISION);
    assert(probe.offsets[6] == PLATFORM_IDENTITY_CONFIG_ECID_HIGH);
    assert(probe.offsets[7] == PLATFORM_IDENTITY_CONFIG_ECID_LOW);
    assert(probe.offsets[8] == PLATFORM_IDENTITY_CONFIG_ECID_HIGH);
    assert(probe.offsets[9] == PLATFORM_IDENTITY_CONFIG_ECID_LOW);
    assert(probe.offsets[10] == PLATFORM_IDENTITY_CONFIG_CHIP_REVISION);
    platform_identity_config_reset_transport();
}

int main(void) {
    test_optional_boolean_accessors();
    test_identity_accessors_and_veneers();
    puts("platform identity config owner tests passed");
    return 0;
}
