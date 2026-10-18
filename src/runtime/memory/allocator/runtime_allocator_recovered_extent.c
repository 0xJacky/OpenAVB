#include "runtime_allocator_recovered_private.h"


static RuntimeAllocatorMetadataArena route_metadata_arena(
    RuntimeAllocatorRoute *route
) {
    RuntimeAllocatorRouteStorage *storage =
        runtime_allocator_recovered_storage(route);
    return (RuntimeAllocatorMetadataArena){
        .lower_bound = storage->metadata_lower,
        .cursor = storage->metadata_cursor,
        .upper_bound = storage->metadata_upper,
    };
}

static void commit_route_metadata_arena(
    RuntimeAllocatorRoute *route,
    RuntimeAllocatorMetadataArena arena
) {
    runtime_allocator_recovered_storage(route)->metadata_cursor = arena.cursor;
}

void *runtime_allocator_metadata_node_allocate(
    RuntimeAllocatorMetadataArena *arena
) {
    if (arena == 0 || arena->lower_bound > arena->cursor ||
        arena->cursor > arena->upper_bound) {
        runtime_allocator_recovered_fail();
    }
    const uintptr_t current = runtime_allocator_recovered_align_up(
        arena->cursor,
        _Alignof(RuntimeAllocatorNode)
    );
    if (current > arena->upper_bound ||
        sizeof(RuntimeAllocatorNode) > arena->upper_bound - current) {
        runtime_allocator_recovered_fail();
    }
    arena->cursor = current + sizeof(RuntimeAllocatorNode);
    void *result = (void *)current;
    runtime_allocator_recovered_zero(result, sizeof(RuntimeAllocatorNode));
    return result;
}

RuntimeAllocatorNode *runtime_allocator_extent_node_allocate(
    RuntimeAllocatorRoute *route,
    const RuntimeFirebloomType *allocation_type,
    uintptr_t lower_bound,
    uintptr_t upper_bound
) {
    if (route == 0 || allocation_type == 0 || lower_bound >= upper_bound) {
        runtime_allocator_recovered_fail();
    }
    RuntimeAllocatorNode *node = route->recycled_nodes;
    if (node != 0) {
        route->recycled_nodes = node->next;
        runtime_allocator_recovered_zero(node, sizeof(*node));
    } else {
        RuntimeAllocatorMetadataArena arena = route_metadata_arena(route);
        node = runtime_allocator_metadata_node_allocate(&arena);
        commit_route_metadata_arena(route, arena);
    }
    node->free_range_owner = (void *)allocation_type;
    node->lower_bound = lower_bound;
    node->upper_bound = upper_bound;
    return node;
}

RuntimeAllocatorRangeRoot **runtime_allocator_descriptor_resolve_route_root_slot(
    RuntimeAllocatorRoute *primary_route,
    const RuntimeAllocatorRoute *keyed_route,
    RuntimeAllocatorTypeDescriptor *descriptor
) {
    if (primary_route == 0 || keyed_route == 0 || descriptor == 0 ||
        descriptor->type == 0) {
        runtime_allocator_recovered_fail();
    }
    RuntimeAllocatorRouteRootBinding *binding = descriptor->bindings;
    while (binding != 0 && binding->key != keyed_route) {
        binding = binding->next;
    }
    if (binding == 0) {
        RuntimeAllocatorMetadataArena arena = route_metadata_arena(primary_route);
        binding = runtime_allocator_metadata_node_allocate(&arena);
        RuntimeAllocatorRangeRoot *root =
            runtime_allocator_metadata_node_allocate(&arena);
        commit_route_metadata_arena(primary_route, arena);
        binding->next = descriptor->bindings;
        binding->key = keyed_route;
        binding->root = root;
        root->type = descriptor->type;
        root->free_extents = 0;
        descriptor->bindings = binding;
    }
    return &binding->root;
}

void runtime_allocator_extent_rebase_storage(RuntimeAllocatorNode *extent) {
    if (extent == 0 || extent->lower_bound >= extent->upper_bound) {
        runtime_allocator_recovered_fail();
    }
    const size_t size = extent->upper_bound - extent->lower_bound;
    const uintptr_t rebased = runtime_allocator_storage_identity_hook(
        extent->lower_bound,
        size
    );
    if (rebased == 0 || UINTPTR_MAX - rebased < size) {
        runtime_allocator_recovered_fail();
    }
    extent->lower_bound = rebased;
    extent->upper_bound = rebased + size;
}

void runtime_allocator_critical_section_enter(void) {
    if (atomic_flag_test_and_set_explicit(
            &runtime_allocator_recovered_lock,
            memory_order_acquire
        )) {
        runtime_allocator_recovered_fail();
    }
    if (runtime_allocator_recovered_platform.critical_enter != 0) {
        runtime_allocator_recovered_platform.critical_enter();
    }
}

void runtime_allocator_critical_section_leave(void) {
    if (!atomic_flag_test_and_set_explicit(
            &runtime_allocator_recovered_lock,
            memory_order_relaxed
        )) {
        runtime_allocator_recovered_fail();
    }
    if (runtime_allocator_recovered_platform.critical_leave != 0) {
        runtime_allocator_recovered_platform.critical_leave();
    }
    atomic_flag_clear_explicit(
        &runtime_allocator_recovered_lock,
        memory_order_release
    );
}

static uintptr_t aligned_candidate(
    uintptr_t lower,
    uintptr_t upper,
    size_t allocation_size,
    size_t alignment
) {
    const uintptr_t candidate = runtime_allocator_recovered_align_up(
        lower,
        alignment
    );
    if (candidate > upper || allocation_size > upper - candidate) {
        return 0;
    }
    return candidate;
}

static RuntimeAllocatorNode **find_free_extent(
    RuntimeAllocatorRangeRoot *range_root,
    size_t allocation_size,
    size_t alignment,
    uintptr_t *candidate_out
) {
    RuntimeAllocatorNode **link = &range_root->free_extents;
    while (*link != 0) {
        const uintptr_t candidate = aligned_candidate(
            (*link)->lower_bound,
            (*link)->upper_bound,
            allocation_size,
            alignment
        );
        if (candidate != 0) {
            *candidate_out = candidate;
            return link;
        }
        link = &(*link)->next;
    }
    return 0;
}

static void track_allocation(
    RuntimeAllocatorRoute *route,
    RuntimeAllocatorNode *node,
    RuntimeAllocatorRangeRoot *range_root,
    uintptr_t lower,
    uintptr_t upper
) {
    node->next = route->allocations;
    node->free_range_owner = range_root;
    node->lower_bound = lower;
    node->upper_bound = upper;
    route->allocations = node;
}

RuntimeFirebloomSpan runtime_allocator_allocate_array_locked(
    RuntimeAllocatorRoute *route,
    RuntimeAllocatorRangeRoot *range_root,
    size_t alignment,
    size_t element_count,
    size_t element_size
) {
    if (route == 0 || range_root == 0 || range_root->type == 0) {
        runtime_allocator_recovered_fail();
    }
    size_t allocation_size = 0;
    size_t payload_size = 0;
    runtime_allocator_compute_array_sizes_checked(
        alignment,
        element_size,
        element_count,
        &allocation_size,
        &payload_size
    );

    uintptr_t lower = 0;
    RuntimeAllocatorNode **free_link = find_free_extent(
        range_root,
        allocation_size,
        alignment,
        &lower
    );
    RuntimeAllocatorNode *allocation_node = 0;
    if (free_link != 0) {
        RuntimeAllocatorNode *free_node = *free_link;
        *free_link = free_node->next;
        const uintptr_t original_lower = free_node->lower_bound;
        const uintptr_t original_upper = free_node->upper_bound;
        allocation_node = free_node;
        if (original_lower < lower) {
            RuntimeAllocatorNode *prefix = runtime_allocator_extent_node_allocate(
                route,
                range_root->type,
                original_lower,
                lower
            );
            prefix->free_range_owner = range_root;
            prefix->next = range_root->free_extents;
            range_root->free_extents = prefix;
        }
        const uintptr_t allocation_upper = lower + allocation_size;
        if (allocation_upper < original_upper) {
            RuntimeAllocatorNode *suffix = runtime_allocator_extent_node_allocate(
                route,
                range_root->type,
                allocation_upper,
                original_upper
            );
            suffix->free_range_owner = range_root;
            suffix->next = range_root->free_extents;
            range_root->free_extents = suffix;
        }
    } else {
        RuntimeAllocatorRouteStorage *storage =
            runtime_allocator_recovered_storage(route);
        if (storage->allocation_cursor < allocation_size) {
            runtime_allocator_recovered_fail();
        }
        lower = runtime_allocator_recovered_align_down(
            storage->allocation_cursor - allocation_size,
            alignment
        );
        if (lower < storage->metadata_upper) {
            runtime_allocator_recovered_fail();
        }
        storage->allocation_cursor = lower;
        allocation_node = runtime_allocator_extent_node_allocate(
            route,
            range_root->type,
            lower,
            lower + allocation_size
        );
    }

    track_allocation(
        route,
        allocation_node,
        range_root,
        lower,
        lower + allocation_size
    );
    runtime_allocator_recovered_zero((void *)lower, allocation_size);
    return (RuntimeFirebloomSpan){
        .current = (void *)lower,
        .lower_bound = (void *)lower,
        .upper_bound = (void *)(lower + payload_size),
        .type = range_root->type,
    };
}

RuntimeFirebloomSpan runtime_allocator_allocate_array_checked(
    RuntimeAllocatorRoute *route,
    RuntimeAllocatorRangeRoot *range_root,
    size_t alignment,
    size_t element_count,
    size_t element_size
) {
    runtime_allocator_critical_section_enter();
    RuntimeFirebloomSpan result = runtime_allocator_allocate_array_locked(
        route,
        range_root,
        alignment,
        element_count,
        element_size
    );
    runtime_allocator_critical_section_leave();
    return result;
}

static void recycle_node(
    RuntimeAllocatorRoute *route,
    RuntimeAllocatorNode *node
) {
    node->next = route->recycled_nodes;
    route->recycled_nodes = node;
}

static void insert_free_extent(
    RuntimeAllocatorRoute *route,
    RuntimeAllocatorRangeRoot *root,
    RuntimeAllocatorNode *node
) {
    RuntimeAllocatorNode **link = &root->free_extents;
    while (*link != 0 && (*link)->lower_bound < node->lower_bound) {
        link = &(*link)->next;
    }
    node->next = *link;
    *link = node;
    if (node->next != 0 && node->upper_bound == node->next->lower_bound) {
        RuntimeAllocatorNode *next = node->next;
        node->upper_bound = next->upper_bound;
        node->next = next->next;
        recycle_node(route, next);
    }
    RuntimeAllocatorNode **previous_link = &root->free_extents;
    RuntimeAllocatorNode *previous = 0;
    while (*previous_link != node) {
        previous = *previous_link;
        previous_link = &(*previous_link)->next;
    }
    if (previous != 0 && previous->upper_bound == node->lower_bound) {
        previous->upper_bound = node->upper_bound;
        previous->next = node->next;
        recycle_node(route, node);
    }
}

bool runtime_allocator_recovered_release_from_route(
    RuntimeAllocatorRoute *route,
    void *allocation
) {
    if (route == 0 || allocation == 0) {
        return allocation == 0;
    }
    RuntimeAllocatorNode **link = &route->allocations;
    while (*link != 0 && (*link)->lower_bound != (uintptr_t)allocation) {
        link = &(*link)->next;
    }
    RuntimeAllocatorNode *node = *link;
    if (node == 0 || node->upper_bound <= node->lower_bound ||
        node->free_range_owner == 0) {
        return false;
    }
    *link = node->next;
    runtime_allocator_recovered_zero(
        allocation,
        node->upper_bound - node->lower_bound
    );
    RuntimeAllocatorRangeRoot *root = node->free_range_owner;
    insert_free_extent(route, root, node);
    return true;
}
