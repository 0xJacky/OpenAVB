#include "avp/runtime/support/runtime_format_compat.h"

#define NANOPRINTF_IMPLEMENTATION
#define NANOPRINTF_USE_FIELD_WIDTH_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_PRECISION_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_FLOAT_FORMAT_SPECIFIERS 0
#define NANOPRINTF_USE_LARGE_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_SMALL_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_BINARY_FORMAT_SPECIFIERS 0
#define NANOPRINTF_USE_WRITEBACK_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_ALT_FORM_FLAG 1
#define NANOPRINTF_USE_FLOAT_SINGLE_PRECISION 0
#include "nanoprintf/nanoprintf.h"

static RuntimeFormatOutput console_output;

static _Noreturn void runtime_format_contract_failure(void) {
    __builtin_trap();
    __builtin_unreachable();
}

static size_t smaller_size(size_t left, size_t right) {
    return left < right ? left : right;
}

static void nanoprintf_write(int byte, void *context) {
    const char value = (char)byte;
    runtime_format_output_write(context, &value, 1);
}

static bool is_conversion_specifier(char byte) {
    switch (byte) {
        case 'd':
        case 'i':
        case 'u':
        case 'o':
        case 'x':
        case 'X':
        case 'c':
        case 's':
        case 'p':
        case 'n':
        case '%':
        case 'f':
        case 'F':
        case 'e':
        case 'E':
        case 'g':
        case 'G':
        case 'a':
        case 'A':
            return true;
        default:
            return false;
    }
}

static bool is_float_specifier(char byte) {
    return byte == 'f' || byte == 'F' || byte == 'e' || byte == 'E' ||
           byte == 'g' || byte == 'G' || byte == 'a' || byte == 'A';
}

typedef enum {
    RUNTIME_FORMAT_LENGTH_DEFAULT,
    RUNTIME_FORMAT_LENGTH_CHAR,
    RUNTIME_FORMAT_LENGTH_SHORT,
    RUNTIME_FORMAT_LENGTH_LONG,
    RUNTIME_FORMAT_LENGTH_LONG_LONG,
    RUNTIME_FORMAT_LENGTH_INTMAX,
    RUNTIME_FORMAT_LENGTH_SIZE,
    RUNTIME_FORMAT_LENGTH_PTRDIFF,
    RUNTIME_FORMAT_LENGTH_LONG_DOUBLE,
} RuntimeFormatLength;

static RuntimeFormatLength conversion_length(
    const char *conversion,
    const char *specifier
) {
    for (const char *cursor = conversion; cursor < specifier; ++cursor) {
        if (*cursor == 'h') {
            return cursor + 1 < specifier && cursor[1] == 'h'
                       ? RUNTIME_FORMAT_LENGTH_CHAR
                       : RUNTIME_FORMAT_LENGTH_SHORT;
        }
        if (*cursor == 'l') {
            return cursor + 1 < specifier && cursor[1] == 'l'
                       ? RUNTIME_FORMAT_LENGTH_LONG_LONG
                       : RUNTIME_FORMAT_LENGTH_LONG;
        }
        if (*cursor == 'j') {
            return RUNTIME_FORMAT_LENGTH_INTMAX;
        }
        if (*cursor == 'z') {
            return RUNTIME_FORMAT_LENGTH_SIZE;
        }
        if (*cursor == 't') {
            return RUNTIME_FORMAT_LENGTH_PTRDIFF;
        }
        if (*cursor == 'L') {
            return RUNTIME_FORMAT_LENGTH_LONG_DOUBLE;
        }
    }
    return RUNTIME_FORMAT_LENGTH_DEFAULT;
}

static void consume_star_arguments(
    va_list *arguments,
    const char *conversion,
    const char *specifier
) {
    for (const char *cursor = conversion; cursor < specifier; ++cursor) {
        if (*cursor == '*') {
            (void)va_arg(*arguments, int);
        }
    }
}

static void consume_integer_argument(
    va_list *arguments,
    RuntimeFormatLength length,
    bool is_signed
) {
    if (is_signed) {
        switch (length) {
            case RUNTIME_FORMAT_LENGTH_LONG:
                (void)va_arg(*arguments, long);
                return;
            case RUNTIME_FORMAT_LENGTH_LONG_LONG:
                (void)va_arg(*arguments, long long);
                return;
            case RUNTIME_FORMAT_LENGTH_INTMAX:
                (void)va_arg(*arguments, intmax_t);
                return;
            case RUNTIME_FORMAT_LENGTH_SIZE:
            case RUNTIME_FORMAT_LENGTH_PTRDIFF:
                (void)va_arg(*arguments, ptrdiff_t);
                return;
            default:
                (void)va_arg(*arguments, int);
                return;
        }
    }
    switch (length) {
        case RUNTIME_FORMAT_LENGTH_LONG:
            (void)va_arg(*arguments, unsigned long);
            return;
        case RUNTIME_FORMAT_LENGTH_LONG_LONG:
            (void)va_arg(*arguments, unsigned long long);
            return;
        case RUNTIME_FORMAT_LENGTH_INTMAX:
            (void)va_arg(*arguments, uintmax_t);
            return;
        case RUNTIME_FORMAT_LENGTH_SIZE:
        case RUNTIME_FORMAT_LENGTH_PTRDIFF:
            (void)va_arg(*arguments, size_t);
            return;
        default:
            (void)va_arg(*arguments, unsigned int);
            return;
    }
}

static void consume_conversion_argument(
    va_list *arguments,
    const char *conversion,
    const char *specifier
) {
    consume_star_arguments(arguments, conversion, specifier);
    const RuntimeFormatLength length =
        conversion_length(conversion, specifier);
    switch (*specifier) {
        case 'd':
        case 'i':
            consume_integer_argument(arguments, length, true);
            return;
        case 'u':
        case 'o':
        case 'x':
        case 'X':
            consume_integer_argument(arguments, length, false);
            return;
        case 'c':
            (void)va_arg(*arguments, int);
            return;
        case 's':
            (void)va_arg(*arguments, char *);
            return;
        case 'p':
        case 'n':
            (void)va_arg(*arguments, void *);
            return;
        default:
            return;
    }
}

static void writeback_emitted_count(
    va_list *arguments,
    RuntimeFormatLength length,
    size_t emitted
) {
    switch (length) {
        case RUNTIME_FORMAT_LENGTH_CHAR:
            *va_arg(*arguments, signed char *) = (signed char)emitted;
            return;
        case RUNTIME_FORMAT_LENGTH_SHORT:
            *va_arg(*arguments, short *) = (short)emitted;
            return;
        case RUNTIME_FORMAT_LENGTH_LONG:
            *va_arg(*arguments, long *) = (long)emitted;
            return;
        case RUNTIME_FORMAT_LENGTH_LONG_LONG:
            *va_arg(*arguments, long long *) = (long long)emitted;
            return;
        case RUNTIME_FORMAT_LENGTH_INTMAX:
            *va_arg(*arguments, intmax_t *) = (intmax_t)emitted;
            return;
        case RUNTIME_FORMAT_LENGTH_SIZE:
        case RUNTIME_FORMAT_LENGTH_PTRDIFF:
            *va_arg(*arguments, ptrdiff_t *) = (ptrdiff_t)emitted;
            return;
        default:
            *va_arg(*arguments, int *) = (int)emitted;
            return;
    }
}

void runtime_format_bind_console(RuntimeFormatWrite write, void *context) {
    if (write == 0) {
        runtime_format_contract_failure();
    }
    console_output = (RuntimeFormatOutput){
        .context = context,
        .write = write,
    };
}

uint64_t RUNTIME_FORMAT_PUBLIC runtime_bounded_hex_encode_source_token(void) {
    return UINT64_C(0x0eff478cd85e2305);
}

size_t RUNTIME_FORMAT_PUBLIC runtime_bounded_hex_encode_uppercase(
    char *output,
    size_t output_capacity,
    const uint8_t *input,
    size_t input_length
) {
    static const char alphabet[] = "0123456789ABCDEF";
    if (output == 0 || (input == 0 && input_length != 0) ||
        input_length > (SIZE_MAX - 1) / 2 ||
        output_capacity < input_length * 2 + 1) {
        runtime_format_contract_failure();
    }
    for (size_t index = 0; index < input_length; ++index) {
        output[index * 2] = alphabet[input[index] >> 4];
        output[index * 2 + 1] = alphabet[input[index] & 15u];
    }
    output[input_length * 2] = '\0';
    return input_length * 2;
}

void RUNTIME_FORMAT_PUBLIC runtime_format_output_write(
    RuntimeFormatOutput *output,
    const char *bytes,
    size_t length
) {
    if (output == 0 || output->write == 0 ||
        (bytes == 0 && length != 0) || length > SIZE_MAX - output->emitted) {
        runtime_format_contract_failure();
    }
    output->write(output->context, bytes, length);
    output->emitted += length;
}

void RUNTIME_FORMAT_PUBLIC runtime_format_console_write(
    void *context,
    const char *bytes,
    size_t length
) {
    (void)context;
    runtime_format_output_write(&console_output, bytes, length);
}

bool RUNTIME_FORMAT_PUBLIC runtime_format_buffer_context_validate(
    const RuntimeFormatBufferContext *context) {
    return context != 0 && context->buffer != 0 &&
           context->used <= context->capacity;
}

bool RUNTIME_FORMAT_PUBLIC runtime_format_validate_byte_span(
    const char *bytes,
    size_t length
) {
    return bytes != 0 || length == 0;
}

void RUNTIME_FORMAT_PUBLIC runtime_format_buffer_write(
    void *opaque_context,
    const char *bytes,
    size_t length
) {
    RuntimeFormatBufferContext *context = opaque_context;
    if (!runtime_format_buffer_context_validate(context) ||
        !runtime_format_validate_byte_span(bytes, length)) {
        runtime_format_contract_failure();
    }
    const size_t copied =
        smaller_size(length, context->capacity - context->used);
    for (size_t index = 0; index < copied; ++index) {
        context->buffer[context->used + index] = bytes[index];
    }
    context->used += copied;
}

int RUNTIME_FORMAT_PUBLIC runtime_format_vwrite(
    RuntimeFormatOutput *output,
    const char *format,
    va_list arguments
) {
    if (output == 0 || output->write == 0 || format == 0) {
        runtime_format_contract_failure();
    }
    const size_t initial_emitted = output->emitted;
    va_list cursor_arguments;
    va_copy(cursor_arguments, arguments);
    const char *cursor = format;
    while (*cursor != '\0') {
        const char *literal = cursor;
        while (*cursor != '\0' && *cursor != '%') {
            ++cursor;
        }
        runtime_format_output_write(output, literal, (size_t)(cursor - literal));
        if (*cursor == '\0') {
            break;
        }

        const char *conversion = cursor++;
        while (*cursor != '\0' && !is_conversion_specifier(*cursor)) {
            ++cursor;
        }
        if (*cursor == '\0') {
            runtime_format_output_write(
                output,
                conversion,
                (size_t)(cursor - conversion)
            );
            break;
        }
        const char specifier = *cursor++;
        if (specifier == 'n') {
            consume_star_arguments(&cursor_arguments, conversion, cursor - 1);
            writeback_emitted_count(
                &cursor_arguments,
                conversion_length(conversion, cursor - 1),
                output->emitted - initial_emitted
            );
            continue;
        }
        if (is_float_specifier(specifier)) {
            consume_star_arguments(&cursor_arguments, conversion, cursor - 1);
            if (conversion_length(conversion, cursor - 1) ==
                RUNTIME_FORMAT_LENGTH_LONG_DOUBLE) {
                (void)va_arg(cursor_arguments, long double);
            } else {
                (void)va_arg(cursor_arguments, double);
            }
            static const char placeholder[] = "<float>";
            runtime_format_output_write(
                output,
                placeholder,
                sizeof(placeholder) - 1
            );
            continue;
        }

        char fragment[64];
        const size_t fragment_length = (size_t)(cursor - conversion);
        if (fragment_length >= sizeof(fragment)) {
            runtime_format_contract_failure();
        }
        for (size_t index = 0; index < fragment_length; ++index) {
            fragment[index] = conversion[index];
        }
        fragment[fragment_length] = '\0';
        va_list fragment_arguments;
        va_copy(fragment_arguments, cursor_arguments);
        (void)npf_vpprintf(
            nanoprintf_write,
            output,
            fragment,
            fragment_arguments
        );
        va_end(fragment_arguments);
        consume_conversion_argument(&cursor_arguments, conversion, cursor - 1);
    }
    const size_t emitted = output->emitted - initial_emitted;
    va_end(cursor_arguments);
    return emitted > INT32_MAX ? INT32_MAX : (int)emitted;
}

int RUNTIME_FORMAT_PUBLIC runtime_vprintf_with_options(
    uint32_t options,
    const char *format,
    va_list arguments
) {
    (void)options;
    console_output.emitted = 0;
    return runtime_format_vwrite(&console_output, format, arguments);
}

int RUNTIME_FORMAT_PUBLIC runtime_vprintf(
    const char *format,
    va_list arguments
) {
    return runtime_vprintf_with_options(0, format, arguments);
}

int RUNTIME_FORMAT_PUBLIC runtime_printf(const char *format, ...) {
    va_list arguments;
    va_start(arguments, format);
    const int result = runtime_vprintf(format, arguments);
    va_end(arguments);
    return result;
}

int RUNTIME_FORMAT_PUBLIC runtime_firebloom_vsnprintf_with_options(
    char *output,
    size_t capacity,
    uint32_t options,
    const char *format,
    va_list arguments
) {
    (void)options;
    if ((output == 0 && capacity != 0) || format == 0) {
        runtime_format_contract_failure();
    }
    char empty_output;
    RuntimeFormatBufferContext buffer_context = {
        .buffer = capacity == 0 ? &empty_output : output,
        .capacity = capacity == 0 ? 0 : capacity - 1,
    };
    RuntimeFormatOutput format_output = {
        .context = &buffer_context,
        .write = runtime_format_buffer_write,
    };
    const int result = runtime_format_vwrite(&format_output, format, arguments);
    if (capacity != 0) {
        output[buffer_context.used] = '\0';
    }
    return result;
}

int RUNTIME_FORMAT_PUBLIC runtime_firebloom_vsnprintf(
    char *output,
    size_t capacity,
    const char *format,
    va_list arguments
) {
    return runtime_firebloom_vsnprintf_with_options(
        output,
        capacity,
        0,
        format,
        arguments
    );
}

int RUNTIME_FORMAT_PUBLIC runtime_vsnprintf(
    char *output,
    size_t capacity,
    const char *format,
    va_list arguments
) {
    return runtime_firebloom_vsnprintf(output, capacity, format, arguments);
}

int RUNTIME_FORMAT_PUBLIC runtime_firebloom_snprintf(
    char *output,
    size_t capacity,
    const char *format,
    ...
) {
    va_list arguments;
    va_start(arguments, format);
    const int result =
        runtime_firebloom_vsnprintf(output, capacity, format, arguments);
    va_end(arguments);
    return result;
}

int RUNTIME_FORMAT_PUBLIC runtime_format_render_conversion(
    RuntimeFormatOutput *output,
    const char *conversion,
    va_list arguments
) {
    return runtime_format_vwrite(output, conversion, arguments);
}

void RUNTIME_FORMAT_PUBLIC runtime_format_update_field_validity(
    RuntimeFormatField *field,
    int64_t value
) {
    if (field == 0) {
        runtime_format_null_state_panic();
    }
    if (value < 0 || value > INT32_MAX) {
        field->valid = false;
    }
}

_Noreturn void RUNTIME_FORMAT_PUBLIC runtime_format_null_state_panic(void) {
    runtime_format_contract_failure();
}

uint32_t RUNTIME_FORMAT_PUBLIC runtime_format_parse_field_from_argument(
    RuntimeFormatField *field,
    int32_t value
) {
    runtime_format_update_field_validity(field, value);
    return value < 0 ? 0 : (uint32_t)value;
}

uint32_t RUNTIME_FORMAT_PUBLIC runtime_format_parse_decimal_field(
    RuntimeFormatField *field,
    const char **cursor,
    const char *upper_bound
) {
    if (field == 0 || cursor == 0 || *cursor == 0 || upper_bound == 0 ||
        *cursor > upper_bound) {
        runtime_format_contract_failure();
    }
    uint64_t value = 0;
    const char *position = *cursor;
    while (position < upper_bound && *position >= '0' && *position <= '9') {
        value = value * 10 + (uint64_t)(*position - '0');
        runtime_format_update_field_validity(field, (int64_t)value);
        ++position;
    }
    *cursor = position;
    return value > UINT32_MAX ? UINT32_MAX : (uint32_t)value;
}

void RUNTIME_FORMAT_PUBLIC runtime_format_write_padding(
    RuntimeFormatOutput *output,
    char byte,
    size_t count
) {
    char chunk[16];
    for (size_t index = 0; index < sizeof(chunk); ++index) {
        chunk[index] = byte;
    }
    while (count != 0) {
        const size_t emitted = smaller_size(count, sizeof(chunk));
        runtime_format_output_write(output, chunk, emitted);
        count -= emitted;
    }
}

void RUNTIME_FORMAT_PUBLIC runtime_format_write_padded_span(
    RuntimeFormatOutput *output,
    const char *bytes,
    size_t length,
    const RuntimeFormatField *field
) {
    if (field == 0 || !field->valid ||
        !runtime_format_validate_byte_span(bytes, length)) {
        runtime_format_contract_failure();
    }
    const size_t padding = field->width > 0 && (size_t)field->width > length
                               ? (size_t)field->width - length
                               : 0;
    if (!field->left_aligned) {
        runtime_format_write_padding(output, ' ', padding);
    }
    runtime_format_output_write(output, bytes, length);
    if (field->left_aligned) {
        runtime_format_write_padding(output, ' ', padding);
    }
}

void RUNTIME_FORMAT_PUBLIC runtime_format_write_integer_conversion(
    RuntimeFormatOutput *output,
    uint64_t value,
    bool negative,
    unsigned base,
    bool uppercase,
    const RuntimeFormatField *field
) {
    static const char lowercase[] = "0123456789abcdef";
    static const char uppercase_digits[] = "0123456789ABCDEF";
    if (field == 0 || !field->valid || base < 2 || base > 16) {
        runtime_format_contract_failure();
    }
    const char *digits = uppercase ? uppercase_digits : lowercase;
    char reversed[64];
    size_t digit_count = 0;
    do {
        reversed[digit_count++] = digits[value % base];
        value /= base;
    } while (value != 0);

    char prefix[3];
    size_t prefix_length = 0;
    if (negative) {
        prefix[prefix_length++] = '-';
    } else if (field->force_sign) {
        prefix[prefix_length++] = '+';
    } else if (field->leading_space) {
        prefix[prefix_length++] = ' ';
    }
    if (field->alternate_form && base == 16) {
        prefix[prefix_length++] = '0';
        prefix[prefix_length++] = uppercase ? 'X' : 'x';
    } else if (field->alternate_form && base == 8 &&
               reversed[digit_count - 1] != '0') {
        prefix[prefix_length++] = '0';
    }

    const size_t precision_zeroes =
        field->precision_specified && field->precision > 0 &&
                (size_t)field->precision > digit_count
            ? (size_t)field->precision - digit_count
            : 0;
    const size_t core_length = prefix_length + precision_zeroes + digit_count;
    const size_t field_padding = field->width > 0 &&
                                         (size_t)field->width > core_length
                                     ? (size_t)field->width - core_length
                                     : 0;
    const bool use_zero_padding =
        field->zero_padded && !field->precision_specified;
    if (!field->left_aligned && !use_zero_padding) {
        runtime_format_write_padding(output, ' ', field_padding);
    }
    runtime_format_output_write(output, prefix, prefix_length);
    if (!field->left_aligned && use_zero_padding) {
        runtime_format_write_padding(output, '0', field_padding);
    }
    runtime_format_write_padding(output, '0', precision_zeroes);
    while (digit_count != 0) {
        --digit_count;
        runtime_format_output_write(output, reversed + digit_count, 1);
    }
    if (field->left_aligned) {
        runtime_format_write_padding(output, ' ', field_padding);
    }
}

int RUNTIME_FORMAT_PUBLIC runtime_vsnprintf_chk(
    char *output,
    size_t capacity,
    int flags,
    size_t object_size,
    const char *format,
    va_list arguments
) {
    (void)flags;
    if (capacity > object_size) {
        runtime_vsnprintf_object_size_fatal(capacity, object_size);
    }
    return runtime_vsnprintf(output, capacity, format, arguments);
}

_Noreturn void RUNTIME_FORMAT_PUBLIC
runtime_vsnprintf_object_size_fatal(size_t capacity, size_t object_size) {
    (void)capacity;
    (void)object_size;
    runtime_format_contract_failure();
}
