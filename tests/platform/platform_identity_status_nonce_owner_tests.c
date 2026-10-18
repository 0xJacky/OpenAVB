#include "avp/platform/platform_identity_status_nonce_owner.h"

#include "avp/platform/platform.h"
#include "avp/platform/platform_identity_config_owner.h"
#include "avp/runtime/prng/runtime_prng.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static struct {
    uint32_t words[0x700 / sizeof(uint32_t)];
    uint64_t generated_nonce;
    size_t generate_count;
    size_t write_count;
} probe;

static uint32_t production_primary;
static uint32_t production_secondary;
static uint32_t production_fuse;
static RuntimePRNG dummy_prng;

_Noreturn void runtime_firebloom_panic(void) {
    abort();
}

RuntimePRNG *runtime_prng_global(void) {
    return &dummy_prng;
}

RuntimePRNGStatus runtime_prng_generate(
    RuntimePRNG *prng,
    uint8_t *output,
    size_t size
) {
    assert(prng == &dummy_prng);
    assert(size == sizeof(probe.generated_nonce));
    for (size_t index = 0; index < size; ++index) {
        output[index] = (uint8_t)(probe.generated_nonce >> (index * 8));
    }
    ++probe.generate_count;
    return RUNTIME_PRNG_STATUS_OK;
}

uint32_t platform_read_production_mode_config_0x0c(void) {
    return production_primary;
}

uint32_t platform_read_production_mode_config_0x08_clone(void) {
    return production_secondary;
}

uint32_t j_platform_read_production_mode_config_0x08(void) {
    return production_secondary;
}

uint32_t j_platform_read_production_mode_config_0x08_clone(void) {
    return production_secondary;
}

uint32_t j_platform_read_chip_production_fuse_config(void) {
    return production_fuse;
}

static uint32_t read32(void *context, uint32_t offset) {
    assert(context == &probe);
    assert((offset & 3u) == 0);
    assert(offset / sizeof(uint32_t) <
        sizeof(probe.words) / sizeof(probe.words[0]));
    return probe.words[offset / sizeof(uint32_t)];
}

static void write32(void *context, uint32_t offset, uint32_t value) {
    assert(context == &probe);
    assert((offset & 3u) == 0);
    assert(offset / sizeof(uint32_t) <
        sizeof(probe.words) / sizeof(probe.words[0]));
    probe.words[offset / sizeof(uint32_t)] = value;
    ++probe.write_count;
}

static int generate(void *context, uint8_t *output, size_t size) {
    assert(context == &probe);
    assert(size == sizeof(probe.generated_nonce));
    for (size_t index = 0; index < size; ++index) {
        output[index] = (uint8_t)(probe.generated_nonce >> (index * 8));
    }
    ++probe.generate_count;
    return 0;
}

static void bind_probe(void) {
    platform_identity_status_bind_transport(
        (PlatformIdentityStatusTransport){
            .read32 = read32,
            .write32 = write32,
            .generate = generate,
            .context = &probe,
        }
    );
}

static void reset_probe(void) {
    memset(&probe, 0, sizeof(probe));
    bind_probe();
}

static uint32_t *word_at(uint32_t offset) {
    return &probe.words[offset / sizeof(uint32_t)];
}

static void test_status_bits_and_outlined_load(void) {
    reset_probe();
    *word_at(PLATFORM_IDENTITY_STATUS_OFFSET) = 0x80000001u;
    platform_status_flag5_set();
    assert(*word_at(PLATFORM_IDENTITY_STATUS_OFFSET) == 0x80000021u);

    assert(!platform_status_flag3_is_set());
    platform_status_flag3_update(true);
    assert(platform_status_flag3_is_set());
    assert(*word_at(PLATFORM_IDENTITY_STATUS_OFFSET) == 0x80000029u);
    platform_status_flag3_update(false);
    assert(!platform_status_flag3_is_set());
    assert(*word_at(PLATFORM_IDENTITY_STATUS_OFFSET) == 0x80000021u);

    const PlatformIdentityStatusBase loaded =
        platform_identity_outlined_load_status_and_base_x19_w8();
    assert(loaded.base ==
        AVP_PLATFORM_BASE + PLATFORM_IDENTITY_STATUS_OFFSET);
    assert(loaded.status == 0x80000021u);
}

static void test_nonce_generation_cache_and_clear(void) {
    reset_probe();
    probe.generated_nonce = UINT64_C(0x1122334455667788);
    assert(platform_generate_boot_nonce() == probe.generated_nonce);
    assert(probe.generate_count == 1);

    assert(platform_get_boot_nonce() == probe.generated_nonce);
    assert(probe.generate_count == 2);
    assert(*word_at(PLATFORM_IDENTITY_BOOT_NONCE_LOW_OFFSET) == 0x55667788u);
    assert(*word_at(PLATFORM_IDENTITY_BOOT_NONCE_HIGH_OFFSET) == 0x11223344u);
    assert((*word_at(PLATFORM_IDENTITY_STATUS_OFFSET) &
        PLATFORM_IDENTITY_STATUS_NONCE_VALID) != 0);

    probe.generated_nonce = UINT64_C(0xaabbccddeeff0011);
    assert(platform_get_boot_nonce() == UINT64_C(0x1122334455667788));
    assert(probe.generate_count == 2);

    platform_boot_nonce_clear_if_requested(1, 0);
    assert(*word_at(PLATFORM_IDENTITY_BOOT_NONCE_LOW_OFFSET) == 0x55667788u);
    platform_boot_nonce_clear_if_requested(0, 1);
    assert(*word_at(PLATFORM_IDENTITY_BOOT_NONCE_LOW_OFFSET) == 0);
    assert(*word_at(PLATFORM_IDENTITY_BOOT_NONCE_HIGH_OFFSET) == 0);
    assert((*word_at(PLATFORM_IDENTITY_STATUS_OFFSET) &
        PLATFORM_IDENTITY_STATUS_NONCE_VALID) != 0);
    assert(platform_get_boot_nonce() == 0);
}

static void test_usb_nonce_identity_uses_boot_and_sep_results(void) {
    static const uint8_t expected_ap_nonce[AVP_AP_NONCE_SIZE] = {
        0x2a, 0xa2, 0x2c, 0xbc, 0x7a, 0xdb, 0xd3, 0x81,
        0x5e, 0xf7, 0x6c, 0x7c, 0x6b, 0x6b, 0xfa, 0x04,
        0xa8, 0x77, 0xe3, 0x25, 0x6c, 0xb2, 0x3a, 0x3e,
        0xc3, 0x36, 0xf7, 0xf1, 0xcd, 0xd4, 0x50, 0x8f,
    };
    const uint32_t configuration_words[5] = {
        0xdeadbeefu,
        0x01020304u,
        0x11223344u,
        0x55667788u,
        0x90abcdefu,
    };
    const uint8_t expected_sep_nonce[AVP_SEP_NONCE_SIZE] = {
        0xef, 0xbe, 0xad, 0xde,
        0x04, 0x03, 0x02, 0x01,
        0x44, 0x33, 0x22, 0x11,
        0x88, 0x77, 0x66, 0x55,
        0xef, 0xcd, 0xab, 0x90,
    };
    reset_probe();
    probe.generated_nonce = UINT64_C(0x1122334455667788);
    AVPPlatformIdentity identity;
    memset(&identity, 0xa5, sizeof(identity));
    platform_identity_prepare_usb_nonces(&identity, configuration_words);
    assert(probe.generate_count == 1);
    assert(memcmp(
        identity.ap_nonce,
        expected_ap_nonce,
        sizeof(expected_ap_nonce)
    ) == 0);
    assert(memcmp(
        identity.sep_nonce,
        expected_sep_nonce,
        sizeof(expected_sep_nonce)
    ) == 0);

    memset(identity.ap_nonce, 0, sizeof(identity.ap_nonce));
    memset(identity.sep_nonce, 0, sizeof(identity.sep_nonce));
    platform_identity_prepare_usb_nonces(&identity, configuration_words);
    assert(probe.generate_count == 1);
    assert(memcmp(
        identity.ap_nonce,
        expected_ap_nonce,
        sizeof(expected_ap_nonce)
    ) == 0);
}

static void test_hardware_identity_install_copy_and_clear(void) {
    reset_probe();
    uint32_t source[PLATFORM_IDENTITY_HARDWARE_WORD_COUNT];
    uint32_t destination[PLATFORM_IDENTITY_HARDWARE_WORD_COUNT];
    for (size_t index = 0;
         index < PLATFORM_IDENTITY_HARDWARE_WORD_COUNT;
         ++index) {
        source[index] = 0x1000u + (uint32_t)index;
        destination[index] = 0xdeadbeefu;
    }

    assert(platform_identity_copy_boot_hardware_words(
        destination,
        destination,
        destination + PLATFORM_IDENTITY_HARDWARE_WORD_COUNT,
        &platform_identity_u32_type
    ) == -1);

    assert(platform_identity_set_boot_hardware_words(
        source,
        source,
        source + PLATFORM_IDENTITY_HARDWARE_WORD_COUNT,
        &platform_identity_u32_type
    ) == 0);
    assert((*word_at(PLATFORM_IDENTITY_STATUS_OFFSET) &
        PLATFORM_IDENTITY_STATUS_HARDWARE_VALID) != 0);

    memset(destination, 0, sizeof(destination));
    assert(platform_identity_copy_boot_hardware_words(
        destination,
        destination,
        destination + PLATFORM_IDENTITY_HARDWARE_WORD_COUNT,
        &platform_identity_u32_type
    ) == 0);
    assert(memcmp(source, destination, sizeof(source)) == 0);

    RuntimeFirebloomSpan span = platform_identity_validate_u32_span(
        destination,
        destination,
        destination + PLATFORM_IDENTITY_HARDWARE_WORD_COUNT,
        &platform_identity_u32_type
    );
    assert(span.current == destination);
    assert(platform_identity_validate_u32_span_thunk(
        destination,
        destination,
        destination + PLATFORM_IDENTITY_HARDWARE_WORD_COUNT,
        &platform_identity_u32_type
    ).current == destination);
    assert(j_platform_identity_validate_u32_span(
        destination,
        destination,
        destination + PLATFORM_IDENTITY_HARDWARE_WORD_COUNT,
        &platform_identity_u32_type
    ).current == destination);

    assert(platform_identity_set_boot_hardware_words(
        0,
        0,
        0,
        0
    ) == 0);
    assert((*word_at(PLATFORM_IDENTITY_STATUS_OFFSET) &
        PLATFORM_IDENTITY_STATUS_HARDWARE_VALID) == 0);
    for (size_t index = 0;
         index < PLATFORM_IDENTITY_HARDWARE_WORD_COUNT;
         ++index) {
        assert(*word_at(
            PLATFORM_IDENTITY_HARDWARE_WORDS_OFFSET +
                (uint32_t)(index * sizeof(uint32_t))
        ) == 0);
    }
}

static void store_variant(const uint8_t bytes[PLATFORM_IDENTITY_VARIANT_CAPACITY]) {
    for (size_t offset = 0;
         offset < PLATFORM_IDENTITY_VARIANT_CAPACITY;
         offset += sizeof(uint32_t)) {
        uint32_t word = 0;
        for (size_t byte = 0; byte < sizeof(uint32_t); ++byte) {
            word |= (uint32_t)bytes[offset + byte] << (byte * 8);
        }
        *word_at(PLATFORM_IDENTITY_VARIANT_OFFSET + (uint32_t)offset) = word;
    }
}

static void test_variant_snapshot(void) {
    reset_probe();
    RuntimeFirebloomSpan empty = platform_copy_variant_name();
    assert(empty.current == 0);
    assert(empty.lower_bound == 0);
    assert(empty.upper_bound == 0);
    assert(empty.type == 0);

    uint8_t bytes[PLATFORM_IDENTITY_VARIANT_CAPACITY];
    memset(bytes, 'V', sizeof(bytes));
    memcpy(bytes, "t8103-dev", sizeof("t8103-dev"));
    store_variant(bytes);
    RuntimeFirebloomSpan variant = platform_copy_variant_name();
    assert(variant.current == variant.lower_bound);
    assert((uint8_t *)variant.upper_bound - (uint8_t *)variant.current == 65);
    assert(variant.type == &platform_identity_variant_type);
    assert(strcmp(variant.current, "t8103-dev") == 0);

    memset(bytes, 'A', sizeof(bytes));
    store_variant(bytes);
    variant = platform_copy_variant_name();
    assert(((const char *)variant.current)[64] == '\0');
    for (size_t index = 0; index < 64; ++index) {
        assert(((const char *)variant.current)[index] == 'A');
    }
}

static void assert_digest_equal(
    const uint8_t digest[PLATFORM_IDENTITY_SHA384_SIZE],
    const uint8_t expected[PLATFORM_IDENTITY_SHA384_SIZE]
) {
    assert(memcmp(digest, expected, PLATFORM_IDENTITY_SHA384_SIZE) == 0);
}

static void test_sha384_public_backend(void) {
    static const uint8_t expected_abc[PLATFORM_IDENTITY_SHA384_SIZE] = {
        0xcb, 0x00, 0x75, 0x3f, 0x45, 0xa3, 0x5e, 0x8b,
        0xb5, 0xa0, 0x3d, 0x69, 0x9a, 0xc6, 0x50, 0x07,
        0x27, 0x2c, 0x32, 0xab, 0x0e, 0xde, 0xd1, 0x63,
        0x1a, 0x8b, 0x60, 0x5a, 0x43, 0xff, 0x5b, 0xed,
        0x80, 0x86, 0x07, 0x2b, 0xa1, 0xe7, 0xcc, 0x23,
        0x58, 0xba, 0xec, 0xa1, 0x34, 0xc8, 0x25, 0xa7,
    };
    static const uint8_t expected_u64[PLATFORM_IDENTITY_SHA384_SIZE] = {
        0x2a, 0xa2, 0x2c, 0xbc, 0x7a, 0xdb, 0xd3, 0x81,
        0x5e, 0xf7, 0x6c, 0x7c, 0x6b, 0x6b, 0xfa, 0x04,
        0xa8, 0x77, 0xe3, 0x25, 0x6c, 0xb2, 0x3a, 0x3e,
        0xc3, 0x36, 0xf7, 0xf1, 0xcd, 0xd4, 0x50, 0x8f,
        0xc0, 0x62, 0x9b, 0x8f, 0x3e, 0x70, 0xda, 0x66,
        0xa2, 0x8e, 0x83, 0x52, 0xa2, 0x6d, 0x6a, 0xbb,
    };
    static const uint8_t source[] = "abc";
    uint8_t digest[PLATFORM_IDENTITY_SHA384_SIZE];

    platform_sha384_digest_bounded(
        source,
        source,
        source + 3,
        &platform_identity_byte_type,
        3,
        digest,
        digest,
        digest + sizeof(digest),
        &platform_identity_byte_type,
        sizeof(digest)
    );
    assert_digest_equal(digest, expected_abc);

    memset(digest, 0, sizeof(digest));
    platform_sha384_digest_bounded_thunk_a(
        source,
        source,
        source + 3,
        &platform_identity_byte_type,
        3,
        digest,
        digest,
        digest + sizeof(digest),
        &platform_identity_byte_type,
        sizeof(digest)
    );
    assert_digest_equal(digest, expected_abc);

    memset(digest, 0, sizeof(digest));
    platform_sha384_digest_bounded_thunk_b(
        source,
        source,
        source + 3,
        &platform_identity_byte_type,
        3,
        digest,
        digest,
        digest + sizeof(digest),
        &platform_identity_byte_type,
        sizeof(digest)
    );
    assert_digest_equal(digest, expected_abc);

    platform_sha384_digest_u64_to_span(
        UINT64_C(0x1122334455667788),
        digest,
        digest,
        digest + sizeof(digest),
        &platform_identity_byte_type,
        sizeof(digest)
    );
    assert_digest_equal(digest, expected_u64);
}

static void test_production_mode_composition(void) {
    production_primary = PLATFORM_IDENTITY_OPTIONAL_FALSE;
    production_secondary = PLATFORM_IDENTITY_OPTIONAL_FALSE;
    production_fuse = PLATFORM_IDENTITY_OPTIONAL_FALSE;
    assert(platform_read_effective_production_mode() ==
        PLATFORM_IDENTITY_OPTIONAL_FALSE);
    assert(platform_compute_iboot_flags() == 0x24);
    assert(platform_compute_chip_production_fuse_mode() == 0);

    production_primary = PLATFORM_IDENTITY_OPTIONAL_TRUE;
    production_secondary = PLATFORM_IDENTITY_OPTIONAL_TRUE;
    production_fuse = PLATFORM_IDENTITY_OPTIONAL_TRUE;
    assert(platform_read_effective_production_mode() ==
        PLATFORM_IDENTITY_OPTIONAL_TRUE);
    assert(platform_compute_iboot_flags() == 0x3c);
    assert(platform_compute_chip_production_fuse_mode() == 7);

    production_primary = PLATFORM_IDENTITY_OPTIONAL_FALSE;
    production_secondary = PLATFORM_IDENTITY_OPTIONAL_TRUE;
    assert(platform_read_effective_production_mode() ==
        PLATFORM_IDENTITY_OPTIONAL_TRUE);
}

int main(void) {
    test_status_bits_and_outlined_load();
    test_nonce_generation_cache_and_clear();
    test_usb_nonce_identity_uses_boot_and_sep_results();
    test_hardware_identity_install_copy_and_clear();
    test_variant_snapshot();
    test_sha384_public_backend();
    test_production_mode_composition();
    platform_identity_status_reset_transport();
    puts("platform identity status nonce owner tests passed");
    return 0;
}
