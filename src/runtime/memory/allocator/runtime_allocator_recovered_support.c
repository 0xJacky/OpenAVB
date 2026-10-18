#include "runtime_allocator_recovered_private.h"


static RuntimeAllocatorRoute *route_containing_allocation(
    const void *allocation,
    size_t *size_out
) {
    RuntimeAllocatorRoute *primary = 0;
    RuntimeAllocatorRoute *fallback = 0;
    runtime_allocator_copy_routes(&primary, &fallback);
    RuntimeAllocatorRoute *routes[2] = {primary, fallback};
    for (size_t index = 0; index < 2; ++index) {
        const size_t size = runtime_allocator_route_allocation_size(
            routes[index],
            allocation
        );
        if (size != 0) {
            if (size_out != 0) {
                *size_out = size;
            }
            return routes[index];
        }
    }
    return 0;
}

void *runtime_malloc(size_t byte_count) {
    RuntimeAllocatorRoute *route = runtime_allocator_require_active_route().current;
    RuntimeFirebloomSpan allocation = runtime_allocator_allocate_array_checked(
        route,
        runtime_allocator_recovered_default_root(route),
        1,
        byte_count,
        1
    );
    return allocation.current;
}

size_t runtime_allocator_active_allocation_size(const void *allocation) {
    size_t size = 0;
    (void)route_containing_allocation(allocation, &size);
    return size;
}

void runtime_allocator_release_allocation(
    void *current,
    void *lower_bound,
    void *upper_bound,
    const RuntimeFirebloomType *type
) {
    if (current == 0) {
        return;
    }
    if (current != lower_bound || (uintptr_t)upper_bound <= (uintptr_t)current ||
        type == 0) {
        runtime_allocator_recovered_fail();
    }
    RuntimeAllocatorRoute *route = route_containing_allocation(current, 0);
    if (route == 0 ||
        !runtime_allocator_recovered_release_from_route(route, current)) {
        runtime_allocator_recovered_fail();
    }
}

RuntimeAllocatorArrayCheckedCallContext
runtime_allocator_outlined_prepare_untyped_array_checked_call(
    RuntimeAllocatorRoute *route,
    size_t element_count
) {
    return (RuntimeAllocatorArrayCheckedCallContext){
        .route = route,
        .element_count = element_count,
        .element_size = 1,
        .result = 0,
    };
}

RuntimeFirebloomSpan runtime_allocator_outlined_load_result_span_from_sp(
    RuntimeFirebloomSpan result
) {
    return result;
}

RuntimeAllocatorCheckedCallContext
runtime_allocator_outlined_prepare_typed_array_checked_call(
    RuntimeAllocatorRoute *route,
    RuntimeAllocatorRangeRoot **range_root_slot
) {
    if (route == 0 || range_root_slot == 0) {
        runtime_allocator_recovered_fail();
    }
    return (RuntimeAllocatorCheckedCallContext){route, range_root_slot};
}

RuntimeAllocatorRegisterPair
runtime_allocator_outlined_save_x0_x1_to_x20_x19(
    uintptr_t value0,
    uintptr_t value1
) {
    return (RuntimeAllocatorRegisterPair){value0, value1};
}

RuntimeAllocatorRangeRoot **runtime_support_resolve_allocator_context(
    RuntimeAllocatorRoute **route_out,
    RuntimeAllocatorRangeRoot ***range_root_slot_out
) {
    RuntimeAllocatorRoute *route = runtime_allocator_require_active_route().current;
    RuntimeAllocatorRangeRoot **slot = &route->reserved_50;
    if (*slot == 0) {
        runtime_allocator_recovered_fail();
    }
    if (route_out != 0) {
        *route_out = route;
    }
    if (range_root_slot_out != 0) {
        *range_root_slot_out = slot;
    }
    return slot;
}

RuntimeSupportAllocationCallbackArguments
runtime_support_outlined_prepare_allocation_callback(
    void *allocation,
    size_t byte_count,
    const void *tag
) {
    return (RuntimeSupportAllocationCallbackArguments){
        .allocation = allocation,
        .element_count = byte_count,
        .element_size = 1,
        .tag = tag,
        .flags = 0,
    };
}

void *runtime_support_allocate(size_t byte_count, const void *tag) {
    RuntimeAllocatorRoute *route = 0;
    RuntimeAllocatorRangeRoot **slot = 0;
    (void)runtime_support_resolve_allocator_context(&route, &slot);
    RuntimeFirebloomSpan allocation = runtime_allocator_allocate_array_checked(
        route,
        *slot,
        16,
        byte_count,
        1
    );
    if (runtime_allocator_recovered_platform.allocation_callback != 0) {
        RuntimeSupportAllocationCallbackArguments arguments =
            runtime_support_outlined_prepare_allocation_callback(
                allocation.current,
                byte_count,
                tag
            );
        runtime_allocator_recovered_platform.allocation_callback(
            arguments.allocation,
            arguments.element_count,
            arguments.element_size,
            arguments.tag,
            arguments.flags
        );
    }
    return allocation.current;
}

void *runtime_support_allocate_or_panic(size_t byte_count, const void *tag) {
    void *allocation = runtime_support_allocate(byte_count, tag);
    if (allocation == 0) {
        runtime_allocator_recovered_fail();
    }
    return allocation;
}

bool runtime_support_release_allocation_checked(void *allocation) {
    if (allocation == 0) {
        return true;
    }
    RuntimeAllocatorRoute *route = route_containing_allocation(allocation, 0);
    return route != 0 &&
        runtime_allocator_recovered_release_from_route(route, allocation);
}
