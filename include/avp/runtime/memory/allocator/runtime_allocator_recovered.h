#pragma once

#include "avp/runtime/memory/allocator/runtime_allocator_array.h"
#include "avp/runtime/memory/allocator/runtime_allocator_route_tuple.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


#define RUNTIME_ALLOCATOR_RECOVERED_PUBLIC __attribute__((used, retain))

typedef struct {
    uintptr_t lower_bound;
    uintptr_t cursor;
    uintptr_t upper_bound;
} RuntimeAllocatorMetadataArena;

struct RuntimeAllocatorRangeRoot {
    const RuntimeFirebloomType *type;
    RuntimeAllocatorNode *free_extents;
};

typedef struct RuntimeAllocatorRouteRootBinding {
    struct RuntimeAllocatorRouteRootBinding *next;
    const RuntimeAllocatorRoute *key;
    RuntimeAllocatorRangeRoot *root;
    uint64_t reserved;
} RuntimeAllocatorRouteRootBinding;

typedef struct {
    const RuntimeFirebloomType *type;
    RuntimeAllocatorRouteRootBinding *bindings;
} RuntimeAllocatorTypeDescriptor;

typedef struct {
    RuntimeAllocatorRoute *route;
    RuntimeAllocatorRangeRoot **range_root_slot;
} RuntimeAllocatorCheckedCallContext;

typedef struct {
    RuntimeAllocatorRoute *route;
    size_t element_count;
    size_t element_size;
    RuntimeFirebloomSpan *result;
} RuntimeAllocatorArrayCheckedCallContext;

typedef struct {
    uintptr_t first;
    uintptr_t second;
} RuntimeAllocatorRegisterPair;

typedef struct {
    void *allocation;
    size_t element_count;
    size_t element_size;
    const void *tag;
    uint64_t flags;
} RuntimeSupportAllocationCallbackArguments;

typedef void (*RuntimeSupportAllocationCallback)(
    void *allocation,
    size_t element_count,
    size_t element_size,
    const void *tag,
    uint64_t flags
);

typedef struct {
    uintptr_t (*storage_identity)(uintptr_t storage_base, size_t byte_size);
    void (*critical_enter)(void);
    void (*critical_leave)(void);
    RuntimeSupportAllocationCallback allocation_callback;
    bool current_domain_uses_boot_route;
} RuntimeAllocatorRecoveredPlatform;

void runtime_allocator_recovered_bind_platform(
    const RuntimeAllocatorRecoveredPlatform *platform
);

RUNTIME_ALLOCATOR_RECOVERED_PUBLIC unsigned
runtime_allocator_minimum_alignment(void);
RUNTIME_ALLOCATOR_RECOVERED_PUBLIC void *runtime_malloc(size_t byte_count);
RUNTIME_ALLOCATOR_RECOVERED_PUBLIC size_t
runtime_allocator_active_allocation_size(const void *allocation);
RUNTIME_ALLOCATOR_RECOVERED_PUBLIC void runtime_allocator_release_allocation(
    void *current,
    void *lower_bound,
    void *upper_bound,
    const RuntimeFirebloomType *type
);
RUNTIME_ALLOCATOR_RECOVERED_PUBLIC void
runtime_allocator_require_active_route_no_output(void);
RUNTIME_ALLOCATOR_RECOVERED_PUBLIC RuntimeFirebloomSpan
runtime_allocator_route_construct_bounded(
    void *context_current,
    const void *context_lower_bound,
    const void *context_upper_bound,
    const RuntimeFirebloomType *context_type,
    void *storage_current,
    const void *storage_lower_bound,
    const void *storage_upper_bound,
    const RuntimeFirebloomType *storage_type,
    size_t storage_size,
    const RuntimeFirebloomType *allocation_type,
    void *route_context_0,
    void *route_context_1,
    void *route_context_2,
    RuntimeAllocatorRoute *route
);
RUNTIME_ALLOCATOR_RECOVERED_PUBLIC void
runtime_allocator_initialize_current_domain_heap(
    void *arena_current,
    const void *arena_lower_bound,
    const void *arena_upper_bound,
    const RuntimeFirebloomType *arena_type,
    size_t arena_size,
    bool clear_arena
);
RUNTIME_ALLOCATOR_RECOVERED_PUBLIC bool
runtime_allocator_current_domain_uses_boot_route(void);
RUNTIME_ALLOCATOR_RECOVERED_PUBLIC RuntimeFirebloomSpan
runtime_allocator_select_active_route(void);
RUNTIME_ALLOCATOR_RECOVERED_PUBLIC RuntimeFirebloomSpan
runtime_allocator_require_active_route(void);
RUNTIME_ALLOCATOR_RECOVERED_PUBLIC RuntimeFirebloomSpan
runtime_bump_allocate_bounded_bytes(size_t byte_size);
RUNTIME_ALLOCATOR_RECOVERED_PUBLIC uintptr_t
runtime_allocator_storage_identity_hook(uintptr_t storage_base,
                                        size_t byte_size);
RUNTIME_ALLOCATOR_RECOVERED_PUBLIC RuntimeFirebloomSpan
runtime_current_domain_allocator_routes_bounded_view(void);
RUNTIME_ALLOCATOR_RECOVERED_PUBLIC RuntimeFirebloomSpan
runtime_domain_allocator_route_tuple_validate_thunk_a(
    RuntimeFirebloomSpan tuple
);
RUNTIME_ALLOCATOR_RECOVERED_PUBLIC RuntimeFirebloomSpan
runtime_domain_allocator_route_tuple_validate_thunk_b(
    RuntimeFirebloomSpan tuple
);
RUNTIME_ALLOCATOR_RECOVERED_PUBLIC RuntimeFirebloomSpan
runtime_domain_allocator_route_tuple_validate_shared(
    RuntimeFirebloomSpan tuple
);

RUNTIME_ALLOCATOR_RECOVERED_PUBLIC RuntimeAllocatorRangeRoot **
runtime_allocator_descriptor_resolve_route_root_slot(
    RuntimeAllocatorRoute *primary_route,
    const RuntimeAllocatorRoute *keyed_route,
    RuntimeAllocatorTypeDescriptor *descriptor
);
RUNTIME_ALLOCATOR_RECOVERED_PUBLIC void *
runtime_allocator_metadata_node_allocate(RuntimeAllocatorMetadataArena *arena);
RUNTIME_ALLOCATOR_RECOVERED_PUBLIC void
runtime_allocator_extent_rebase_storage(RuntimeAllocatorNode *extent);
RUNTIME_ALLOCATOR_RECOVERED_PUBLIC RuntimeFirebloomSpan
runtime_allocator_allocate_array_checked(
    RuntimeAllocatorRoute *route,
    RuntimeAllocatorRangeRoot *range_root,
    size_t alignment,
    size_t element_count,
    size_t element_size
);
RUNTIME_ALLOCATOR_RECOVERED_PUBLIC void
runtime_allocator_critical_section_enter(void);
RUNTIME_ALLOCATOR_RECOVERED_PUBLIC RuntimeFirebloomSpan
runtime_allocator_allocate_array_locked(
    RuntimeAllocatorRoute *route,
    RuntimeAllocatorRangeRoot *range_root,
    size_t alignment,
    size_t element_count,
    size_t element_size
);
RUNTIME_ALLOCATOR_RECOVERED_PUBLIC void
runtime_allocator_critical_section_leave(void);
RUNTIME_ALLOCATOR_RECOVERED_PUBLIC RuntimeAllocatorRoute *
runtime_allocator_route_create_in_place(
    const RuntimeFirebloomType *allocation_type,
    void *storage_base,
    size_t storage_size
);
RUNTIME_ALLOCATOR_RECOVERED_PUBLIC RuntimeAllocatorNode *
runtime_allocator_extent_node_allocate(
    RuntimeAllocatorRoute *route,
    const RuntimeFirebloomType *allocation_type,
    uintptr_t lower_bound,
    uintptr_t upper_bound
);

RUNTIME_ALLOCATOR_RECOVERED_PUBLIC RuntimeAllocatorArrayCheckedCallContext
runtime_allocator_outlined_prepare_untyped_array_checked_call(
    RuntimeAllocatorRoute *route,
    size_t element_count
);
RUNTIME_ALLOCATOR_RECOVERED_PUBLIC RuntimeFirebloomSpan
runtime_allocator_outlined_load_result_span_from_sp(
    RuntimeFirebloomSpan result
);
RUNTIME_ALLOCATOR_RECOVERED_PUBLIC RuntimeAllocatorCheckedCallContext
runtime_allocator_outlined_prepare_typed_array_checked_call(
    RuntimeAllocatorRoute *route,
    RuntimeAllocatorRangeRoot **range_root_slot
);
RUNTIME_ALLOCATOR_RECOVERED_PUBLIC RuntimeAllocatorRegisterPair
runtime_allocator_outlined_save_x0_x1_to_x20_x19(
    uintptr_t value0,
    uintptr_t value1
);

RUNTIME_ALLOCATOR_RECOVERED_PUBLIC void *runtime_support_allocate(
    size_t byte_count,
    const void *tag
);
RUNTIME_ALLOCATOR_RECOVERED_PUBLIC void *runtime_support_allocate_or_panic(
    size_t byte_count,
    const void *tag
);
RUNTIME_ALLOCATOR_RECOVERED_PUBLIC bool
runtime_support_release_allocation_checked(void *allocation);
RUNTIME_ALLOCATOR_RECOVERED_PUBLIC RuntimeAllocatorRangeRoot **
runtime_support_resolve_allocator_context(
    RuntimeAllocatorRoute **route_out,
    RuntimeAllocatorRangeRoot ***range_root_slot_out
);
RUNTIME_ALLOCATOR_RECOVERED_PUBLIC RuntimeSupportAllocationCallbackArguments
runtime_support_outlined_prepare_allocation_callback(
    void *allocation,
    size_t byte_count,
    const void *tag
);
