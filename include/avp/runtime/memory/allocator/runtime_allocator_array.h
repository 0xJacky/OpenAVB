#pragma once

#include "avp/runtime/memory/allocator/runtime_allocator.h"

#include <stddef.h>
#include <stdint.h>


typedef struct {
    size_t element_size;
    size_t alignment;
} RuntimeAllocatorElementGeometry;

typedef struct {
    RuntimeAllocatorRoute *route;
    const RuntimeFirebloomType *type;
} RuntimeAllocatorTypedRange;

typedef struct {
    void *context;
    RuntimeAllocatorRoute *(*select_route)(void *context, bool primary);
    RuntimeFirebloomSpan (*allocate)(
        void *context,
        RuntimeAllocatorRoute *route,
        size_t payload_size,
        size_t allocation_size,
        size_t alignment,
        const RuntimeFirebloomType *type
    );
} RuntimeAllocatorArrayPlatform;

#define RUNTIME_ALLOCATOR_ARRAY_PUBLIC __attribute__((used, retain))

void runtime_allocator_array_bind_platform(
    const RuntimeAllocatorArrayPlatform *platform
);

RUNTIME_ALLOCATOR_ARRAY_PUBLIC RuntimeFirebloomSpan
runtime_allocator_allocate_bytes_impl(
    RuntimeAllocatorRoute *route,
    size_t byte_count
);
RUNTIME_ALLOCATOR_ARRAY_PUBLIC RuntimeFirebloomSpan
runtime_allocator_allocate_array_impl(
    RuntimeAllocatorRoute *route,
    size_t element_size,
    size_t element_count
);
RUNTIME_ALLOCATOR_ARRAY_PUBLIC RuntimeFirebloomSpan
runtime_allocator_allocate_array_aligned_impl(
    RuntimeAllocatorRoute *route,
    size_t element_size,
    size_t element_count,
    size_t alignment
);
RUNTIME_ALLOCATOR_ARRAY_PUBLIC RuntimeAllocatorTypedRange
runtime_allocator_resolve_typed_range_root_slot(
    RuntimeAllocatorTypedRange range
);
RUNTIME_ALLOCATOR_ARRAY_PUBLIC RuntimeFirebloomSpan
runtime_allocator_allocate_typed_instance_impl(
    RuntimeAllocatorTypedRange range
);
RUNTIME_ALLOCATOR_ARRAY_PUBLIC RuntimeFirebloomSpan
runtime_allocator_allocate_typed_array_current(
    RuntimeAllocatorTypedRange range,
    size_t element_count
);
RUNTIME_ALLOCATOR_ARRAY_PUBLIC RuntimeFirebloomSpan
runtime_allocator_allocate_typed_array_aligned_impl(
    RuntimeAllocatorTypedRange range,
    size_t element_count,
    size_t alignment
);

RUNTIME_ALLOCATOR_ARRAY_PUBLIC RuntimeFirebloomSpan runtime_allocator_allocate_bytes(
    RuntimeAllocatorRoute *route,
    size_t byte_count
);
RUNTIME_ALLOCATOR_ARRAY_PUBLIC RuntimeFirebloomSpan runtime_allocator_allocate_array(
    RuntimeAllocatorRoute *route,
    size_t element_size,
    size_t element_count
);
RUNTIME_ALLOCATOR_ARRAY_PUBLIC RuntimeFirebloomSpan
runtime_allocator_allocate_array_aligned(
    RuntimeAllocatorRoute *route,
    size_t element_size,
    size_t element_count,
    size_t alignment
);
RUNTIME_ALLOCATOR_ARRAY_PUBLIC RuntimeFirebloomSpan
runtime_allocator_allocate_typed_instance(
    RuntimeAllocatorTypedRange range
);
RUNTIME_ALLOCATOR_ARRAY_PUBLIC RuntimeFirebloomSpan
runtime_allocator_allocate_typed_array_aligned(
    RuntimeAllocatorTypedRange range,
    size_t element_count,
    size_t alignment
);

RUNTIME_ALLOCATOR_ARRAY_PUBLIC void
runtime_allocator_compute_array_sizes_checked(
    size_t alignment,
    size_t element_size,
    size_t element_count,
    size_t *allocation_size,
    size_t *payload_size
);
RUNTIME_ALLOCATOR_ARRAY_PUBLIC size_t runtime_allocator_align_up_checked(
    size_t size,
    size_t alignment
);
RUNTIME_ALLOCATOR_ARRAY_PUBLIC _Noreturn void
runtime_allocator_align_up_overflow_fatal(void);
RUNTIME_ALLOCATOR_ARRAY_PUBLIC RuntimeAllocatorElementGeometry
runtime_firebloom_type_element_geometry(
    const RuntimeFirebloomType *type
);
RUNTIME_ALLOCATOR_ARRAY_PUBLIC RuntimeFirebloomSpan
runtime_allocator_outlined_allocate_one_typed_element_checked(
    RuntimeAllocatorTypedRange range
);
