#include "avp/runtime/support/runtime_format_outliners.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>


static uint64_t active_guard = UINT64_C(0x6f75746c696e6572);

uint64_t runtime_stack_guard(void) {
    return active_guard;
}

typedef struct {
    char bytes[32];
    size_t length;
} Sink;

static void sink_write(void *context, const char *bytes, size_t length) {
    Sink *sink = context;
    assert(length <= sizeof(sink->bytes) - sink->length);
    memcpy(sink->bytes + sink->length, bytes, length);
    sink->length += length;
}

int main(void) {
    assert(runtime_format_update_length_modifier_flags(0x35, 1, 0x80) ==
           0x35);
    assert(runtime_format_update_length_modifier_flags(0x35, 0, 0x80) ==
           0xb5);
    assert(runtime_format_update_length_modifier_flags(0xb5, 0, 0) == 0x35);
    assert(runtime_format_stack_guard_load_to_x9() == active_guard);

    Sink sink = {0};
    RuntimeFormatOutput output = {
        .context = &sink,
        .write = sink_write,
    };
    runtime_format_write_two_byte_scratch(&output, "AB");
    runtime_format_write_fourteen_byte_scratch(&output, "CDEFGHIJKLMNOP");
    assert(output.emitted == 16);
    assert(sink.length == 16);
    assert(memcmp(sink.bytes, "ABCDEFGHIJKLMNOP", 16) == 0);

    const char literal[] = "literal";
    const RuntimeFormatLiteralOutput prepared =
        runtime_format_prepare_literal_output(&output, literal);
    assert(prepared.output == &output);
    assert(prepared.current == literal);
    assert(prepared.lower == literal);

    const uint64_t arguments[] = {
        UINT64_C(0x1122334455667788),
        UINT64_C(0x99aabbccddeeff00),
        UINT64_C(0x0123456789abcdef),
    };
    RuntimeFormatArgumentCursorState state = {
        .argument_cursor = arguments,
    };
    assert(runtime_format_consume_argument_to_x8(&state) == arguments[0]);
    assert(state.argument_cursor == arguments + 1);
    runtime_format_advance_argument_cursor(&state);
    assert(state.argument_cursor == arguments + 2);
    assert(runtime_format_consume_argument_to_x8(&state) == arguments[2]);

    const RuntimeFormatTuple tuple = runtime_format_save_primary_tuple(
        UINT64_C(1), UINT64_C(2), UINT64_C(3), UINT64_C(4));
    assert(tuple.current == 1);
    assert(tuple.lower == 2);
    assert(tuple.upper == 3);
    assert(tuple.type == 4);
    return 0;
}
