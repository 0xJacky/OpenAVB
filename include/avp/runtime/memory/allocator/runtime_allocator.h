#pragma once

#include "avp/runtime/firebloom/runtime_firebloom.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


typedef struct RuntimeAllocatorNode RuntimeAllocatorNode;
typedef struct RuntimeAllocatorRoute RuntimeAllocatorRoute;
typedef struct RuntimeAllocatorRangeRoot RuntimeAllocatorRangeRoot;

struct RuntimeAllocatorNode {
    RuntimeAllocatorNode *next;
    void *free_range_owner;
    uintptr_t lower_bound;
    uintptr_t upper_bound;
};

_Static_assert(
    sizeof(RuntimeAllocatorNode) == 0x20,
    "RuntimeAllocatorNode layout"
);

struct RuntimeAllocatorRoute {
    uint8_t opaque_00[0x28];
    uintptr_t owned_lower_bound;
    uint8_t opaque_30[0x10];
    uintptr_t owned_upper_bound;
    const RuntimeFirebloomType *allocation_type;
    RuntimeAllocatorRangeRoot *reserved_50;
    RuntimeAllocatorNode *allocations;
    RuntimeAllocatorNode *recycled_nodes;
};

_Static_assert(
    offsetof(RuntimeAllocatorRoute, owned_lower_bound) == 0x28,
    "RuntimeAllocatorRoute owned lower offset"
);
_Static_assert(
    offsetof(RuntimeAllocatorRoute, owned_upper_bound) == 0x40,
    "RuntimeAllocatorRoute owned upper offset"
);
_Static_assert(
    offsetof(RuntimeAllocatorRoute, allocations) == 0x58,
    "RuntimeAllocatorRoute allocation list offset"
);
_Static_assert(
    offsetof(RuntimeAllocatorRoute, recycled_nodes) == 0x60,
    "RuntimeAllocatorRoute recycled list offset"
);
_Static_assert(sizeof(RuntimeAllocatorRoute) == 0x68, "allocator route size");

typedef struct {
    RuntimeAllocatorRoute *primary;
    RuntimeAllocatorRoute *fallback;
} RuntimeAllocatorRoutes;

typedef struct {
    RuntimeAllocatorRoute *route;
    void *free_range_owner;
    RuntimeAllocatorNode *node;
    void *allocation;
} RuntimeAllocatorUnlinkedAllocation;

_Static_assert(
    sizeof(RuntimeAllocatorUnlinkedAllocation) == 0x20,
    "allocator unlink result size"
);

typedef void (*RuntimeAllocatorUnmapFunction)(
    uintptr_t address,
    size_t size
);

typedef struct {
    uintptr_t lower_bound;
    uintptr_t upper_bound;
    uintptr_t forbidden_mask;
    RuntimeAllocatorUnmapFunction unmap;
} RuntimeAllocatorMappingProfile;

void runtime_allocator_publish_routes(RuntimeAllocatorRoutes routes);
void runtime_allocator_copy_routes(
    RuntimeAllocatorRoute **primary,
    RuntimeAllocatorRoute **fallback
);
RuntimeAllocatorRoute *runtime_allocator_select_route(bool primary);
RuntimeAllocatorRoute *runtime_allocator_select_available_route(void);
void runtime_allocator_route_initialize(
    RuntimeAllocatorRoute *route,
    uintptr_t owned_lower_bound,
    uintptr_t owned_upper_bound
);
void runtime_allocator_route_track(
    RuntimeAllocatorRoute *route,
    RuntimeAllocatorNode *node,
    void *allocation,
    size_t size,
    void *free_range_owner
);
bool runtime_allocator_route_release(
    RuntimeAllocatorRoute *route,
    void *allocation
);
bool runtime_allocator_route_unlink_allocation(
    RuntimeAllocatorRoute *route,
    void *allocation,
    RuntimeAllocatorUnlinkedAllocation *result
);
void runtime_allocator_recycle_unlinked_allocation(
    RuntimeAllocatorUnlinkedAllocation *allocation
);
size_t runtime_allocator_route_allocation_size(
    const RuntimeAllocatorRoute *route,
    const void *allocation
);
void runtime_allocator_release_address(void *allocation);
void runtime_allocator_publish_mapping_profile(
    RuntimeAllocatorMappingProfile profile
);
void runtime_allocator_release_bounded(RuntimeFirebloomSpan allocation);
