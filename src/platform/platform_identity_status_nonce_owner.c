#include "avp/platform/platform_identity_status_nonce_owner.h"

#include "avp/crypto/hash/corecrypto_sha512_compat.h"
#include "avp/platform/mmio.h"
#include "avp/platform/platform.h"
#include "avp/platform/platform_identity_config_owner.h"
#include "avp/runtime/memory/runtime_memory.h"
#include "avp/runtime/prng/runtime_prng.h"


const RuntimeFirebloomType platform_identity_u32_type = {
    .identity = 0x00144f00,
    .element_size = sizeof(uint32_t),
    .bytewise_copyable = false,
};

const RuntimeFirebloomType platform_identity_variant_type = {
    .identity = 0x00144f18,
    .element_size = sizeof(uint32_t),
    .bytewise_copyable = false,
};

const RuntimeFirebloomType platform_identity_byte_type = {
    .identity = 0x00144f30,
    .element_size = sizeof(uint8_t),
    .bytewise_copyable = true,
};

#if defined(__ELF__)
/* Nonce identity state shares the already mapped PRNG platform-state window. */
#define PLATFORM_IDENTITY_STATE \
    __attribute__((section(".runtime_prng.state"), used))
#else
#define PLATFORM_IDENTITY_STATE
#endif

static PlatformIdentityStatusTransport bound_transport
    PLATFORM_IDENTITY_STATE;
static uint8_t variant_name[PLATFORM_IDENTITY_VARIANT_CAPACITY + 1]
    PLATFORM_IDENTITY_STATE;

static _Noreturn void platform_identity_contract_failure(void) {
    runtime_firebloom_panic();
}

static uint32_t platform_identity_default_read32(
    void *context,
    uint32_t offset
) {
    (void)context;
    return mmio_read32(AVP_PLATFORM_BASE + offset);
}

static void platform_identity_default_write32(
    void *context,
    uint32_t offset,
    uint32_t value
) {
    (void)context;
    mmio_write32(AVP_PLATFORM_BASE + offset, value);
}

static int platform_identity_default_generate(
    void *context,
    uint8_t *output,
    size_t size
) {
    (void)context;
    return runtime_prng_generate(runtime_prng_global(), output, size);
}

static PlatformIdentityStatusTransport platform_identity_transport(void) {
    if (bound_transport.read32 != 0) {
        return bound_transport;
    }
    return (PlatformIdentityStatusTransport){
        .read32 = platform_identity_default_read32,
        .write32 = platform_identity_default_write32,
        .generate = platform_identity_default_generate,
    };
}

static void platform_identity_require_span(
    const void *current,
    const void *lower_bound,
    const void *upper_bound,
    const RuntimeFirebloomType *type_context,
    size_t size
) {
    const uintptr_t current_address = (uintptr_t)current;
    const uintptr_t lower_address = (uintptr_t)lower_bound;
    const uintptr_t upper_address = (uintptr_t)upper_bound;
    if (type_context == 0 || current_address < lower_address ||
        current_address > upper_address ||
        size > upper_address - current_address) {
        platform_identity_contract_failure();
    }
}

static void platform_identity_require_u32_span(
    const void *current,
    const void *lower_bound,
    const void *upper_bound,
    const RuntimeFirebloomType *type_context,
    size_t size
) {
    platform_identity_require_span(
        current,
        lower_bound,
        upper_bound,
        type_context,
        size
    );
    if (type_context->element_size != sizeof(uint32_t) ||
        type_context->identity != platform_identity_u32_type.identity ||
        ((uintptr_t)current - (uintptr_t)lower_bound) % sizeof(uint32_t) != 0) {
        platform_identity_contract_failure();
    }
}

static uint32_t platform_identity_status_read(void) {
    const PlatformIdentityStatusTransport transport =
        platform_identity_transport();
    return transport.read32(
        transport.context,
        PLATFORM_IDENTITY_STATUS_OFFSET
    );
}

static void platform_identity_status_write(uint32_t value) {
    const PlatformIdentityStatusTransport transport =
        platform_identity_transport();
    transport.write32(
        transport.context,
        PLATFORM_IDENTITY_STATUS_OFFSET,
        value
    );
}

void platform_identity_status_bind_transport(
    PlatformIdentityStatusTransport transport
) {
    if (transport.read32 == 0 || transport.write32 == 0 ||
        transport.generate == 0) {
        platform_identity_contract_failure();
    }
    bound_transport = transport;
}

void platform_identity_status_reset_transport(void) {
    bound_transport = (PlatformIdentityStatusTransport){0};
}

uint16_t platform_read_effective_production_mode(void) {
    const uint16_t primary =
        (uint16_t)platform_read_production_mode_config_0x0c();
    const uint16_t secondary =
        (uint16_t)platform_read_production_mode_config_0x08_clone();
    const uint16_t presence = (uint16_t)(primary & secondary & UINT16_C(0xff));
    const uint16_t payload = (uint16_t)((primary | secondary) & UINT16_C(0xff00));
    return (uint16_t)(presence | payload);
}

uint32_t platform_compute_iboot_flags(void) {
    const uint32_t effective = platform_read_effective_production_mode();
    const uint32_t config = j_platform_read_production_mode_config_0x08();
    const uint32_t base = (effective & 0x100u) == 0 ? 0x24u : 0x2cu;
    return base | ((config >> 4) & 0x10u);
}

uint32_t platform_compute_chip_production_fuse_mode(void) {
    const uint32_t effective = platform_read_effective_production_mode();
    const uint32_t config =
        j_platform_read_production_mode_config_0x08_clone();
    const uint32_t fuse = j_platform_read_chip_production_fuse_config();
    return ((effective >> 8) & 1u) |
        ((config >> 7) & 2u) |
        ((fuse >> 6) & 4u);
}

void platform_sha384_digest_bounded(
    const void *source_current,
    const void *source_lower_bound,
    const void *source_upper_bound,
    const RuntimeFirebloomType *source_type,
    size_t source_length,
    uint8_t *digest_current,
    uint8_t *digest_lower_bound,
    uint8_t *digest_upper_bound,
    const RuntimeFirebloomType *digest_type,
    size_t digest_capacity
) {
    platform_identity_require_span(
        source_current,
        source_lower_bound,
        source_upper_bound,
        source_type,
        source_length
    );
    if (digest_capacity < PLATFORM_IDENTITY_SHA384_SIZE) {
        platform_identity_contract_failure();
    }
    platform_identity_require_span(
        digest_current,
        digest_lower_bound,
        digest_upper_bound,
        digest_type,
        PLATFORM_IDENTITY_SHA384_SIZE
    );

    AVPCCSHA512Context context;
    avp_ccsha512_init(&context, &avp_ccsha384_info);
    if (avp_ccsha512_update(&context, source_current, source_length) != 0 ||
        ccsha512_final(&avp_ccsha384_info, &context, digest_current) != 0) {
        platform_identity_contract_failure();
    }
}

void platform_sha384_digest_bounded_thunk_a(
    const void *source_current,
    const void *source_lower_bound,
    const void *source_upper_bound,
    const RuntimeFirebloomType *source_type,
    size_t source_length,
    uint8_t *digest_current,
    uint8_t *digest_lower_bound,
    uint8_t *digest_upper_bound,
    const RuntimeFirebloomType *digest_type,
    size_t digest_capacity
) {
    platform_sha384_digest_bounded(
        source_current,
        source_lower_bound,
        source_upper_bound,
        source_type,
        source_length,
        digest_current,
        digest_lower_bound,
        digest_upper_bound,
        digest_type,
        digest_capacity
    );
}

void platform_sha384_digest_bounded_thunk_b(
    const void *source_current,
    const void *source_lower_bound,
    const void *source_upper_bound,
    const RuntimeFirebloomType *source_type,
    size_t source_length,
    uint8_t *digest_current,
    uint8_t *digest_lower_bound,
    uint8_t *digest_upper_bound,
    const RuntimeFirebloomType *digest_type,
    size_t digest_capacity
) {
    platform_sha384_digest_bounded(
        source_current,
        source_lower_bound,
        source_upper_bound,
        source_type,
        source_length,
        digest_current,
        digest_lower_bound,
        digest_upper_bound,
        digest_type,
        digest_capacity
    );
}

void platform_sha384_digest_u64_to_span(
    uint64_t value,
    uint8_t *digest_current,
    uint8_t *digest_lower_bound,
    uint8_t *digest_upper_bound,
    const RuntimeFirebloomType *digest_type,
    size_t digest_capacity
) {
    platform_sha384_digest_bounded_thunk_a(
        &value,
        &value,
        (const uint8_t *)&value + sizeof(value),
        &platform_identity_byte_type,
        sizeof(value),
        digest_current,
        digest_lower_bound,
        digest_upper_bound,
        digest_type,
        digest_capacity
    );
}

uint64_t platform_generate_boot_nonce(void) {
    uint64_t nonce = 0;
    const PlatformIdentityStatusTransport transport =
        platform_identity_transport();
    if (transport.generate(
            transport.context,
            (uint8_t *)&nonce,
            sizeof(nonce)
        ) != 0) {
        platform_identity_contract_failure();
    }
    return nonce;
}

void platform_status_flag5_set(void) {
    platform_identity_status_write(
        platform_identity_status_read() | PLATFORM_IDENTITY_STATUS_FLAG5
    );
}

void platform_boot_nonce_clear_if_requested(
    uint32_t primary_request,
    uint32_t clear_request
) {
    (void)primary_request;
    if ((clear_request & 1u) != 0) {
        const PlatformIdentityStatusTransport transport =
            platform_identity_transport();
        transport.write32(
            transport.context,
            PLATFORM_IDENTITY_BOOT_NONCE_LOW_OFFSET,
            0
        );
        transport.write32(
            transport.context,
            PLATFORM_IDENTITY_BOOT_NONCE_HIGH_OFFSET,
            0
        );
    }
}

uint64_t platform_get_boot_nonce(void) {
    const PlatformIdentityStatusTransport transport =
        platform_identity_transport();
    uint32_t status = platform_identity_status_read();
    if ((status & PLATFORM_IDENTITY_STATUS_NONCE_VALID) == 0) {
        const uint64_t nonce = platform_generate_boot_nonce();
        const uint32_t low = (uint32_t)nonce;
        transport.write32(
            transport.context,
            PLATFORM_IDENTITY_BOOT_NONCE_LOW_OFFSET,
            low
        );
        transport.write32(
            transport.context,
            PLATFORM_IDENTITY_BOOT_NONCE_HIGH_OFFSET,
            (uint32_t)(nonce >> 32)
        );
        status |= PLATFORM_IDENTITY_STATUS_NONCE_VALID;
        platform_identity_status_write(status);
        if (low == 0) {
            platform_identity_contract_failure();
        }
    }
    const uint32_t low = transport.read32(
        transport.context,
        PLATFORM_IDENTITY_BOOT_NONCE_LOW_OFFSET
    );
    const uint32_t high = transport.read32(
        transport.context,
        PLATFORM_IDENTITY_BOOT_NONCE_HIGH_OFFSET
    );
    return ((uint64_t)high << 32) | low;
}

void platform_identity_prepare_usb_nonces(
    AVPPlatformIdentity *identity,
    const uint32_t configuration_words[5]
) {
    if (identity == 0 || configuration_words == 0) {
        platform_identity_contract_failure();
    }
    uint8_t digest[PLATFORM_IDENTITY_SHA384_SIZE];
    platform_sha384_digest_u64_to_span(
        platform_get_boot_nonce(),
        digest,
        digest,
        digest + sizeof(digest),
        &platform_identity_byte_type,
        sizeof(digest)
    );
    for (size_t index = 0; index < sizeof(identity->ap_nonce); ++index) {
        identity->ap_nonce[index] = digest[index];
    }
    /* SEP returns its 20-byte nonce through five little-endian configuration
     * words, which is the byte representation consumed by the DFU identity. */
    for (size_t index = 0; index < sizeof(identity->sep_nonce); ++index) {
        identity->sep_nonce[index] = (uint8_t)(
            configuration_words[index / sizeof(uint32_t)] >>
            ((index % sizeof(uint32_t)) * 8)
        );
    }
}

RuntimeFirebloomSpan platform_identity_validate_u32_span(
    void *current,
    const void *lower_bound,
    const void *upper_bound,
    const RuntimeFirebloomType *type_context
) {
    platform_identity_require_u32_span(
        current,
        lower_bound,
        upper_bound,
        type_context,
        0
    );
    return (RuntimeFirebloomSpan){
        .current = current,
        .lower_bound = (void *)lower_bound,
        .upper_bound = (void *)upper_bound,
        .type = type_context,
    };
}

RuntimeFirebloomSpan platform_identity_validate_u32_span_thunk(
    void *current,
    const void *lower_bound,
    const void *upper_bound,
    const RuntimeFirebloomType *type_context
) {
    return platform_identity_validate_u32_span(
        current,
        lower_bound,
        upper_bound,
        type_context
    );
}

RuntimeFirebloomSpan j_platform_identity_validate_u32_span(
    void *current,
    const void *lower_bound,
    const void *upper_bound,
    const RuntimeFirebloomType *type_context
) {
    return platform_identity_validate_u32_span(
        current,
        lower_bound,
        upper_bound,
        type_context
    );
}

PlatformIdentityStatusBase
platform_identity_outlined_load_status_and_base_x19_w8(void) {
    return (PlatformIdentityStatusBase){
        .base = AVP_PLATFORM_BASE + PLATFORM_IDENTITY_STATUS_OFFSET,
        .status = platform_identity_status_read(),
    };
}

int platform_identity_copy_boot_hardware_words(
    uint32_t *current,
    uint32_t *lower_bound,
    uint32_t *upper_bound,
    const RuntimeFirebloomType *type_context
) {
    if (current == 0) {
        platform_identity_contract_failure();
    }
    if ((platform_identity_status_read() &
            PLATFORM_IDENTITY_STATUS_HARDWARE_VALID) == 0) {
        return -1;
    }
    (void)platform_identity_validate_u32_span_thunk(
        current,
        lower_bound,
        upper_bound,
        type_context
    );
    platform_identity_require_u32_span(
        current,
        lower_bound,
        upper_bound,
        type_context,
        PLATFORM_IDENTITY_HARDWARE_WORD_COUNT * sizeof(uint32_t)
    );
    const PlatformIdentityStatusTransport transport =
        platform_identity_transport();
    for (size_t index = 0;
         index < PLATFORM_IDENTITY_HARDWARE_WORD_COUNT;
         ++index) {
        current[index] = transport.read32(
            transport.context,
            PLATFORM_IDENTITY_HARDWARE_WORDS_OFFSET +
                (uint32_t)(index * sizeof(uint32_t))
        );
    }
    return 0;
}

int platform_identity_set_boot_hardware_words(
    const uint32_t *current,
    const uint32_t *lower_bound,
    const uint32_t *upper_bound,
    const RuntimeFirebloomType *type_context
) {
    const PlatformIdentityStatusTransport transport =
        platform_identity_transport();
    uint32_t status = platform_identity_status_read();
    if (current != 0) {
        (void)platform_identity_validate_u32_span_thunk(
            (void *)current,
            lower_bound,
            upper_bound,
            type_context
        );
        platform_identity_require_u32_span(
            current,
            lower_bound,
            upper_bound,
            type_context,
            PLATFORM_IDENTITY_HARDWARE_WORD_COUNT * sizeof(uint32_t)
        );
        status |= PLATFORM_IDENTITY_STATUS_HARDWARE_VALID;
    } else {
        status &= ~PLATFORM_IDENTITY_STATUS_HARDWARE_VALID;
    }
    for (size_t index = 0;
         index < PLATFORM_IDENTITY_HARDWARE_WORD_COUNT;
         ++index) {
        transport.write32(
            transport.context,
            PLATFORM_IDENTITY_HARDWARE_WORDS_OFFSET +
                (uint32_t)(index * sizeof(uint32_t)),
            current != 0 ? current[index] : 0
        );
    }
    platform_identity_status_write(status);
    return 0;
}

bool platform_status_flag3_is_set(void) {
    return (platform_identity_status_read() &
        PLATFORM_IDENTITY_STATUS_FLAG3) != 0;
}

void platform_status_flag3_update(bool enabled) {
    uint32_t status = platform_identity_status_read() &
        ~PLATFORM_IDENTITY_STATUS_FLAG3;
    if (enabled) {
        status |= PLATFORM_IDENTITY_STATUS_FLAG3;
    }
    platform_identity_status_write(status);
}

RuntimeFirebloomSpan platform_copy_variant_name(void) {
    const PlatformIdentityStatusTransport transport =
        platform_identity_transport();
    for (size_t offset = 0;
         offset < PLATFORM_IDENTITY_VARIANT_CAPACITY;
         offset += sizeof(uint32_t)) {
        const uint32_t word = transport.read32(
            transport.context,
            PLATFORM_IDENTITY_VARIANT_OFFSET + (uint32_t)offset
        );
        for (size_t byte = 0; byte < sizeof(uint32_t); ++byte) {
            variant_name[offset + byte] =
                (uint8_t)(word >> (byte * 8));
        }
    }
    variant_name[PLATFORM_IDENTITY_VARIANT_CAPACITY] = '\0';
    if (variant_name[0] == '\0') {
        return (RuntimeFirebloomSpan){0};
    }
    return (RuntimeFirebloomSpan){
        .current = variant_name,
        .lower_bound = variant_name,
        .upper_bound = variant_name + sizeof(variant_name),
        .type = &platform_identity_variant_type,
    };
}
