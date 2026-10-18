#include "avp/runtime/boot/boot_arguments_outliners.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static RuntimeFirebloomType byte_type = {
    .identity = UINT64_C(0x100000001),
    .element_size = 1,
    .bytewise_copyable = true,
};

static void assert_span_equal(
    RuntimeFirebloomSpan left,
    RuntimeFirebloomSpan right
) {
    assert(left.current == right.current);
    assert(left.lower_bound == right.lower_bound);
    assert(left.upper_bound == right.upper_bound);
    assert(left.type == right.type);
}

int main(void) {
    uint8_t bytes[16] = {0};
    const RuntimeFirebloomSpan span = {
        .current = bytes + 3,
        .lower_bound = bytes,
        .upper_bound = bytes + sizeof(bytes),
        .type = &byte_type,
    };

    assert_span_equal(
        boot_arguments_outlined_restore_input_span_x27_x24(span), span);
    assert_span_equal(
        boot_arguments_outlined_save_input_span_x27_x24(span), span);
    assert_span_equal(
        boot_arguments_outlined_save_owned_span_x27_x20(span), span);
    assert_span_equal(
        boot_arguments_outlined_save_input_span_x27_x24_clone(span), span);
    assert_span_equal(
        boot_arguments_outlined_restore_token_span_x20_x25(span), span);

    const RuntimeFirebloomSpan cleared =
        boot_arguments_outlined_clear_result_span_x27(NULL);
    assert(cleared.current == NULL);
    assert(cleared.lower_bound == NULL);
    assert(cleared.upper_bound == NULL);
    assert(cleared.type == NULL);

    assert_span_equal(
        boot_arguments_outlined_restore_owned_span_suffix_x21_x23(
            span.current, span.lower_bound, span.upper_bound, span.type),
        span);

    size_t key_length = 4;
    size_t following_word = 9;
    const BootArgumentsLengthOutputBounds output =
        boot_arguments_outlined_prepare_key_length_output_bounds(
            &key_length, &following_word);
    assert(output.current == &key_length);
    assert(output.lower_bound == &key_length);
    assert(output.upper_bound == &following_word);

    BootArgumentsFrame90 frame = {0};
    for (size_t index = 0; index < 10; index++) {
        frame.lanes[index] = UINT64_C(0x100) + index;
    }
    const BootArgumentsFrame90 saved =
        boot_arguments_outlined_frame90_prologue(frame);
    const BootArgumentsFrame90 restored =
        boot_arguments_outlined_frame90_epilogue(saved);
    for (size_t index = 0; index < 10; index++) {
        assert(restored.lanes[index] == UINT64_C(0x100) + index);
    }

    puts("boot arguments outliner tests passed");
    return 0;
}
