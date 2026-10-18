/*
 * These routines adapt musl's string algorithms to the ROM's bounded span
 * ABI. Bounds violations remain terminal runtime failures.
 *
 * Portions derived from musl libc, Copyright (c) 2005-2020 Rich Felker,
 * et al., MIT License; see vendor/musl-string/COPYRIGHT.
 */

#include "avp/runtime/support/runtime_string_compat.h"

#include <stdint.h>

static _Noreturn void runtime_string_bounds_fail(void) {
    __builtin_trap();
}

static size_t span_capacity(RuntimeFirebloomSpan span) {
    uintptr_t current = (uintptr_t)span.current;
    uintptr_t lower = (uintptr_t)span.lower_bound;
    uintptr_t upper = (uintptr_t)span.upper_bound;
    if (current < lower || current > upper) {
        runtime_string_bounds_fail();
    }
    return upper - current;
}

static size_t span_string_length(RuntimeFirebloomSpan span) {
    const unsigned char *string = span.current;
    size_t capacity = span_capacity(span);
    for (size_t index = 0; index < capacity; ++index) {
        if (string[index] == 0) {
            return index;
        }
    }
    runtime_string_bounds_fail();
}

static void *move_bytes(void *destination, const void *source, size_t count) {
    unsigned char *output = destination;
    const unsigned char *input = source;
    uintptr_t output_address = (uintptr_t)output;
    uintptr_t input_address = (uintptr_t)input;
    if (output_address < input_address ||
        output_address - input_address >= count) {
        for (size_t index = 0; index < count; ++index) {
            output[index] = input[index];
        }
    } else {
        while (count != 0) {
            --count;
            output[count] = input[count];
        }
    }
    return destination;
}

int runtime_memcmp(const void *left, const void *right, size_t count) {
    const unsigned char *left_bytes = left;
    const unsigned char *right_bytes = right;
    for (size_t index = 0; index < count; ++index) {
        if (left_bytes[index] != right_bytes[index]) {
            return (int)left_bytes[index] - (int)right_bytes[index];
        }
    }
    return 0;
}

RuntimeFirebloomSpan runtime_bounded_strchr(
    RuntimeFirebloomSpan source,
    int character
) {
    const unsigned char *string = source.current;
    size_t capacity = span_capacity(source);
    unsigned char match = (unsigned char)character;
    for (size_t index = 0; index < capacity; ++index) {
        if (string[index] == match) {
            source.current = (void *)(string + index);
            return source;
        }
        if (string[index] == 0) {
            return (RuntimeFirebloomSpan){0};
        }
    }
    runtime_string_bounds_fail();
}

int runtime_bounded_strcmp(
    RuntimeFirebloomSpan left,
    RuntimeFirebloomSpan right
) {
    size_t left_length = span_string_length(left);
    size_t right_length = span_string_length(right);
    size_t compared = left_length < right_length ? left_length : right_length;
    int result = runtime_memcmp(left.current, right.current, compared);
    if (result != 0) {
        return result;
    }
    const unsigned char *left_bytes = left.current;
    const unsigned char *right_bytes = right.current;
    return (int)left_bytes[compared] - (int)right_bytes[compared];
}

size_t runtime_bounded_strlcat(
    RuntimeFirebloomSpan destination,
    RuntimeFirebloomSpan source
) {
    size_t capacity = span_capacity(destination);
    size_t destination_length = span_string_length(destination);
    size_t source_length = span_string_length(source);
    char *output = destination.current;
    const char *input = source.current;

    size_t available = capacity - destination_length;
    if (available != 0) {
        size_t copied = source_length < available - 1 ?
            source_length : available - 1;
        for (size_t index = 0; index < copied; ++index) {
            output[destination_length + index] = input[index];
        }
        output[destination_length + copied] = '\0';
    }
    return destination_length + source_length;
}

size_t runtime_bounded_strlcpy(
    RuntimeFirebloomSpan destination,
    RuntimeFirebloomSpan source
) {
    size_t capacity = span_capacity(destination);
    size_t source_length = span_string_length(source);
    char *output = destination.current;
    const char *input = source.current;
    if (capacity != 0) {
        size_t copied = source_length < capacity - 1 ?
            source_length : capacity - 1;
        for (size_t index = 0; index < copied; ++index) {
            output[index] = input[index];
        }
        output[copied] = '\0';
    }
    return source_length;
}

int runtime_bounded_strncmp(
    RuntimeFirebloomSpan left,
    RuntimeFirebloomSpan right,
    size_t count
) {
    if (count == 0) {
        return 0;
    }
    size_t left_length = span_string_length(left);
    size_t right_length = span_string_length(right);
    size_t left_available = left_length + 1;
    size_t right_available = right_length + 1;
    size_t compared = count;
    if (compared > left_available) {
        compared = left_available;
    }
    if (compared > right_available) {
        compared = right_available;
    }
    return runtime_memcmp(left.current, right.current, compared);
}

size_t runtime_strlcpy(
    char *destination,
    const char *source,
    size_t capacity
) {
    size_t length = runtime_strlen(source);
    if (capacity != 0) {
        size_t copied = length < capacity - 1 ? length : capacity - 1;
        for (size_t index = 0; index < copied; ++index) {
            destination[index] = source[index];
        }
        destination[copied] = '\0';
    }
    return length;
}

size_t runtime_strlen(const char *string) {
    const char *cursor = string;
    while (*cursor != '\0') {
        ++cursor;
    }
    return (size_t)(cursor - string);
}

int runtime_strncmp(
    const char *left,
    const char *right,
    size_t count
) {
    const unsigned char *left_bytes = (const unsigned char *)left;
    const unsigned char *right_bytes = (const unsigned char *)right;
    while (count != 0 && *left_bytes != 0 &&
        *left_bytes == *right_bytes) {
        ++left_bytes;
        ++right_bytes;
        --count;
    }
    if (count == 0) {
        return 0;
    }
    return (int)*left_bytes - (int)*right_bytes;
}

void *runtime_memmove_source_first(
    const void *source,
    void *destination,
    size_t count
) {
    return move_bytes(destination, source, count);
}

void *runtime_bounded_memmove(
    void *destination,
    size_t capacity,
    const void *source,
    size_t count
) {
    if (count > capacity) {
        runtime_string_bounds_fail();
    }
    return move_bytes(destination, source, count);
}

void *runtime_bounded_memset(
    void *destination,
    size_t capacity,
    int value,
    size_t count
) {
    if (count > capacity) {
        runtime_string_bounds_fail();
    }
    unsigned char *output = destination;
    for (size_t index = 0; index < count; ++index) {
        output[index] = (unsigned char)value;
    }
    return destination;
}
