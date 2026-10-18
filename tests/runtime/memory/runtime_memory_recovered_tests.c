#include "avp/runtime/memory/runtime_memory.h"
#include "avp/runtime/memory/runtime_memory_recovered.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>


static uint8_t allocation[128];
static size_t allocated_size;
static size_t allocated_alignment;

_Noreturn void pvpanic_trigger(void) {
    __builtin_trap();
}

static RuntimeFirebloomSpan mock_allocate(
    void *context,
    size_t size,
    size_t alignment
) {
    assert(context == allocation);
    assert(size <= sizeof(allocation));
    allocated_size = size;
    allocated_alignment = alignment;
    return (RuntimeFirebloomSpan){
        .current = allocation,
        .lower_bound = allocation,
        .upper_bound = allocation + size,
        .type = &runtime_memory_byte_type,
    };
}

static RuntimeFirebloomSpan byte_span(void *storage, size_t size) {
    return (RuntimeFirebloomSpan){
        .current = storage,
        .lower_bound = storage,
        .upper_bound = (uint8_t *)storage + size,
        .type = &runtime_memory_byte_type,
    };
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
}

static void invoke_short_zero(void) {
    uint8_t storage[16] = {0};
    runtime_firebloom_zero_32_byte_span(byte_span(storage, sizeof(storage)));
}

static void invoke_memset_capacity(void) {
    uint8_t storage[4] = {0};
    (void)runtime_memset_s(storage, sizeof(storage), 0, sizeof(storage) + 1);
}

static void invoke_memset_signed_size(void) {
    uint8_t storage[4] = {0};
    (void)runtime_memset_s_checked(
        storage,
        SIZE_MAX,
        0,
        1,
        SIZE_MAX
    );
}

static void invoke_firebloom_maximum(void) {
    uint8_t storage[4] = {0};
    (void)runtime_firebloom_memset_s_checked(
        byte_span(storage, sizeof(storage)),
        sizeof(storage),
        0,
        2,
        1
    );
}

static void invoke_memmove_panic(void) {
    runtime_bounded_memmove_capacity_panic();
}

static void invoke_memset_panic(void) {
    runtime_bounded_memset_capacity_panic();
}

static void test_ctype(void) {
    assert(runtime_ctype_flags_byte(0) == UINT8_C(0x08));
    assert(runtime_ctype_flags_byte('\t') == UINT8_C(0x28));
    assert(runtime_ctype_flags_byte(' ') == UINT8_C(0xa0));
    assert(runtime_ctype_flags_byte('0') == UINT8_C(0x50));
    assert(runtime_ctype_flags_byte('A') == UINT8_C(0x41));
    assert(runtime_ctype_flags_byte('G') == UINT8_C(0x01));
    assert(runtime_ctype_flags_byte('a') == UINT8_C(0x42));
    assert(runtime_ctype_flags_byte('g') == UINT8_C(0x02));
    assert(runtime_ctype_flags_byte('!') == UINT8_C(0x04));
    assert(runtime_ctype_flags_byte(0x80) == 0);
    assert(runtime_ascii_is_space('\n'));
    assert(runtime_ascii_is_space(' '));
    assert(!runtime_ascii_is_space('A'));
}

static uint64_t parse(
    const char *text,
    size_t size,
    int base,
    size_t *end_offset
) {
    RuntimeFirebloomSpan end = {0};
    const RuntimeFirebloomSpan input = byte_span((void *)text, size);
    const uint64_t value = runtime_bounded_strtoull(input, &end, base);
    assert(end.lower_bound == input.lower_bound);
    assert(end.upper_bound == input.upper_bound);
    assert(end.type == input.type);
    *end_offset = (size_t)((const char *)end.current - text);
    return value;
}

static void test_strtoull(void) {
    size_t end = 0;
    assert(parse("  +123x", 7, 10, &end) == 123);
    assert(end == 6);
    assert(parse("-10", 3, 10, &end) == UINT64_MAX - 9);
    assert(end == 3);
    assert(parse("0779", 4, 0, &end) == 63);
    assert(end == 3);
    assert(parse("0x2a!", 5, 0, &end) == 42);
    assert(end == 4);
    assert(parse("0Xff", 4, 16, &end) == 255);
    assert(end == 4);
    assert(parse("xyz", 3, 10, &end) == 0);
    assert(end == 0);

    int *runtime_errno = runtime_errno_span().current;
    *runtime_errno = 0;
    assert(parse("1", 1, 1, &end) == 0);
    assert(end == 0);
    assert(*runtime_errno == RUNTIME_MEMORY_EINVAL);

    *runtime_errno = 0;
    const char overflow[] = "18446744073709551616z";
    assert(parse(overflow, sizeof(overflow) - 1, 10, &end) == UINT64_MAX);
    assert(end == 20);
    assert(*runtime_errno == RUNTIME_MEMORY_ERANGE);
}

static void test_strdup_and_byte_validation(void) {
    const char text[] = "bounded";
    const RuntimeFirebloomSpan duplicate = runtime_bounded_strdup(
        byte_span((void *)text, sizeof(text))
    );
    assert(duplicate.current == allocation);
    assert(allocated_size == sizeof(text));
    assert(allocated_alignment == 1);
    for (size_t index = 0; index < sizeof(text); ++index) {
        assert(allocation[index] == (uint8_t)text[index]);
    }

    const RuntimeFirebloomType compatible = {
        .identity = UINT64_C(0x1234),
        .element_size = 1,
        .bytewise_copyable = true,
    };
    RuntimeFirebloomSpan span = byte_span(allocation, sizeof(text));
    span.type = &compatible;
    span = runtime_firebloom_validate_byte_span(span);
    assert(span.type == &runtime_memory_byte_type);
}

static void test_secure_fill(void) {
    uint8_t storage[40];
    for (size_t index = 0; index < sizeof(storage); ++index) {
        storage[index] = UINT8_C(0xaa);
    }
    assert(runtime_memset_s(storage, sizeof(storage), 0x5a, 7) == 0);
    for (size_t index = 0; index < sizeof(storage); ++index) {
        assert(storage[index] == (index < 7 ? UINT8_C(0x5a) : UINT8_C(0xaa)));
    }
    assert(runtime_firebloom_memset_s_checked(
        byte_span(storage, sizeof(storage)),
        sizeof(storage),
        0x11,
        9,
        9
    ) == 0);
    for (size_t index = 0; index < 9; ++index) {
        assert(storage[index] == UINT8_C(0x11));
    }

    runtime_firebloom_zero_32_byte_span(byte_span(storage, sizeof(storage)));
    for (size_t index = 0; index < 32; ++index) {
        assert(storage[index] == 0);
    }
    runtime_bzero_tail_clone(storage + 32, 8);
    for (size_t index = 32; index < sizeof(storage); ++index) {
        assert(storage[index] == 0);
    }
}

static void test_compare_and_outlined_validation(void) {
    uint8_t left[] = {1, 2, 3, 4};
    uint8_t right[] = {1, 2, 5, 4};
    assert(runtime_firebloom_compare_span(
        byte_span(left, sizeof(left)),
        byte_span(right, sizeof(right)),
        sizeof(left)
    ) == -2);
    assert(runtime_firebloom_compare_span(
        byte_span(right, sizeof(right)),
        byte_span(left, sizeof(left)),
        sizeof(left)
    ) == 2);
    assert(runtime_firebloom_compare_span(
        byte_span(left, sizeof(left)),
        byte_span(left, sizeof(left)),
        sizeof(left)
    ) == 0);
    const RuntimeFirebloomSpan right_span = byte_span(right, sizeof(right));
    const RuntimeFirebloomSpan saved =
        runtime_firebloom_outlined_validate_left_span_save_right(
            byte_span(left, sizeof(left)),
            right_span,
            sizeof(left)
        );
    assert(saved.current == right_span.current);
    assert(saved.upper_bound == right_span.upper_bound);
}

int main(void) {
    const RuntimeMemoryRecoveredPlatform platform = {
        .context = allocation,
        .allocate = mock_allocate,
    };
    runtime_memory_recovered_bind_platform(&platform);
    test_ctype();
    test_strtoull();
    test_strdup_and_byte_validation();
    test_secure_fill();
    test_compare_and_outlined_validation();
    assert(runtime_bounded_strlen_source_token() ==
        UINT64_C(0x06920dc7cd459896));
    expect_trap(invoke_short_zero);
    expect_trap(invoke_memset_capacity);
    expect_trap(invoke_memset_signed_size);
    expect_trap(invoke_firebloom_maximum);
    expect_trap(invoke_memmove_panic);
    expect_trap(invoke_memset_panic);
    puts("runtime recovered memory tests passed");
    return 0;
}
