#include "avp/runtime/support/runtime_format_compat.h"

#include <assert.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct {
    char bytes[512];
    size_t length;
} TestSink;

static void sink_write(void *context, const char *bytes, size_t length) {
    TestSink *sink = context;
    assert(length <= sizeof(sink->bytes) - sink->length - 1);
    memcpy(sink->bytes + sink->length, bytes, length);
    sink->length += length;
    sink->bytes[sink->length] = '\0';
}

static int call_vsnprintf(char *output, size_t capacity, const char *format,
                          ...) {
    va_list arguments;
    va_start(arguments, format);
    const int result =
        runtime_vsnprintf(output, capacity, format, arguments);
    va_end(arguments);
    return result;
}

static int call_vsnprintf_chk(char *output, size_t capacity,
                              size_t object_size, const char *format, ...) {
    va_list arguments;
    va_start(arguments, format);
    const int result = runtime_vsnprintf_chk(
        output, capacity, 0, object_size, format, arguments);
    va_end(arguments);
    return result;
}

static int call_render(RuntimeFormatOutput *output, const char *format, ...) {
    va_list arguments;
    va_start(arguments, format);
    const int result =
        runtime_format_render_conversion(output, format, arguments);
    va_end(arguments);
    return result;
}

static void test_fixed_format_vectors(void) {
    char expected[256];
    char actual[256];
    const int expected_length = snprintf(
        expected, sizeof(expected), "%#08x|%+7d|%-6u|%.4s|%hhu|%zu|%%",
        UINT32_C(0x1a2b), -91, 77u, "abcdef", 257, (size_t)4097);
    const int actual_length = runtime_firebloom_snprintf(
        actual, sizeof(actual), "%#08x|%+7d|%-6u|%.4s|%hhu|%zu|%%",
        UINT32_C(0x1a2b), -91, 77u, "abcdef", 257, (size_t)4097);
    assert(actual_length == expected_length);
    assert(strcmp(actual, expected) == 0);

    char truncated[9];
    assert(runtime_firebloom_snprintf(truncated, sizeof(truncated),
                                      "0123456789abcdef") == 16);
    assert(strcmp(truncated, "01234567") == 0);
    assert(call_vsnprintf(actual, sizeof(actual), "%llX:%s",
                          UINT64_C(0x123456789abcdef0), "done") == 21);
    assert(strcmp(actual, "123456789ABCDEF0:done") == 0);
    assert(call_vsnprintf_chk(actual, sizeof(actual), sizeof(actual), "%u",
                              UINT32_C(42)) == 2);
    assert(strcmp(actual, "42") == 0);
    assert(runtime_firebloom_snprintf(actual, sizeof(actual),
                                      "A=%f B=%d", 1.5, 7) == 13);
    assert(strcmp(actual, "A=<float> B=7") == 0);
    int written = -1;
    assert(runtime_firebloom_snprintf(actual, sizeof(actual), "abc%n:%d",
                                      &written, 9) == 5);
    assert(written == 3);
    assert(strcmp(actual, "abc:9") == 0);
}

static uint64_t next_word(uint64_t *state) {
    uint64_t value = *state;
    value ^= value << 13;
    value ^= value >> 7;
    value ^= value << 17;
    *state = value;
    return value;
}

static void test_randomized_libc_oracle(void) {
    uint64_t random = UINT64_C(0x6e616e6f7072696e);
    for (size_t iteration = 0; iteration < 2048; ++iteration) {
        const uint64_t unsigned_value = next_word(&random);
        const int64_t signed_value = (int64_t)next_word(&random);
        const int width = 1 + (int)(next_word(&random) % 24);
        const int precision = (int)(next_word(&random) % 18);
        char expected[256];
        char actual[256];
        const int expected_length = snprintf(
            expected, sizeof(expected), "%#0*llx|%+*.*lld|%-*u", width,
            (unsigned long long)unsigned_value, width, precision,
            (long long)signed_value, width, (unsigned)unsigned_value);
        const int actual_length = runtime_firebloom_snprintf(
            actual, sizeof(actual), "%#0*llx|%+*.*lld|%-*u", width,
            (unsigned long long)unsigned_value, width, precision,
            (long long)signed_value, width, (unsigned)unsigned_value);
        assert(actual_length == expected_length);
        assert(strcmp(actual, expected) == 0);
    }
}

static void test_hex_encoder_oracle(void) {
    uint64_t random = UINT64_C(0x4845582d4f524143);
    for (size_t iteration = 0; iteration < 1024; ++iteration) {
        uint8_t input[64];
        const size_t length = next_word(&random) % sizeof(input);
        for (size_t index = 0; index < length; ++index) {
            input[index] = (uint8_t)next_word(&random);
        }
        char actual[sizeof(input) * 2 + 1];
        char expected[sizeof(actual)];
        for (size_t index = 0; index < length; ++index) {
            snprintf(expected + index * 2, 3, "%02X", input[index]);
        }
        expected[length * 2] = '\0';
        assert(runtime_bounded_hex_encode_uppercase(
                   actual, sizeof(actual), input, length) == length * 2);
        assert(strcmp(actual, expected) == 0);
    }
    assert(runtime_bounded_hex_encode_source_token() ==
           UINT64_C(0x0eff478cd85e2305));
}

static void test_output_and_console_paths(void) {
    TestSink sink = {0};
    runtime_format_bind_console(sink_write, &sink);
    assert(runtime_printf("code=%#x/%s", UINT32_C(0x52), "ok") == 12);
    assert(strcmp(sink.bytes, "code=0x52/ok") == 0);

    TestSink conversion_sink = {0};
    RuntimeFormatOutput output = {
        .context = &conversion_sink,
        .write = sink_write,
    };
    assert(call_render(&output, "%08X", UINT32_C(0x1a2b)) == 8);
    assert(strcmp(conversion_sink.bytes, "00001A2B") == 0);
    assert(output.emitted == 8);

    conversion_sink = (TestSink){0};
    output.emitted = 0;
    assert(call_render(&output, "A=%*.*f B=%d", 8, 3, 1.5, 7) == 13);
    assert(strcmp(conversion_sink.bytes, "A=<float> B=7") == 0);

    char bounded[5] = {0};
    RuntimeFormatBufferContext buffer_context = {
        .buffer = bounded,
        .capacity = sizeof(bounded),
    };
    runtime_format_buffer_write(&buffer_context, "abcdefgh", 8);
    assert(buffer_context.used == sizeof(bounded));
    assert(memcmp(bounded, "abcde", sizeof(bounded)) == 0);
    assert(runtime_format_buffer_context_validate(&buffer_context));
    assert(runtime_format_validate_byte_span(0, 0));
}

static void test_field_parsers(void) {
    RuntimeFormatField field = {.valid = true};
    assert(runtime_format_parse_field_from_argument(&field, 17) == 17);
    assert(field.valid);
    assert(runtime_format_parse_field_from_argument(&field, -1) == 0);
    assert(!field.valid);

    field.valid = true;
    const char *digits = "2147483648tail";
    const char *cursor = digits;
    assert(runtime_format_parse_decimal_field(
               &field, &cursor, digits + strlen(digits)) ==
           UINT32_C(2147483648));
    assert(cursor == digits + 10);
    assert(!field.valid);
}

static void test_padding_and_integer_helpers(void) {
    TestSink sink = {0};
    RuntimeFormatOutput output = {.context = &sink, .write = sink_write};
    RuntimeFormatField field = {
        .width = 12,
        .precision = 6,
        .precision_specified = true,
        .valid = true,
        .zero_padded = true,
        .alternate_form = true,
    };
    runtime_format_write_integer_conversion(&output, UINT64_C(0x12ab), false,
                                            16, false, &field);
    char expected[64];
    snprintf(expected, sizeof(expected), "%#012.6x", UINT32_C(0x12ab));
    assert(strcmp(sink.bytes, expected) == 0);

    sink = (TestSink){0};
    output.emitted = 0;
    field = (RuntimeFormatField){
        .width = 9,
        .valid = true,
        .left_aligned = true,
    };
    runtime_format_write_padded_span(&output, "abc", 3, &field);
    assert(strcmp(sink.bytes, "abc      ") == 0);
    runtime_format_write_padding(&output, '.', 19);
    assert(strcmp(sink.bytes, "abc      ...................") == 0);
}

static void trap_checked_size(void) {
    char output[4];
    (void)call_vsnprintf_chk(output, 5, sizeof(output), "%s", "x");
}

static void trap_null_field(void) {
    runtime_format_update_field_validity(0, 1);
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

static void test_contract_traps(void) {
    expect_trap(trap_checked_size);
    expect_trap(trap_null_field);
}

int main(void) {
    test_fixed_format_vectors();
    test_randomized_libc_oracle();
    test_hex_encoder_oracle();
    test_output_and_console_paths();
    test_field_parsers();
    test_padding_and_integer_helpers();
    test_contract_traps();
    return 0;
}
