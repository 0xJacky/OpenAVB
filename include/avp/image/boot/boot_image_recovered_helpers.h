#pragma once

#include "avp/runtime/firebloom/runtime_firebloom.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


#define BOOT_IMAGE_HELPER_PUBLIC __attribute__((used, retain))

enum {
    BOOT_IMAGE_MANIFEST_DIGEST_SIZE = 48,
    BOOT_IMAGE_BREADCRUMB_CODE_BASE = 0x40040026,
    BOOT_IMAGE_PROPERTY_BREADCRUMB_CODE_BASE = 0x40040011,
};

typedef struct {
    void *current;
    void *lower_bound;
    void *upper_bound;
} BootImageBounds;

typedef struct {
    uint8_t reserved[0x6e];
    uint8_t digest_valid;
    uint8_t digest[BOOT_IMAGE_MANIFEST_DIGEST_SIZE];
} BootImageManifestDigestRecord;

typedef struct {
    uint8_t reserved[0x20];
    RuntimeFirebloomSpan record;
} BootImageManifestDigestContext;

typedef struct {
    size_t *current;
    size_t *lower_bound;
    size_t *upper_bound;
    const RuntimeFirebloomType *type;
} BootImageConsumedLengthOutput;

typedef struct {
    uint32_t status;
    bool value;
} BootImageBooleanResult;

BOOT_IMAGE_HELPER_PUBLIC const uint8_t *
boot_image_expected_manifest_digest_callback(
    uint32_t property,
    const BootImageManifestDigestContext *context
);
BOOT_IMAGE_HELPER_PUBLIC _Noreturn void
boot_image_expected_manifest_digest_bounds_fatal(void);
BOOT_IMAGE_HELPER_PUBLIC RuntimeFirebloomSpan
boot_img4_validate_byte_span(RuntimeFirebloomSpan span);
BOOT_IMAGE_HELPER_PUBLIC RuntimeFirebloomSpan
boot_img4_validate_byte_span_thunk_a(RuntimeFirebloomSpan span);
BOOT_IMAGE_HELPER_PUBLIC RuntimeFirebloomSpan
boot_img4_validate_byte_span_thunk_b(RuntimeFirebloomSpan span);
BOOT_IMAGE_HELPER_PUBLIC RuntimeFirebloomSpan
boot_img4_validate_byte_span_clone(RuntimeFirebloomSpan span);
BOOT_IMAGE_HELPER_PUBLIC RuntimeFirebloomSpan
boot_image_validate_source_registry_node_span(
    RuntimeFirebloomSpan span,
    size_t record_size
);
BOOT_IMAGE_HELPER_PUBLIC RuntimeFirebloomSpan
boot_image_validate_property_context_span(
    RuntimeFirebloomSpan span,
    size_t context_size
);
BOOT_IMAGE_HELPER_PUBLIC uint64_t boot_image_stack_guard_load_to_x9(void);
BOOT_IMAGE_HELPER_PUBLIC uint64_t boot_image_stack_guard_load_to_x8(void);
BOOT_IMAGE_HELPER_PUBLIC BootImageBounds
boot_image_outlined_restore_provider_descriptor_bounds(
    void *current,
    void *lower_bound,
    void *upper_bound
);
BOOT_IMAGE_HELPER_PUBLIC BootImageConsumedLengthOutput
boot_img4_outlined_prepare_consumed_length_output(size_t *output);
BOOT_IMAGE_HELPER_PUBLIC uint32_t boot_image_breadcrumb_code_base(void);
BOOT_IMAGE_HELPER_PUBLIC RuntimeFirebloomSpan
boot_image_outlined_restore_output_span_x19(RuntimeFirebloomSpan span);
BOOT_IMAGE_HELPER_PUBLIC uint32_t
boot_image_property_breadcrumb_code_base(void);
BOOT_IMAGE_HELPER_PUBLIC void boot_img4_outlined_clear_payload_type_output(
    RuntimeFirebloomSpan *output
);
BOOT_IMAGE_HELPER_PUBLIC void boot_image_publish_sha384_digest_info(
    const void **slot,
    const void *digest_info
);
BOOT_IMAGE_HELPER_PUBLIC const RuntimeFirebloomType *
boot_image_outlined_load_byte_type_to_x7(void);
BOOT_IMAGE_HELPER_PUBLIC RuntimeFirebloomSpan
boot_image_outlined_restore_header_storage_span(RuntimeFirebloomSpan span);
BOOT_IMAGE_HELPER_PUBLIC BootImageBounds
boot_image_outlined_save_provider_descriptor_bounds(
    void *current,
    void *lower_bound,
    void *upper_bound
);
BOOT_IMAGE_HELPER_PUBLIC int boot_img4_compare_property_data(
    RuntimeFirebloomSpan left,
    RuntimeFirebloomSpan right
);
BOOT_IMAGE_HELPER_PUBLIC BootImageBooleanResult
boot_image_property_outlined_set_true_and_clear_status(void);
BOOT_IMAGE_HELPER_PUBLIC uint32_t
boot_image_property_breadcrumb_code_base_to_w19(void);
BOOT_IMAGE_HELPER_PUBLIC RuntimeFirebloomSpan
boot_image_outlined_restore_image_span_x23_x20(RuntimeFirebloomSpan span);
BOOT_IMAGE_HELPER_PUBLIC RuntimeFirebloomSpan
boot_image_outlined_restore_span_upper_and_type_x28_x27(
    RuntimeFirebloomSpan span,
    void *upper_bound,
    const RuntimeFirebloomType *type
);
