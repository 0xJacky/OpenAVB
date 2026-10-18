#include "runtime_allocator_recovered_private.h"


#define RUNTIME_ALLOCATOR_MINIMUM_ALIGNMENT ((size_t)0x40)
#define RUNTIME_ALLOCATOR_MINIMUM_STORAGE ((size_t)0x200)
#define RUNTIME_ALLOCATOR_METADATA_LIMIT ((size_t)0x1000)

RuntimeAllocatorRecoveredPlatform runtime_allocator_recovered_platform;
atomic_flag runtime_allocator_recovered_lock = ATOMIC_FLAG_INIT;

static RuntimeAllocatorRoutes current_domain_routes_view;

_Noreturn void runtime_allocator_recovered_fail(void) {
    __builtin_trap();
}

void runtime_allocator_recovered_zero(void *pointer, size_t size) {
    volatile uint8_t *bytes = pointer;
    for (size_t index = 0; index < size; ++index) {
        bytes[index] = 0;
    }
}

static bool is_power_of_two(size_t value) {
    return value != 0 && (value & (value - 1)) == 0;
}

uintptr_t runtime_allocator_recovered_align_up(
    uintptr_t value,
    size_t alignment
) {
    if (!is_power_of_two(alignment) ||
        UINTPTR_MAX - value < alignment - 1) {
        runtime_allocator_recovered_fail();
    }
    return (value + alignment - 1) & ~((uintptr_t)alignment - 1);
}

uintptr_t runtime_allocator_recovered_align_down(
    uintptr_t value,
    size_t alignment
) {
    if (!is_power_of_two(alignment)) {
        runtime_allocator_recovered_fail();
    }
    return value & ~((uintptr_t)alignment - 1);
}

RuntimeAllocatorRouteStorage *runtime_allocator_recovered_storage(
    RuntimeAllocatorRoute *route
) {
    if (route == 0) {
        runtime_allocator_recovered_fail();
    }
    return (RuntimeAllocatorRouteStorage *)(void *)route->opaque_00;
}

RuntimeAllocatorRangeRoot *runtime_allocator_recovered_default_root(
    RuntimeAllocatorRoute *route
) {
    if (route == 0 || route->reserved_50 == 0) {
        runtime_allocator_recovered_fail();
    }
    return route->reserved_50;
}

void runtime_allocator_recovered_bind_platform(
    const RuntimeAllocatorRecoveredPlatform *platform
) {
    runtime_allocator_recovered_platform = platform == 0
        ? (RuntimeAllocatorRecoveredPlatform){0}
        : *platform;
}

unsigned runtime_allocator_minimum_alignment(void) {
    return (unsigned)RUNTIME_ALLOCATOR_MINIMUM_ALIGNMENT;
}

uintptr_t runtime_allocator_storage_identity_hook(
    uintptr_t storage_base,
    size_t byte_size
) {
    if (runtime_allocator_recovered_platform.storage_identity != 0) {
        return runtime_allocator_recovered_platform.storage_identity(
            storage_base,
            byte_size
        );
    }
    return storage_base;
}

RuntimeAllocatorRoute *runtime_allocator_route_create_in_place(
    const RuntimeFirebloomType *allocation_type,
    void *storage_base,
    size_t storage_size
) {
    const uintptr_t lower = (uintptr_t)storage_base;
    if (allocation_type == 0 || storage_base == 0 ||
        storage_size < RUNTIME_ALLOCATOR_MINIMUM_STORAGE ||
        UINTPTR_MAX - lower < storage_size) {
        runtime_allocator_recovered_fail();
    }

    runtime_allocator_recovered_zero(storage_base, storage_size);
    RuntimeAllocatorRoute *route = storage_base;
    RuntimeAllocatorRouteStorage *storage =
        runtime_allocator_recovered_storage(route);
    const uintptr_t storage_upper = lower + storage_size;
    const uintptr_t metadata_lower = runtime_allocator_recovered_align_up(
        lower + sizeof(*route),
        _Alignof(RuntimeAllocatorNode)
    );
    size_t metadata_size = storage_size / 4;
    if (metadata_size > RUNTIME_ALLOCATOR_METADATA_LIMIT) {
        metadata_size = RUNTIME_ALLOCATOR_METADATA_LIMIT;
    }
    const uintptr_t metadata_upper = runtime_allocator_recovered_align_down(
        metadata_lower + metadata_size,
        _Alignof(RuntimeAllocatorNode)
    );
    const uintptr_t allocation_lower = runtime_allocator_recovered_align_up(
        metadata_upper,
        RUNTIME_ALLOCATOR_MINIMUM_ALIGNMENT
    );
    if (metadata_upper - metadata_lower <
            sizeof(RuntimeAllocatorRangeRoot) ||
        allocation_lower >= storage_upper) {
        runtime_allocator_recovered_fail();
    }

    storage->metadata_lower = metadata_lower;
    storage->metadata_cursor = metadata_lower;
    storage->metadata_upper = metadata_upper;
    storage->allocation_cursor = storage_upper;
    storage->allocation_upper = storage_upper;
    route->owned_lower_bound = UINTPTR_MAX;
    route->owned_upper_bound = 0;
    route->allocation_type = allocation_type;

    RuntimeAllocatorMetadataArena arena = {
        .lower_bound = storage->metadata_lower,
        .cursor = storage->metadata_cursor,
        .upper_bound = storage->metadata_upper,
    };
    RuntimeAllocatorRangeRoot *root =
        runtime_allocator_metadata_node_allocate(&arena);
    storage->metadata_cursor = arena.cursor;
    root->type = allocation_type;
    root->free_extents = 0;
    route->reserved_50 = root;
    return route;
}

RuntimeFirebloomSpan runtime_allocator_route_construct_bounded(
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
) {
    (void)context_type;
    (void)storage_type;
    (void)route_context_0;
    (void)route_context_1;
    (void)route_context_2;
    const uintptr_t storage = (uintptr_t)storage_current;
    if (context_current != 0 &&
        ((uintptr_t)context_current < (uintptr_t)context_lower_bound ||
         (uintptr_t)context_current >= (uintptr_t)context_upper_bound)) {
        runtime_allocator_recovered_fail();
    }
    if (storage == 0 || storage < (uintptr_t)storage_lower_bound ||
        storage > (uintptr_t)storage_upper_bound ||
        storage_size > (uintptr_t)storage_upper_bound - storage) {
        runtime_allocator_recovered_fail();
    }
    RuntimeAllocatorRoute *created = runtime_allocator_route_create_in_place(
        allocation_type,
        storage_current,
        storage_size
    );
    if (route != 0 && route != created) {
        *route = *created;
        created = route;
    }
    return (RuntimeFirebloomSpan){
        .current = created,
        .lower_bound = created,
        .upper_bound = (uint8_t *)created + sizeof(*created),
        .type = allocation_type,
    };
}

bool runtime_allocator_current_domain_uses_boot_route(void) {
    return runtime_allocator_recovered_platform.current_domain_uses_boot_route;
}

RuntimeFirebloomSpan runtime_allocator_select_active_route(void) {
    RuntimeAllocatorRoute *route = runtime_allocator_select_available_route();
    if (route == 0 && runtime_allocator_current_domain_uses_boot_route()) {
        route = runtime_allocator_validate_route_tuple(
            runtime_allocator_boot_route_tuple()
        );
    }
    return (RuntimeFirebloomSpan){
        .current = route,
        .lower_bound = route,
        .upper_bound = route == 0 ? 0 : (uint8_t *)route + sizeof(*route),
        .type = route == 0 ? 0 : route->allocation_type,
    };
}

RuntimeFirebloomSpan runtime_allocator_require_active_route(void) {
    RuntimeFirebloomSpan route = runtime_allocator_select_active_route();
    if (route.current == 0) {
        runtime_allocator_recovered_fail();
    }
    return route;
}

void runtime_allocator_require_active_route_no_output(void) {
    (void)runtime_allocator_require_active_route();
}

RuntimeFirebloomSpan runtime_current_domain_allocator_routes_bounded_view(
    void
) {
    runtime_allocator_copy_routes(
        &current_domain_routes_view.primary,
        &current_domain_routes_view.fallback
    );
    return (RuntimeFirebloomSpan){
        .current = &current_domain_routes_view,
        .lower_bound = &current_domain_routes_view,
        .upper_bound = (uint8_t *)&current_domain_routes_view +
            sizeof(current_domain_routes_view),
        .type = 0,
    };
}

RuntimeFirebloomSpan runtime_domain_allocator_route_tuple_validate_shared(
    RuntimeFirebloomSpan tuple
) {
    const uintptr_t current = (uintptr_t)tuple.current;
    if (tuple.current == 0 ||
        current < (uintptr_t)tuple.lower_bound ||
        current >= (uintptr_t)tuple.upper_bound ||
        sizeof(RuntimeAllocatorRoute) >
            (uintptr_t)tuple.upper_bound - current) {
        runtime_allocator_recovered_fail();
    }
    return tuple;
}

RuntimeFirebloomSpan runtime_domain_allocator_route_tuple_validate_thunk_a(
    RuntimeFirebloomSpan tuple
) {
    return runtime_domain_allocator_route_tuple_validate_shared(tuple);
}

RuntimeFirebloomSpan runtime_domain_allocator_route_tuple_validate_thunk_b(
    RuntimeFirebloomSpan tuple
) {
    return runtime_domain_allocator_route_tuple_validate_shared(tuple);
}

void runtime_allocator_initialize_current_domain_heap(
    void *arena_current,
    const void *arena_lower_bound,
    const void *arena_upper_bound,
    const RuntimeFirebloomType *arena_type,
    size_t arena_size,
    bool clear_arena
) {
    const uintptr_t current = (uintptr_t)arena_current;
    if (arena_current == 0 || arena_type == 0 ||
        current < (uintptr_t)arena_lower_bound ||
        current > (uintptr_t)arena_upper_bound ||
        arena_size > (uintptr_t)arena_upper_bound - current) {
        runtime_allocator_recovered_fail();
    }
    if (clear_arena) {
        runtime_allocator_recovered_zero(arena_current, arena_size);
    }
    RuntimeAllocatorRoute *route = runtime_allocator_route_create_in_place(
        arena_type,
        arena_current,
        arena_size
    );
    RuntimeAllocatorRoute *fallback = 0;
    if (runtime_allocator_current_domain_uses_boot_route()) {
        fallback = runtime_allocator_validate_route_tuple(
            runtime_allocator_boot_route_tuple()
        );
    }
    runtime_allocator_publish_routes((RuntimeAllocatorRoutes){route, fallback});
}

RuntimeFirebloomSpan runtime_bump_allocate_bounded_bytes(size_t byte_size) {
    RuntimeAllocatorRoute *route = runtime_allocator_require_active_route().current;
    return runtime_allocator_allocate_array_checked(
        route,
        runtime_allocator_recovered_default_root(route),
        16,
        byte_size,
        1
    );
}
