#pragma once

#include "avp/runtime/support/runtime_format_compat.h"

#include <stddef.h>
#include <stdint.h>


#define RUNTIME_FORMAT_OUTLINER_PUBLIC __attribute__((used, retain))

typedef struct {
    RuntimeFormatOutput *output;
    const char *current;
    const char *lower;
} RuntimeFormatLiteralOutput;

typedef struct {
    uintptr_t current;
    uintptr_t lower;
    uintptr_t upper;
    uintptr_t type;
} RuntimeFormatTuple;

typedef struct {
    uint8_t reserved[0x58];
    const uint64_t *argument_cursor;
} RuntimeFormatArgumentCursorState;

_Static_assert(
    offsetof(RuntimeFormatArgumentCursorState, argument_cursor) == 0x58,
    "format argument cursor offset"
);

RUNTIME_FORMAT_OUTLINER_PUBLIC uint8_t
runtime_format_update_length_modifier_flags(
    uint8_t current_flags,
    uint32_t modifier_present,
    uint8_t fallback_flags
);
RUNTIME_FORMAT_OUTLINER_PUBLIC uint64_t
runtime_format_stack_guard_load_to_x9(void);
RUNTIME_FORMAT_OUTLINER_PUBLIC void runtime_format_write_two_byte_scratch(
    RuntimeFormatOutput *output,
    const char scratch[2]
);
RUNTIME_FORMAT_OUTLINER_PUBLIC RuntimeFormatLiteralOutput
runtime_format_prepare_literal_output(
    RuntimeFormatOutput *output,
    const char *literal
);
RUNTIME_FORMAT_OUTLINER_PUBLIC uint64_t runtime_format_consume_argument_to_x8(
    RuntimeFormatArgumentCursorState *state
);
RUNTIME_FORMAT_OUTLINER_PUBLIC void runtime_format_advance_argument_cursor(
    RuntimeFormatArgumentCursorState *state
);
RUNTIME_FORMAT_OUTLINER_PUBLIC void runtime_format_write_fourteen_byte_scratch(
    RuntimeFormatOutput *output,
    const char scratch[14]
);
RUNTIME_FORMAT_OUTLINER_PUBLIC RuntimeFormatTuple
runtime_format_save_primary_tuple(
    uintptr_t current,
    uintptr_t lower,
    uintptr_t upper,
    uintptr_t type
);
