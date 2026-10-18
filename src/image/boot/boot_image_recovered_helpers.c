#include "avp/image/boot/boot_image_recovered_helpers.h"


extern uintptr_t __stack_chk_guard;

static const RuntimeFirebloomType boot_image_byte_type = {
    .identity = 1,
    .element_size = 1,
    .bytewise_copyable = true,
};

static _Noreturn void boot_image_helper_trap(void) {
    __builtin_trap();
    __builtin_unreachable();
}

static size_t boot_image_span_size(RuntimeFirebloomSpan span) {
    const uintptr_t current = (uintptr_t)span.current;
    const uintptr_t lower = (uintptr_t)span.lower_bound;
    const uintptr_t upper = (uintptr_t)span.upper_bound;
    if (current == 0 || lower == 0 || upper == 0 || current < lower ||
        current > upper) {
        boot_image_helper_trap();
    }
    return upper - current;
}

static RuntimeFirebloomSpan boot_image_validate_typed_span(
    RuntimeFirebloomSpan span,
    size_t required_size
) {
    const size_t available = boot_image_span_size(span);
    if (span.type == 0 || available < required_size) {
        boot_image_helper_trap();
    }
    return span;
}

const uint8_t *boot_image_expected_manifest_digest_callback(
    uint32_t property,
    const BootImageManifestDigestContext *context
) {
    (void)property;
    if (context == 0) {
        boot_image_expected_manifest_digest_bounds_fatal();
    }
    RuntimeFirebloomSpan span = boot_image_validate_typed_span(
        context->record,
        sizeof(BootImageManifestDigestRecord)
    );
    const BootImageManifestDigestRecord *record = span.current;
    return record->digest_valid != 0 ? record->digest : 0;
}

_Noreturn void boot_image_expected_manifest_digest_bounds_fatal(void) {
    boot_image_helper_trap();
}

RuntimeFirebloomSpan boot_img4_validate_byte_span(RuntimeFirebloomSpan span) {
    (void)boot_image_validate_typed_span(span, 0);
    if (!span.type->bytewise_copyable || span.type->element_size != 1) {
        boot_image_helper_trap();
    }
    return span;
}

RuntimeFirebloomSpan boot_img4_validate_byte_span_thunk_a(
    RuntimeFirebloomSpan span
) {
    return boot_img4_validate_byte_span(span);
}

RuntimeFirebloomSpan boot_img4_validate_byte_span_thunk_b(
    RuntimeFirebloomSpan span
) {
    return boot_img4_validate_byte_span(span);
}

RuntimeFirebloomSpan boot_img4_validate_byte_span_clone(
    RuntimeFirebloomSpan span
) {
    return boot_img4_validate_byte_span(span);
}

RuntimeFirebloomSpan boot_image_validate_source_registry_node_span(
    RuntimeFirebloomSpan span,
    size_t record_size
) {
    return boot_image_validate_typed_span(span, record_size);
}

RuntimeFirebloomSpan boot_image_validate_property_context_span(
    RuntimeFirebloomSpan span,
    size_t context_size
) {
    return boot_image_validate_typed_span(span, context_size);
}

uint64_t boot_image_stack_guard_load_to_x9(void) {
    return __stack_chk_guard;
}

uint64_t boot_image_stack_guard_load_to_x8(void) {
    return __stack_chk_guard;
}

BootImageBounds boot_image_outlined_restore_provider_descriptor_bounds(
    void *current,
    void *lower_bound,
    void *upper_bound
) {
    return (BootImageBounds){current, lower_bound, upper_bound};
}

BootImageConsumedLengthOutput
boot_img4_outlined_prepare_consumed_length_output(size_t *output) {
    if (output == 0) {
        boot_image_helper_trap();
    }
    *output = 0;
    return (BootImageConsumedLengthOutput){
        .current = output,
        .lower_bound = output,
        .upper_bound = output + 1,
        .type = &boot_image_byte_type,
    };
}

uint32_t boot_image_breadcrumb_code_base(void) {
    return BOOT_IMAGE_BREADCRUMB_CODE_BASE;
}

RuntimeFirebloomSpan boot_image_outlined_restore_output_span_x19(
    RuntimeFirebloomSpan span
) {
    return span;
}

uint32_t boot_image_property_breadcrumb_code_base(void) {
    return BOOT_IMAGE_PROPERTY_BREADCRUMB_CODE_BASE;
}

void boot_img4_outlined_clear_payload_type_output(
    RuntimeFirebloomSpan *output
) {
    if (output == 0) {
        boot_image_helper_trap();
    }
    *output = (RuntimeFirebloomSpan){0};
}

void boot_image_publish_sha384_digest_info(
    const void **slot,
    const void *digest_info
) {
    if (slot == 0 || digest_info == 0) {
        boot_image_helper_trap();
    }
    *slot = digest_info;
}

const RuntimeFirebloomType *boot_image_outlined_load_byte_type_to_x7(void) {
    return &boot_image_byte_type;
}

RuntimeFirebloomSpan boot_image_outlined_restore_header_storage_span(
    RuntimeFirebloomSpan span
) {
    return span;
}

BootImageBounds boot_image_outlined_save_provider_descriptor_bounds(
    void *current,
    void *lower_bound,
    void *upper_bound
) {
    return (BootImageBounds){current, lower_bound, upper_bound};
}

int boot_img4_compare_property_data(
    RuntimeFirebloomSpan left,
    RuntimeFirebloomSpan right
) {
    left = boot_img4_validate_byte_span(left);
    right = boot_img4_validate_byte_span(right);
    const size_t left_size = boot_image_span_size(left);
    const size_t right_size = boot_image_span_size(right);
    const size_t shared_size = left_size < right_size ? left_size : right_size;
    const uint8_t *left_bytes = left.current;
    const uint8_t *right_bytes = right.current;
    for (size_t index = 0; index < shared_size; ++index) {
        if (left_bytes[index] < right_bytes[index]) {
            return -1;
        }
        if (left_bytes[index] > right_bytes[index]) {
            return 1;
        }
    }
    return (left_size > right_size) - (left_size < right_size);
}

BootImageBooleanResult boot_image_property_outlined_set_true_and_clear_status(
    void
) {
    return (BootImageBooleanResult){.status = 0, .value = true};
}

uint32_t boot_image_property_breadcrumb_code_base_to_w19(void) {
    return BOOT_IMAGE_PROPERTY_BREADCRUMB_CODE_BASE;
}

RuntimeFirebloomSpan boot_image_outlined_restore_image_span_x23_x20(
    RuntimeFirebloomSpan span
) {
    return span;
}

RuntimeFirebloomSpan
boot_image_outlined_restore_span_upper_and_type_x28_x27(
    RuntimeFirebloomSpan span,
    void *upper_bound,
    const RuntimeFirebloomType *type
) {
    span.upper_bound = upper_bound;
    span.type = type;
    return span;
}
