#pragma once

#include <stddef.h>
#include <stdint.h>

#include "avp/runtime/firebloom/runtime_firebloom.h"


#define RUNTIME_UNPRIVILEGED_MEMORY_PUBLIC __attribute__((used, retain))

extern const RuntimeFirebloomType runtime_unprivileged_byte_type;
extern const RuntimeFirebloomType runtime_unprivileged_u16_type;
extern const RuntimeFirebloomType runtime_unprivileged_u32_type;
extern const RuntimeFirebloomType runtime_unprivileged_u64_type;

RUNTIME_UNPRIVILEGED_MEMORY_PUBLIC void runtime_copy_from_unprivileged(
    RuntimeFirebloomSpan destination,
    const void *unprivileged_source,
    size_t byte_count
);
RUNTIME_UNPRIVILEGED_MEMORY_PUBLIC void runtime_copy_to_unprivileged(
    void *unprivileged_destination,
    RuntimeFirebloomSpan source,
    size_t byte_count
);
RUNTIME_UNPRIVILEGED_MEMORY_PUBLIC RuntimeFirebloomSpan
runtime_unprivileged_copy_validate_u64_tuple(RuntimeFirebloomSpan span);
RUNTIME_UNPRIVILEGED_MEMORY_PUBLIC RuntimeFirebloomSpan
runtime_unprivileged_copy_validate_u32_tuple(RuntimeFirebloomSpan span);
RUNTIME_UNPRIVILEGED_MEMORY_PUBLIC RuntimeFirebloomSpan
runtime_unprivileged_copy_validate_u16_tuple(RuntimeFirebloomSpan span);
RUNTIME_UNPRIVILEGED_MEMORY_PUBLIC RuntimeFirebloomSpan
runtime_unprivileged_copy_make_destination_byte_span(
    void *base,
    const void *upper_bound,
    size_t byte_offset
);
RUNTIME_UNPRIVILEGED_MEMORY_PUBLIC RuntimeFirebloomSpan
runtime_unprivileged_copy_make_source_byte_span(
    const void *base,
    const void *upper_bound,
    size_t byte_offset
);
