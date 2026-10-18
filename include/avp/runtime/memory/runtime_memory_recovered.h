#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "avp/runtime/firebloom/runtime_firebloom.h"


enum {
    RUNTIME_MEMORY_EINVAL = 22,
    RUNTIME_MEMORY_ERANGE = 34,
};

typedef RuntimeFirebloomSpan (*RuntimeMemoryAllocate)(
    void *context,
    size_t size,
    size_t alignment
);

typedef struct {
    void *context;
    RuntimeMemoryAllocate allocate;
} RuntimeMemoryRecoveredPlatform;

#define RUNTIME_MEMORY_RECOVERED_PUBLIC __attribute__((used, retain))

extern const RuntimeFirebloomType runtime_memory_byte_type;

void runtime_memory_recovered_bind_platform(
    const RuntimeMemoryRecoveredPlatform *platform
);
RUNTIME_MEMORY_RECOVERED_PUBLIC void runtime_firebloom_zero_32_byte_span(
    RuntimeFirebloomSpan destination
);
RUNTIME_MEMORY_RECOVERED_PUBLIC bool runtime_ascii_is_space(int character);
RUNTIME_MEMORY_RECOVERED_PUBLIC uint8_t runtime_ctype_flags_byte(
    int character
);
RUNTIME_MEMORY_RECOVERED_PUBLIC int runtime_firebloom_memset_s_checked(
    RuntimeFirebloomSpan destination,
    size_t destination_size,
    int value,
    size_t count,
    size_t maximum_count
);
RUNTIME_MEMORY_RECOVERED_PUBLIC RuntimeFirebloomSpan runtime_errno_span(void);
RUNTIME_MEMORY_RECOVERED_PUBLIC uint64_t runtime_bounded_strtoull(
    RuntimeFirebloomSpan input,
    RuntimeFirebloomSpan *end,
    int base
);
RUNTIME_MEMORY_RECOVERED_PUBLIC RuntimeFirebloomSpan runtime_bounded_strdup(
    RuntimeFirebloomSpan input
);
RUNTIME_MEMORY_RECOVERED_PUBLIC RuntimeFirebloomSpan
runtime_firebloom_validate_byte_span(RuntimeFirebloomSpan input);
RUNTIME_MEMORY_RECOVERED_PUBLIC uint64_t
runtime_bounded_strlen_source_token(void);
RUNTIME_MEMORY_RECOVERED_PUBLIC _Noreturn void
runtime_bounded_memmove_capacity_panic(void);
RUNTIME_MEMORY_RECOVERED_PUBLIC _Noreturn void
runtime_bounded_memset_capacity_panic(void);
RUNTIME_MEMORY_RECOVERED_PUBLIC int runtime_firebloom_compare_span(
    RuntimeFirebloomSpan left,
    RuntimeFirebloomSpan right,
    size_t size
);
RUNTIME_MEMORY_RECOVERED_PUBLIC int runtime_memset_s(
    void *destination,
    size_t destination_size,
    int value,
    size_t count
);
RUNTIME_MEMORY_RECOVERED_PUBLIC int runtime_memset_s_checked(
    void *destination,
    size_t destination_size,
    int value,
    size_t count,
    size_t maximum_count
);
RUNTIME_MEMORY_RECOVERED_PUBLIC RuntimeFirebloomSpan
runtime_firebloom_outlined_validate_left_span_save_right(
    RuntimeFirebloomSpan left,
    RuntimeFirebloomSpan right,
    size_t size
);
RUNTIME_MEMORY_RECOVERED_PUBLIC void runtime_bzero_tail_clone(
    void *destination,
    size_t size
);
