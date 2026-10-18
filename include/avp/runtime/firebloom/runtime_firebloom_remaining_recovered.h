#pragma once

#include "avp/runtime/firebloom/runtime_firebloom.h"

#include <stddef.h>
#include <stdint.h>

#if defined(__ELF__) && __has_attribute(retain)
#define RUNTIME_FIREBLOOM_REMAINING_PUBLIC \
    __attribute__((used, retain, noinline, section(".text.runtime_support_recovered")))
#elif defined(__ELF__)
#define RUNTIME_FIREBLOOM_REMAINING_PUBLIC \
    __attribute__((used, noinline, section(".text.runtime_support_recovered")))
#else
#define RUNTIME_FIREBLOOM_REMAINING_PUBLIC __attribute__((used, noinline))
#endif

typedef struct {
    uintptr_t lower_bound;
    uintptr_t upper_bound;
} RuntimeFirebloomDescriptorBounds;

RUNTIME_FIREBLOOM_REMAINING_PUBLIC RuntimeFirebloomSpan
j_runtime_firebloom_return_empty_tuple(const RuntimeFirebloomType *type);
RUNTIME_FIREBLOOM_REMAINING_PUBLIC RuntimeFirebloomSpan
j_runtime_firebloom_return_empty_tuple_clone(const RuntimeFirebloomType *type);
RUNTIME_FIREBLOOM_REMAINING_PUBLIC size_t
runtime_firebloom_descriptor_byte_size(const void *descriptor,
                                       const void *lower_bound,
                                       const void *upper_bound);
RUNTIME_FIREBLOOM_REMAINING_PUBLIC RuntimeFirebloomDescriptorBounds
runtime_firebloom_outlined_save_descriptor_bounds_x21_x20(
    const void *lower_bound,
    const void *upper_bound);
