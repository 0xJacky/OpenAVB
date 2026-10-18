#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "avp/platform/platform.h"
#include "avp/runtime/firebloom/runtime_firebloom.h"


#if __has_attribute(retain)
#define PLATFORM_IDENTITY_STATUS_PUBLIC __attribute__((used, retain))
#else
#define PLATFORM_IDENTITY_STATUS_PUBLIC __attribute__((used))
#endif

enum {
    PLATFORM_IDENTITY_STATUS_OFFSET = 0x300,
    PLATFORM_IDENTITY_BOOT_NONCE_LOW_OFFSET = 0x328,
    PLATFORM_IDENTITY_BOOT_NONCE_HIGH_OFFSET = 0x32c,
    PLATFORM_IDENTITY_HARDWARE_WORDS_OFFSET = 0x330,
    PLATFORM_IDENTITY_HARDWARE_WORD_COUNT = 12,
    PLATFORM_IDENTITY_VARIANT_OFFSET = 0x5c8,
    PLATFORM_IDENTITY_VARIANT_CAPACITY = 64,
    PLATFORM_IDENTITY_STATUS_NONCE_VALID = 1u << 1,
    PLATFORM_IDENTITY_STATUS_FLAG3 = 1u << 3,
    PLATFORM_IDENTITY_STATUS_HARDWARE_VALID = 1u << 4,
    PLATFORM_IDENTITY_STATUS_FLAG5 = 1u << 5,
    PLATFORM_IDENTITY_SHA384_SIZE = 48,
};

typedef uint32_t (*PlatformIdentityStatusRead32)(
    void *context,
    uint32_t offset
);
typedef void (*PlatformIdentityStatusWrite32)(
    void *context,
    uint32_t offset,
    uint32_t value
);
typedef int (*PlatformIdentityStatusGenerate)(
    void *context,
    uint8_t *output,
    size_t size
);

typedef struct {
    PlatformIdentityStatusRead32 read32;
    PlatformIdentityStatusWrite32 write32;
    PlatformIdentityStatusGenerate generate;
    void *context;
} PlatformIdentityStatusTransport;

typedef struct {
    uintptr_t base;
    uint32_t status;
} PlatformIdentityStatusBase;

extern const RuntimeFirebloomType platform_identity_u32_type;
extern const RuntimeFirebloomType platform_identity_variant_type;
extern const RuntimeFirebloomType platform_identity_byte_type;

void platform_identity_status_bind_transport(
    PlatformIdentityStatusTransport transport
);
void platform_identity_status_reset_transport(void);

PLATFORM_IDENTITY_STATUS_PUBLIC uint32_t platform_compute_iboot_flags(void);
PLATFORM_IDENTITY_STATUS_PUBLIC void platform_sha384_digest_u64_to_span(
    uint64_t value,
    uint8_t *digest_current,
    uint8_t *digest_lower_bound,
    uint8_t *digest_upper_bound,
    const RuntimeFirebloomType *digest_type,
    size_t digest_capacity
);
PLATFORM_IDENTITY_STATUS_PUBLIC uint64_t platform_generate_boot_nonce(void);
PLATFORM_IDENTITY_STATUS_PUBLIC void platform_status_flag5_set(void);
PLATFORM_IDENTITY_STATUS_PUBLIC void platform_boot_nonce_clear_if_requested(
    uint32_t primary_request,
    uint32_t clear_request
);
PLATFORM_IDENTITY_STATUS_PUBLIC uint64_t platform_get_boot_nonce(void);
PLATFORM_IDENTITY_STATUS_PUBLIC void platform_identity_prepare_usb_nonces(
    AVPPlatformIdentity *identity,
    const uint32_t configuration_words[5]
);
PLATFORM_IDENTITY_STATUS_PUBLIC int
platform_identity_copy_boot_hardware_words(
    uint32_t *current,
    uint32_t *lower_bound,
    uint32_t *upper_bound,
    const RuntimeFirebloomType *type_context
);
PLATFORM_IDENTITY_STATUS_PUBLIC int
platform_identity_set_boot_hardware_words(
    const uint32_t *current,
    const uint32_t *lower_bound,
    const uint32_t *upper_bound,
    const RuntimeFirebloomType *type_context
);
PLATFORM_IDENTITY_STATUS_PUBLIC bool platform_status_flag3_is_set(void);
PLATFORM_IDENTITY_STATUS_PUBLIC void platform_status_flag3_update(
    bool enabled
);
PLATFORM_IDENTITY_STATUS_PUBLIC RuntimeFirebloomSpan
platform_copy_variant_name(void);
PLATFORM_IDENTITY_STATUS_PUBLIC RuntimeFirebloomSpan
platform_identity_validate_u32_span_thunk(
    void *current,
    const void *lower_bound,
    const void *upper_bound,
    const RuntimeFirebloomType *type_context
);
PLATFORM_IDENTITY_STATUS_PUBLIC RuntimeFirebloomSpan
j_platform_identity_validate_u32_span(
    void *current,
    const void *lower_bound,
    const void *upper_bound,
    const RuntimeFirebloomType *type_context
);
PLATFORM_IDENTITY_STATUS_PUBLIC RuntimeFirebloomSpan
platform_identity_validate_u32_span(
    void *current,
    const void *lower_bound,
    const void *upper_bound,
    const RuntimeFirebloomType *type_context
);
PLATFORM_IDENTITY_STATUS_PUBLIC PlatformIdentityStatusBase
platform_identity_outlined_load_status_and_base_x19_w8(void);
PLATFORM_IDENTITY_STATUS_PUBLIC void platform_sha384_digest_bounded(
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
);
PLATFORM_IDENTITY_STATUS_PUBLIC void platform_sha384_digest_bounded_thunk_a(
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
);
PLATFORM_IDENTITY_STATUS_PUBLIC void platform_sha384_digest_bounded_thunk_b(
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
);
PLATFORM_IDENTITY_STATUS_PUBLIC uint32_t
platform_compute_chip_production_fuse_mode(void);
PLATFORM_IDENTITY_STATUS_PUBLIC uint16_t
platform_read_effective_production_mode(void);
