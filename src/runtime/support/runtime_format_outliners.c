#include "avp/runtime/support/runtime_format_outliners.h"

#include "avp/runtime/support/stack_guard.h"


static _Noreturn void runtime_format_outliner_contract_failure(void) {
    __builtin_trap();
    __builtin_unreachable();
}

uint8_t RUNTIME_FORMAT_OUTLINER_PUBLIC
runtime_format_update_length_modifier_flags(
    uint8_t current_flags,
    uint32_t modifier_present,
    uint8_t fallback_flags
) {
    const uint8_t high_flags = modifier_present != 0
        ? (uint8_t)(current_flags & UINT8_C(0x80))
        : (uint8_t)(fallback_flags & UINT8_C(0x80));
    return (uint8_t)(high_flags | (current_flags & UINT8_C(0x7f)));
}

uint64_t RUNTIME_FORMAT_OUTLINER_PUBLIC
runtime_format_stack_guard_load_to_x9(void) {
    return runtime_stack_guard();
}

void RUNTIME_FORMAT_OUTLINER_PUBLIC runtime_format_write_two_byte_scratch(
    RuntimeFormatOutput *output,
    const char scratch[2]
) {
    runtime_format_output_write(output, scratch, 2);
}

RuntimeFormatLiteralOutput RUNTIME_FORMAT_OUTLINER_PUBLIC
runtime_format_prepare_literal_output(
    RuntimeFormatOutput *output,
    const char *literal
) {
    if (output == NULL || literal == NULL) {
        runtime_format_outliner_contract_failure();
    }
    return (RuntimeFormatLiteralOutput){
        .output = output,
        .current = literal,
        .lower = literal,
    };
}

uint64_t RUNTIME_FORMAT_OUTLINER_PUBLIC runtime_format_consume_argument_to_x8(
    RuntimeFormatArgumentCursorState *state
) {
    if (state == NULL || state->argument_cursor == NULL) {
        runtime_format_outliner_contract_failure();
    }
    const uint64_t value = *state->argument_cursor;
    ++state->argument_cursor;
    return value;
}

void RUNTIME_FORMAT_OUTLINER_PUBLIC runtime_format_advance_argument_cursor(
    RuntimeFormatArgumentCursorState *state
) {
    if (state == NULL || state->argument_cursor == NULL) {
        runtime_format_outliner_contract_failure();
    }
    ++state->argument_cursor;
}

void RUNTIME_FORMAT_OUTLINER_PUBLIC runtime_format_write_fourteen_byte_scratch(
    RuntimeFormatOutput *output,
    const char scratch[14]
) {
    runtime_format_output_write(output, scratch, 14);
}

RuntimeFormatTuple RUNTIME_FORMAT_OUTLINER_PUBLIC
runtime_format_save_primary_tuple(
    uintptr_t current,
    uintptr_t lower,
    uintptr_t upper,
    uintptr_t type
) {
    return (RuntimeFormatTuple){
        .current = current,
        .lower = lower,
        .upper = upper,
        .type = type,
    };
}
