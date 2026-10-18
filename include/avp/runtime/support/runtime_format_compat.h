#pragma once

#include <stdbool.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

typedef void (*RuntimeFormatWrite)(
    void *context,
    const char *bytes,
    size_t length
);

typedef struct {
    void *context;
    RuntimeFormatWrite write;
    size_t emitted;
} RuntimeFormatOutput;

typedef struct {
    char *buffer;
    size_t capacity;
    size_t used;
} RuntimeFormatBufferContext;

typedef struct {
    int32_t width;
    int32_t precision;
    bool precision_specified;
    bool valid;
    bool left_aligned;
    bool zero_padded;
    bool alternate_form;
    bool force_sign;
    bool leading_space;
} RuntimeFormatField;

#define RUNTIME_FORMAT_PUBLIC __attribute__((used, retain))

void runtime_format_bind_console(RuntimeFormatWrite write, void *context);
RUNTIME_FORMAT_PUBLIC uint64_t runtime_bounded_hex_encode_source_token(void);
RUNTIME_FORMAT_PUBLIC size_t runtime_bounded_hex_encode_uppercase(
    char *output,
    size_t output_capacity,
    const uint8_t *input,
    size_t input_length
);
RUNTIME_FORMAT_PUBLIC int runtime_vprintf(const char *format, va_list arguments);
RUNTIME_FORMAT_PUBLIC int runtime_vprintf_with_options(
    uint32_t options,
    const char *format,
    va_list arguments
);
RUNTIME_FORMAT_PUBLIC int runtime_firebloom_vsnprintf(
    char *output,
    size_t capacity,
    const char *format,
    va_list arguments
);
RUNTIME_FORMAT_PUBLIC int runtime_firebloom_vsnprintf_with_options(
    char *output,
    size_t capacity,
    uint32_t options,
    const char *format,
    va_list arguments
);
RUNTIME_FORMAT_PUBLIC int runtime_printf(const char *format, ...)
    __attribute__((format(printf, 1, 2)));
RUNTIME_FORMAT_PUBLIC int runtime_firebloom_snprintf(
    char *output,
    size_t capacity,
    const char *format,
    ...
)
    __attribute__((format(printf, 3, 4)));
RUNTIME_FORMAT_PUBLIC int runtime_vsnprintf(
    char *output,
    size_t capacity,
    const char *format,
    va_list arguments
);
RUNTIME_FORMAT_PUBLIC int runtime_format_vwrite(
    RuntimeFormatOutput *output,
    const char *format,
    va_list arguments
);
RUNTIME_FORMAT_PUBLIC void runtime_format_console_write(
    void *context,
    const char *bytes,
    size_t length
);
RUNTIME_FORMAT_PUBLIC void runtime_format_output_write(
    RuntimeFormatOutput *output,
    const char *bytes,
    size_t length
);
RUNTIME_FORMAT_PUBLIC bool runtime_format_buffer_context_validate(
    const RuntimeFormatBufferContext *context);
RUNTIME_FORMAT_PUBLIC bool runtime_format_validate_byte_span(
    const char *bytes,
    size_t length
);
RUNTIME_FORMAT_PUBLIC void runtime_format_buffer_write(
    void *context,
    const char *bytes,
    size_t length
);
RUNTIME_FORMAT_PUBLIC int runtime_format_render_conversion(
    RuntimeFormatOutput *output,
    const char *conversion,
    va_list arguments
);
RUNTIME_FORMAT_PUBLIC void runtime_format_update_field_validity(
    RuntimeFormatField *field,
    int64_t value
);
RUNTIME_FORMAT_PUBLIC _Noreturn void runtime_format_null_state_panic(void);
RUNTIME_FORMAT_PUBLIC uint32_t runtime_format_parse_field_from_argument(
    RuntimeFormatField *field,
    int32_t value
);
RUNTIME_FORMAT_PUBLIC uint32_t runtime_format_parse_decimal_field(
    RuntimeFormatField *field,
    const char **cursor,
    const char *upper_bound
);
RUNTIME_FORMAT_PUBLIC void runtime_format_write_integer_conversion(
    RuntimeFormatOutput *output,
    uint64_t value,
    bool negative,
    unsigned base,
    bool uppercase,
    const RuntimeFormatField *field
);
RUNTIME_FORMAT_PUBLIC void runtime_format_write_padded_span(
    RuntimeFormatOutput *output,
    const char *bytes,
    size_t length,
    const RuntimeFormatField *field
);
RUNTIME_FORMAT_PUBLIC void runtime_format_write_padding(
    RuntimeFormatOutput *output,
    char byte,
    size_t count
);
RUNTIME_FORMAT_PUBLIC int runtime_vsnprintf_chk(
    char *output,
    size_t capacity,
    int flags,
    size_t object_size,
    const char *format,
    va_list arguments
);
RUNTIME_FORMAT_PUBLIC _Noreturn void
runtime_vsnprintf_object_size_fatal(size_t capacity, size_t object_size);
