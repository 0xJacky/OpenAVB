#include "avp/runtime/boot/boot_arguments_outliners.h"

RuntimeFirebloomSpan boot_arguments_outlined_restore_input_span_x27_x24(
    RuntimeFirebloomSpan input
) {
    return input;
}

RuntimeFirebloomSpan boot_arguments_outlined_clear_result_span_x27(
    void *anchor
) {
    return (RuntimeFirebloomSpan){
        .current = anchor,
        .lower_bound = anchor,
        .upper_bound = anchor,
        .type = (const RuntimeFirebloomType *)anchor,
    };
}

RuntimeFirebloomSpan boot_arguments_outlined_restore_owned_span_suffix_x21_x23(
    void *current,
    void *lower_bound,
    void *upper_bound,
    const RuntimeFirebloomType *type
) {
    return (RuntimeFirebloomSpan){
        .current = current,
        .lower_bound = lower_bound,
        .upper_bound = upper_bound,
        .type = type,
    };
}

BootArgumentsLengthOutputBounds
boot_arguments_outlined_prepare_key_length_output_bounds(
    size_t *current,
    size_t *upper_bound
) {
    return (BootArgumentsLengthOutputBounds){
        .current = current,
        .lower_bound = current,
        .upper_bound = upper_bound,
    };
}

RuntimeFirebloomSpan boot_arguments_outlined_save_input_span_x27_x24(
    RuntimeFirebloomSpan input
) {
    return input;
}

RuntimeFirebloomSpan boot_arguments_outlined_save_owned_span_x27_x20(
    RuntimeFirebloomSpan owned
) {
    return owned;
}

RuntimeFirebloomSpan boot_arguments_outlined_save_input_span_x27_x24_clone(
    RuntimeFirebloomSpan input
) {
    return input;
}

RuntimeFirebloomSpan boot_arguments_outlined_restore_token_span_x20_x25(
    RuntimeFirebloomSpan token
) {
    return token;
}

BootArgumentsFrame90 boot_arguments_outlined_frame90_epilogue(
    BootArgumentsFrame90 frame
) {
    return frame;
}

BootArgumentsFrame90 boot_arguments_outlined_frame90_prologue(
    BootArgumentsFrame90 frame
) {
    return frame;
}
