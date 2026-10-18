#include "avp/image/boot/boot_image_recovered_helpers.h"

#include <assert.h>
#include <signal.h>
#include <stdint.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>


uintptr_t __stack_chk_guard = UINT64_C(0x55aa55aa12344321);

static const RuntimeFirebloomType byte_type = {
    .identity = 1,
    .element_size = 1,
    .bytewise_copyable = true,
};

static const RuntimeFirebloomType record_type = {
    .identity = 2,
    .element_size = 1,
    .bytewise_copyable = false,
};

static RuntimeFirebloomSpan span_for(
    void *bytes,
    size_t size,
    const RuntimeFirebloomType *type
) {
    return (RuntimeFirebloomSpan){
        .current = bytes,
        .lower_bound = bytes,
        .upper_bound = (uint8_t *)bytes + size,
        .type = type,
    };
}

static void expect_trap(void (*operation)(void)) {
    const pid_t child = fork();
    assert(child >= 0);
    if (child == 0) {
        operation();
        _exit(0);
    }
    int status = 0;
    assert(waitpid(child, &status, 0) == child);
    assert(WIFSIGNALED(status));
    assert(WTERMSIG(status) == SIGILL || WTERMSIG(status) == SIGTRAP);
}

static void invalid_byte_span(void) {
    uint8_t value = 0;
    (void)boot_img4_validate_byte_span(span_for(&value, 1, &record_type));
}

static void invalid_digest_bounds(void) {
    BootImageManifestDigestRecord record = {0};
    BootImageManifestDigestContext context = {
        .record = span_for(&record, sizeof(record) - 1, &record_type),
    };
    (void)boot_image_expected_manifest_digest_callback(0, &context);
}

static void test_digest_and_bounds(void) {
    BootImageManifestDigestRecord record = {0};
    for (size_t index = 0; index < sizeof(record.digest); ++index) {
        record.digest[index] = (uint8_t)(index + 1);
    }
    BootImageManifestDigestContext context = {
        .record = span_for(&record, sizeof(record), &record_type),
    };
    assert(boot_image_expected_manifest_digest_callback(0, &context) == 0);
    record.digest_valid = 1;
    assert(boot_image_expected_manifest_digest_callback(0, &context) ==
           record.digest);
    RuntimeFirebloomSpan validated =
        boot_image_validate_source_registry_node_span(context.record, 0x40);
    assert(validated.current == &record);
    validated = boot_image_validate_property_context_span(
        context.record,
        sizeof(record)
    );
    assert(validated.upper_bound == (uint8_t *)&record + sizeof(record));
}

static void test_span_helpers(void) {
    uint8_t left[] = {1, 2, 3};
    uint8_t right[] = {1, 2, 4};
    RuntimeFirebloomSpan left_span = span_for(left, sizeof(left), &byte_type);
    RuntimeFirebloomSpan right_span = span_for(
        right,
        sizeof(right),
        &byte_type
    );
    assert(boot_img4_validate_byte_span(left_span).current == left);
    assert(boot_img4_validate_byte_span_thunk_a(left_span).current == left);
    assert(boot_img4_validate_byte_span_thunk_b(left_span).current == left);
    assert(boot_img4_validate_byte_span_clone(left_span).current == left);
    assert(boot_img4_compare_property_data(left_span, right_span) < 0);
    assert(boot_img4_compare_property_data(right_span, left_span) > 0);
    right[2] = 3;
    assert(boot_img4_compare_property_data(left_span, right_span) == 0);

    RuntimeFirebloomSpan restored =
        boot_image_outlined_restore_output_span_x19(left_span);
    assert(restored.current == left);
    restored = boot_image_outlined_restore_header_storage_span(left_span);
    assert(restored.lower_bound == left);
    restored = boot_image_outlined_restore_image_span_x23_x20(left_span);
    assert(restored.upper_bound == left + sizeof(left));
    restored = boot_image_outlined_restore_span_upper_and_type_x28_x27(
        left_span,
        left + 2,
        &record_type
    );
    assert(restored.upper_bound == left + 2);
    assert(restored.type == &record_type);

    RuntimeFirebloomSpan cleared = left_span;
    boot_img4_outlined_clear_payload_type_output(&cleared);
    assert(cleared.current == 0 && cleared.type == 0);
}

static void test_scalar_and_publication_helpers(void) {
    assert(boot_image_stack_guard_load_to_x8() == __stack_chk_guard);
    assert(boot_image_stack_guard_load_to_x9() == __stack_chk_guard);
    assert(boot_image_breadcrumb_code_base() == UINT32_C(0x40040026));
    assert(boot_image_property_breadcrumb_code_base() ==
           UINT32_C(0x40040011));
    assert(boot_image_property_breadcrumb_code_base_to_w19() ==
           UINT32_C(0x40040011));
    assert(boot_image_outlined_load_byte_type_to_x7()->element_size == 1);

    size_t consumed = 17;
    BootImageConsumedLengthOutput output =
        boot_img4_outlined_prepare_consumed_length_output(&consumed);
    assert(consumed == 0);
    assert(output.current == &consumed);
    assert(output.upper_bound == &consumed + 1);

    int descriptor = 0;
    const void *slot = 0;
    boot_image_publish_sha384_digest_info(&slot, &descriptor);
    assert(slot == &descriptor);
    BootImageBounds bounds =
        boot_image_outlined_save_provider_descriptor_bounds(
            &descriptor,
            &descriptor,
            &descriptor + 1
        );
    assert(bounds.current == &descriptor);
    bounds = boot_image_outlined_restore_provider_descriptor_bounds(
        bounds.current,
        bounds.lower_bound,
        bounds.upper_bound
    );
    assert(bounds.upper_bound == &descriptor + 1);
    const BootImageBooleanResult result =
        boot_image_property_outlined_set_true_and_clear_status();
    assert(result.status == 0 && result.value);
}

int main(void) {
    test_digest_and_bounds();
    test_span_helpers();
    test_scalar_and_publication_helpers();
    expect_trap(invalid_byte_span);
    expect_trap(invalid_digest_bounds);
}
