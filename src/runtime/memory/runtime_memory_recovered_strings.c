#include "avp/runtime/memory/runtime_memory_recovered.h"
#include "avp/runtime/memory/runtime_memory.h"

#include <stdbool.h>


/*
 * Related public string implementations:
 * musl f21a965 intscan.c and strdup.c (MIT):
 * https://github.com/ifduyue/musl/commit/f21a96538f78fa8e2040831b4209b35f2fb581da
 * FreeBSD 3aac283 strtoull.c and strdup.c (BSD-3-Clause):
 * https://github.com/freebsd/freebsd-src/commit/3aac283613bd3fd0228a06d6c854ca0bf190ecfb
 * AVP's bounded input and four-register end result are compatible extensions.
 */

enum {
    RUNTIME_CTYPE_UPPER = 0x01,
    RUNTIME_CTYPE_LOWER = 0x02,
    RUNTIME_CTYPE_PUNCTUATION = 0x04,
    RUNTIME_CTYPE_CONTROL = 0x08,
    RUNTIME_CTYPE_DIGIT = 0x10,
    RUNTIME_CTYPE_SPACE = 0x20,
    RUNTIME_CTYPE_HEX = 0x40,
    RUNTIME_CTYPE_PRINTABLE = 0x80,
};

static RuntimeMemoryRecoveredPlatform memory_platform;
static bool memory_platform_bound;
static int runtime_errno_value;

const RuntimeFirebloomType runtime_memory_byte_type = {
    .identity = UINT64_C(0x62797465),
    .element_size = 1,
    .bytewise_copyable = true,
};

static const RuntimeFirebloomType runtime_errno_type = {
    .identity = UINT64_C(0x6572726e6f),
    .element_size = sizeof(runtime_errno_value),
    .bytewise_copyable = false,
};

static _Noreturn void runtime_memory_recovered_fail(void) {
    __builtin_trap();
}

static const uint8_t *runtime_memory_require_input(
    RuntimeFirebloomSpan input
) {
    const uintptr_t current = (uintptr_t)input.current;
    const uintptr_t lower = (uintptr_t)input.lower_bound;
    const uintptr_t upper = (uintptr_t)input.upper_bound;
    if (input.current == 0 || input.type == 0 || current < lower ||
        current >= upper || input.type->element_size != 1) {
        runtime_memory_recovered_fail();
    }
    return input.current;
}

void runtime_memory_recovered_bind_platform(
    const RuntimeMemoryRecoveredPlatform *platform
) {
    if (platform == 0 || platform->allocate == 0) {
        runtime_memory_recovered_fail();
    }
    memory_platform = *platform;
    memory_platform_bound = true;
}

uint8_t runtime_ctype_flags_byte(int character) {
    const uint8_t value = (uint8_t)character;
    if (value <= 8 || (value >= 14 && value <= 31) || value == 127) {
        return RUNTIME_CTYPE_CONTROL;
    }
    if (value >= 9 && value <= 13) {
        return RUNTIME_CTYPE_CONTROL | RUNTIME_CTYPE_SPACE;
    }
    if (value == ' ') {
        return RUNTIME_CTYPE_PRINTABLE | RUNTIME_CTYPE_SPACE;
    }
    if (value >= '0' && value <= '9') {
        return RUNTIME_CTYPE_DIGIT | RUNTIME_CTYPE_HEX;
    }
    if (value >= 'A' && value <= 'Z') {
        return RUNTIME_CTYPE_UPPER |
            (value <= 'F' ? RUNTIME_CTYPE_HEX : 0);
    }
    if (value >= 'a' && value <= 'z') {
        return RUNTIME_CTYPE_LOWER |
            (value <= 'f' ? RUNTIME_CTYPE_HEX : 0);
    }
    if (value >= '!' && value <= '~') {
        return RUNTIME_CTYPE_PUNCTUATION;
    }
    return 0;
}

bool runtime_ascii_is_space(int character) {
    return (runtime_ctype_flags_byte(character) & RUNTIME_CTYPE_SPACE) != 0;
}

RuntimeFirebloomSpan runtime_errno_span(void) {
    return (RuntimeFirebloomSpan){
        .current = &runtime_errno_value,
        .lower_bound = &runtime_errno_value,
        .upper_bound = &runtime_errno_value + 1,
        .type = &runtime_errno_type,
    };
}

static int runtime_digit_value(uint8_t character) {
    if (character >= '0' && character <= '9') {
        return character - '0';
    }
    if (character >= 'A' && character <= 'Z') {
        return character - 'A' + 10;
    }
    if (character >= 'a' && character <= 'z') {
        return character - 'a' + 10;
    }
    return -1;
}

static void runtime_bounded_strtoull_publish_end(
    RuntimeFirebloomSpan input,
    RuntimeFirebloomSpan *end,
    const uint8_t *position
) {
    if (end != 0) {
        *end = input;
        end->current = (void *)position;
    }
}

uint64_t runtime_bounded_strtoull(
    RuntimeFirebloomSpan input,
    RuntimeFirebloomSpan *end,
    int base
) {
    const uint8_t *const original = runtime_memory_require_input(input);
    const uint8_t *cursor = original;
    const uint8_t *const upper = input.upper_bound;
    while (cursor < upper && runtime_ascii_is_space(*cursor)) {
        ++cursor;
    }
    bool negative = false;
    if (cursor < upper && (*cursor == '+' || *cursor == '-')) {
        negative = *cursor == '-';
        ++cursor;
    }
    if (base != 0 && (base < 2 || base > 36)) {
        runtime_errno_value = RUNTIME_MEMORY_EINVAL;
        runtime_bounded_strtoull_publish_end(input, end, original);
        return 0;
    }

    if ((base == 0 || base == 16) && upper - cursor >= 3 &&
        cursor[0] == '0' && (cursor[1] == 'x' || cursor[1] == 'X') &&
        runtime_digit_value(cursor[2]) >= 0 &&
        runtime_digit_value(cursor[2]) < 16) {
        base = 16;
        cursor += 2;
    } else if (base == 0) {
        base = cursor < upper && *cursor == '0' ? 8 : 10;
    }

    const uint8_t *const digits_begin = cursor;
    uint64_t value = 0;
    bool overflow = false;
    while (cursor < upper) {
        const int digit = runtime_digit_value(*cursor);
        if (digit < 0 || digit >= base) {
            break;
        }
        if (value > (UINT64_MAX - (uint64_t)digit) / (uint64_t)base) {
            overflow = true;
            value = UINT64_MAX;
        } else if (!overflow) {
            value = value * (uint64_t)base + (uint64_t)digit;
        }
        ++cursor;
    }
    if (cursor == digits_begin) {
        runtime_bounded_strtoull_publish_end(input, end, original);
        return 0;
    }
    runtime_bounded_strtoull_publish_end(input, end, cursor);
    if (overflow) {
        runtime_errno_value = RUNTIME_MEMORY_ERANGE;
        return UINT64_MAX;
    }
    return negative ? UINT64_C(0) - value : value;
}

RuntimeFirebloomSpan runtime_firebloom_validate_byte_span(
    RuntimeFirebloomSpan input
) {
    (void)runtime_memory_require_input(input);
    input.type = &runtime_memory_byte_type;
    return input;
}

RuntimeFirebloomSpan runtime_bounded_strdup(RuntimeFirebloomSpan input) {
    const char *current = (const char *)runtime_memory_require_input(input);
    const size_t length = runtime_bounded_strlen(
        current,
        input.lower_bound,
        input.upper_bound
    );
    if (!memory_platform_bound) {
        runtime_memory_recovered_fail();
    }
    RuntimeFirebloomSpan output = memory_platform.allocate(
        memory_platform.context,
        length + 1,
        1
    );
    output = runtime_firebloom_validate_byte_span(output);
    const uintptr_t output_current = (uintptr_t)output.current;
    const uintptr_t output_upper = (uintptr_t)output.upper_bound;
    if (length + 1 > output_upper - output_current) {
        runtime_memory_recovered_fail();
    }
    runtime_memmove(output.current, input.current, length + 1);
    return output;
}

uint64_t runtime_bounded_strlen_source_token(void) {
    return UINT64_C(0x06920dc7cd459896);
}
