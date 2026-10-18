#pragma once

#include "avp/runtime/memory/allocator/runtime_allocator_recovered.h"

#include <stdatomic.h>


typedef struct {
    uintptr_t metadata_lower;
    uintptr_t metadata_cursor;
    uintptr_t metadata_upper;
    uintptr_t allocation_cursor;
    uintptr_t allocation_upper;
} RuntimeAllocatorRouteStorage;

_Static_assert(
    sizeof(RuntimeAllocatorRouteStorage) == 0x28,
    "recovered allocator route storage layout"
);

extern RuntimeAllocatorRecoveredPlatform runtime_allocator_recovered_platform;
extern atomic_flag runtime_allocator_recovered_lock;

_Noreturn void runtime_allocator_recovered_fail(void);
void runtime_allocator_recovered_zero(void *pointer, size_t size);
uintptr_t runtime_allocator_recovered_align_up(
    uintptr_t value,
    size_t alignment
);
uintptr_t runtime_allocator_recovered_align_down(
    uintptr_t value,
    size_t alignment
);
RuntimeAllocatorRouteStorage *runtime_allocator_recovered_storage(
    RuntimeAllocatorRoute *route
);
RuntimeAllocatorRangeRoot *runtime_allocator_recovered_default_root(
    RuntimeAllocatorRoute *route
);
bool runtime_allocator_recovered_release_from_route(
    RuntimeAllocatorRoute *route,
    void *allocation
);
