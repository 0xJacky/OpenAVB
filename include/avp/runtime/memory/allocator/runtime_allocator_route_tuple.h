#pragma once

#include "avp/runtime/memory/allocator/runtime_allocator.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


typedef struct {
    RuntimeAllocatorRoute *current;
    uintptr_t lower_bound;
    uintptr_t upper_bound;
    uintptr_t cookie;
} RuntimeAllocatorRouteTuple;

_Static_assert(
    sizeof(RuntimeAllocatorRouteTuple) == 0x20,
    "allocator route tuple size"
);

typedef struct {
    uint64_t metadata[3];
    RuntimeAllocatorRouteTuple route;
} RuntimeAllocatorRouteRecord;

_Static_assert(
    offsetof(RuntimeAllocatorRouteRecord, route) == 0x18,
    "allocator record route offset"
);

#define RUNTIME_ALLOCATOR_ROUTE_TUPLE_PUBLIC __attribute__((used, retain))

void runtime_allocator_publish_boot_route_tuple(
    RuntimeAllocatorRouteTuple tuple
);
RUNTIME_ALLOCATOR_ROUTE_TUPLE_PUBLIC RuntimeAllocatorRoute *
runtime_allocator_validate_route_tuple_tail_clone_1(
    RuntimeAllocatorRouteTuple tuple
);
RUNTIME_ALLOCATOR_ROUTE_TUPLE_PUBLIC RuntimeAllocatorRoute *
runtime_allocator_validate_route_tuple_tail_clone_2(
    RuntimeAllocatorRouteTuple tuple
);
RUNTIME_ALLOCATOR_ROUTE_TUPLE_PUBLIC RuntimeAllocatorRoute *
runtime_allocator_validate_route_tuple_tail_clone_3(
    RuntimeAllocatorRouteTuple tuple
);
RUNTIME_ALLOCATOR_ROUTE_TUPLE_PUBLIC RuntimeAllocatorRoute *
runtime_allocator_validate_route_tuple_tail_clone_4(
    RuntimeAllocatorRouteTuple tuple
);
RUNTIME_ALLOCATOR_ROUTE_TUPLE_PUBLIC RuntimeAllocatorRoute *
runtime_allocator_validate_route_tuple(
    RuntimeAllocatorRouteTuple tuple
);
RUNTIME_ALLOCATOR_ROUTE_TUPLE_PUBLIC uint64_t
runtime_allocator_route_source_token(void);
RUNTIME_ALLOCATOR_ROUTE_TUPLE_PUBLIC RuntimeAllocatorRouteTuple
runtime_allocator_outlined_prepare_route_result_tuple(void);
RUNTIME_ALLOCATOR_ROUTE_TUPLE_PUBLIC RuntimeAllocatorRouteTuple
runtime_allocator_outlined_save_route_tuple_x19_x22(
    RuntimeAllocatorRouteTuple tuple
);
RUNTIME_ALLOCATOR_ROUTE_TUPLE_PUBLIC bool
runtime_allocator_outlined_compare_route_current_to_upper(
    RuntimeAllocatorRouteTuple tuple
);
RUNTIME_ALLOCATOR_ROUTE_TUPLE_PUBLIC RuntimeAllocatorRouteTuple
runtime_allocator_outlined_restore_route_tuple_x19_x22(
    RuntimeAllocatorRouteTuple tuple
);
RUNTIME_ALLOCATOR_ROUTE_TUPLE_PUBLIC RuntimeAllocatorRouteTuple
runtime_allocator_outlined_load_record_route_to_frame(
    const RuntimeAllocatorRouteRecord *record
);
RUNTIME_ALLOCATOR_ROUTE_TUPLE_PUBLIC RuntimeAllocatorRouteTuple
runtime_allocator_outlined_load_record_route(
    const RuntimeAllocatorRouteRecord *record
);
RUNTIME_ALLOCATOR_ROUTE_TUPLE_PUBLIC RuntimeAllocatorRouteTuple
runtime_allocator_boot_route_tuple(void);
