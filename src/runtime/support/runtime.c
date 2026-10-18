#include "avp/runtime/exception/pvpanic.h"
#include "avp/runtime/memory/runtime_memory.h"
#include "avp/runtime/support/recovered_symbol.h"

#include <stddef.h>
#include <stdint.h>


_Noreturn void runtime_firebloom_panic(void) {
    pvpanic_trigger();
}

void *memcpy(void *destination, const void *source, size_t size) {
    unsigned char *output = destination;
    const unsigned char *input = source;
    for (size_t index = 0; index < size; ++index) {
        output[index] = input[index];
    }
    return destination;
}

void *runtime_memmove(void *destination, const void *source, size_t size) {
    unsigned char *output = destination;
    const unsigned char *input = source;
    const uintptr_t output_address = (uintptr_t)output;
    const uintptr_t input_address = (uintptr_t)input;
    if (output_address < input_address ||
        output_address - input_address >= size) {
        for (size_t index = 0; index < size; ++index) {
            output[index] = input[index];
        }
    } else {
        for (size_t index = size; index != 0; --index) {
            output[index - 1] = input[index - 1];
        }
    }
    return destination;
}

void *memmove(void *destination, const void *source, size_t size) {
    return runtime_memmove(destination, source, size);
}

void *runtime_memset(void *destination, int value, size_t size) {
    unsigned char *output = destination;
    for (size_t index = 0; index < size; ++index) {
        output[index] = (unsigned char)value;
    }
    return destination;
}

void *memset(void *destination, int value, size_t size) {
    return runtime_memset(destination, value, size);
}

int strcmp(const char *left, const char *right) {
    while (*left != '\0' && *left == *right) {
        ++left;
        ++right;
    }
    return (int)(unsigned char)*left - (int)(unsigned char)*right;
}

void runtime_bzero(void *destination, size_t size) {
    (void)runtime_memset(destination, 0, size);
}

AVP_RECOVERED_ENTRY size_t runtime_bounded_strnlen(
    const char *current,
    const char *lower,
    const char *upper,
    size_t limit
) {
    for (size_t index = 0; index < limit; ++index) {
        const char *cursor = current + index;
        if (cursor < lower || cursor >= upper) {
            runtime_firebloom_panic();
        }
        if (*cursor == '\0') {
            return index;
        }
    }
    return limit;
}

size_t runtime_bounded_strlen(
    const char *current,
    const char *lower,
    const char *upper
) {
    if (current < lower || current > upper) {
        runtime_firebloom_panic();
    }
    const size_t span = (size_t)(upper - current);
    const size_t length = runtime_bounded_strnlen(
        current,
        lower,
        upper,
        span
    );
    if (length >= span) {
        runtime_firebloom_panic();
    }
    return length;
}
