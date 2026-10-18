#pragma once

#include "avp/runtime/firebloom/runtime_firebloom.h"

#include <stddef.h>
#include <stdint.h>

#define BOOT_ARGUMENTS_OUTLINER_PUBLIC __attribute__((used, retain))

typedef struct {
    uintptr_t lanes[10];
} BootArgumentsFrame90;

typedef struct {
    size_t *current;
    size_t *lower_bound;
    size_t *upper_bound;
} BootArgumentsLengthOutputBounds;

BOOT_ARGUMENTS_OUTLINER_PUBLIC RuntimeFirebloomSpan
boot_arguments_outlined_restore_input_span_x27_x24(
    RuntimeFirebloomSpan input
);
BOOT_ARGUMENTS_OUTLINER_PUBLIC RuntimeFirebloomSpan
boot_arguments_outlined_clear_result_span_x27(void *anchor);
BOOT_ARGUMENTS_OUTLINER_PUBLIC RuntimeFirebloomSpan
boot_arguments_outlined_restore_owned_span_suffix_x21_x23(
    void *current,
    void *lower_bound,
    void *upper_bound,
    const RuntimeFirebloomType *type
);
BOOT_ARGUMENTS_OUTLINER_PUBLIC BootArgumentsLengthOutputBounds
boot_arguments_outlined_prepare_key_length_output_bounds(
    size_t *current,
    size_t *upper_bound
);
BOOT_ARGUMENTS_OUTLINER_PUBLIC RuntimeFirebloomSpan
boot_arguments_outlined_save_input_span_x27_x24(RuntimeFirebloomSpan input);
BOOT_ARGUMENTS_OUTLINER_PUBLIC RuntimeFirebloomSpan
boot_arguments_outlined_save_owned_span_x27_x20(RuntimeFirebloomSpan owned);
BOOT_ARGUMENTS_OUTLINER_PUBLIC RuntimeFirebloomSpan
boot_arguments_outlined_save_input_span_x27_x24_clone(
    RuntimeFirebloomSpan input
);
BOOT_ARGUMENTS_OUTLINER_PUBLIC RuntimeFirebloomSpan
boot_arguments_outlined_restore_token_span_x20_x25(
    RuntimeFirebloomSpan token
);
BOOT_ARGUMENTS_OUTLINER_PUBLIC BootArgumentsFrame90
boot_arguments_outlined_frame90_epilogue(BootArgumentsFrame90 frame);
BOOT_ARGUMENTS_OUTLINER_PUBLIC BootArgumentsFrame90
boot_arguments_outlined_frame90_prologue(BootArgumentsFrame90 frame);
