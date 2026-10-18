#include "avp/runtime/memory/runtime_memory_recovered.h"
#include "avp/runtime/memory/runtime_memory.h"


/*
 * Secure fill behavior was checked against FreeBSD memset_s at 3aac283
 * (BSD-3-Clause). Volatile stores preserve the observable wipe contract.
 * https://github.com/freebsd/freebsd-src/commit/3aac283613bd3fd0228a06d6c854ca0bf190ecfb
 */

static _Noreturn void runtime_memory_secure_fail(void) {
    __builtin_trap();
}

void runtime_firebloom_zero_32_byte_span(
    RuntimeFirebloomSpan destination
) {
    (void)runtime_firebloom_fill_span(destination, 0, 0x20);
}

static void runtime_memory_secure_fill(
    void *destination,
    int value,
    size_t count
) {
    if (destination == 0 && count != 0) {
        runtime_memory_secure_fail();
    }
    volatile uint8_t *output = destination;
    for (size_t index = 0; index < count; ++index) {
        output[index] = (uint8_t)value;
    }
}

int runtime_memset_s_checked(
    void *destination,
    size_t destination_size,
    int value,
    size_t count,
    size_t maximum_count
) {
    if (count > maximum_count ||
        (destination_size & (SIZE_MAX / 2 + 1)) != 0 ||
        count > destination_size) {
        runtime_memory_secure_fail();
    }
    runtime_memory_secure_fill(destination, value, count);
    return 0;
}

int runtime_memset_s(
    void *destination,
    size_t destination_size,
    int value,
    size_t count
) {
    return runtime_memset_s_checked(
        destination,
        destination_size,
        value,
        count,
        SIZE_MAX
    );
}

int runtime_firebloom_memset_s_checked(
    RuntimeFirebloomSpan destination,
    size_t destination_size,
    int value,
    size_t count,
    size_t maximum_count
) {
    if (count > maximum_count ||
        (destination_size & (SIZE_MAX / 2 + 1)) != 0 ||
        count > destination_size) {
        runtime_memory_secure_fail();
    }
    (void)runtime_firebloom_fill_span(destination, value, count);
    return 0;
}

int runtime_firebloom_compare_span(
    RuntimeFirebloomSpan left,
    RuntimeFirebloomSpan right,
    size_t size
) {
    runtime_firebloom_validate_span(&left, size);
    runtime_firebloom_validate_span(&right, size);
    const uint8_t *left_bytes = left.current;
    const uint8_t *right_bytes = right.current;
    for (size_t index = 0; index < size; ++index) {
        if (left_bytes[index] != right_bytes[index]) {
            return (int)left_bytes[index] - (int)right_bytes[index];
        }
    }
    return 0;
}

RuntimeFirebloomSpan
runtime_firebloom_outlined_validate_left_span_save_right(
    RuntimeFirebloomSpan left,
    RuntimeFirebloomSpan right,
    size_t size
) {
    runtime_firebloom_validate_span(&left, size);
    return right;
}

_Noreturn void runtime_bounded_memmove_capacity_panic(void) {
    runtime_memory_secure_fail();
}

_Noreturn void runtime_bounded_memset_capacity_panic(void) {
    runtime_memory_secure_fail();
}

void runtime_bzero_tail_clone(void *destination, size_t size) {
    runtime_bzero(destination, size);
}
