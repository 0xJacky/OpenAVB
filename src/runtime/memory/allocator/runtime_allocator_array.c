#include "avp/runtime/memory/allocator/runtime_allocator_array.h"


/*
 * FreeRTOS-Kernel ce221a8 (MIT) and musl f21a965 (MIT) were reviewed for
 * aligned allocation and calloc overflow behavior:
 * https://github.com/FreeRTOS/FreeRTOS-Kernel/commit/ce221a8bb468e462ca6b435cef66a9636e00baf4
 * https://github.com/ifduyue/musl/commit/f21a96538f78fa8e2040831b4209b35f2fb581da
 * AVP allocation returns a typed four-word capability span and selects a
 * domain route, so the external heap boundary remains a narrow hook.
 */

static RuntimeAllocatorArrayPlatform allocator_array_platform;

static _Noreturn void runtime_allocator_array_fail(void) {
    __builtin_trap();
}

static bool runtime_allocator_array_platform_is_bound(void) {
    return allocator_array_platform.select_route != 0 &&
        allocator_array_platform.allocate != 0;
}

void runtime_allocator_array_bind_platform(
    const RuntimeAllocatorArrayPlatform *platform
) {
    if (platform == 0) {
        runtime_allocator_array_fail();
    }
    allocator_array_platform = *platform;
    if (!runtime_allocator_array_platform_is_bound()) {
        runtime_allocator_array_fail();
    }
}

static RuntimeFirebloomSpan runtime_allocator_allocate_checked(
    RuntimeAllocatorRoute *route,
    const RuntimeFirebloomType *type,
    size_t alignment,
    size_t element_size,
    size_t element_count
) {
    if (!runtime_allocator_array_platform_is_bound() || route == 0) {
        runtime_allocator_array_fail();
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
    RuntimeFirebloomSpan result = allocator_array_platform.allocate(
        allocator_array_platform.context,
        route,
        payload_size,
        allocation_size,
        alignment,
        type
    );
    if (result.current == 0 || result.lower_bound != result.current ||
        result.upper_bound == 0 ||
        (uintptr_t)result.upper_bound < (uintptr_t)result.lower_bound ||
        (uintptr_t)result.upper_bound - (uintptr_t)result.lower_bound <
            payload_size || result.type != type) {
        runtime_allocator_array_fail();
    }
    return result;
}

RuntimeFirebloomSpan runtime_allocator_allocate_bytes_impl(
    RuntimeAllocatorRoute *route,
    size_t byte_count
) {
    return runtime_allocator_allocate_checked(
        route,
        route == 0 ? 0 : route->allocation_type,
        16,
        1,
        byte_count
    );
}

RuntimeFirebloomSpan runtime_allocator_allocate_array_impl(
    RuntimeAllocatorRoute *route,
    size_t element_size,
    size_t element_count
) {
    return runtime_allocator_allocate_checked(
        route,
        route == 0 ? 0 : route->allocation_type,
        16,
        element_size,
        element_count
    );
}

RuntimeFirebloomSpan runtime_allocator_allocate_array_aligned_impl(
    RuntimeAllocatorRoute *route,
    size_t element_size,
    size_t element_count,
    size_t alignment
) {
    return runtime_allocator_allocate_checked(
        route,
        route == 0 ? 0 : route->allocation_type,
        alignment,
        element_size,
        element_count
    );
}

RuntimeAllocatorTypedRange runtime_allocator_resolve_typed_range_root_slot(
    RuntimeAllocatorTypedRange range
) {
    if (!runtime_allocator_array_platform_is_bound() || range.type == 0) {
        runtime_allocator_array_fail();
    }
    if (range.route == 0) {
        range.route = allocator_array_platform.select_route(
            allocator_array_platform.context,
            true
        );
    }
    if (range.route == 0) {
        runtime_allocator_array_fail();
    }
    return range;
}

RuntimeFirebloomSpan runtime_allocator_allocate_typed_instance_impl(
    RuntimeAllocatorTypedRange range
) {
    const RuntimeAllocatorTypedRange resolved =
        runtime_allocator_resolve_typed_range_root_slot(range);
    const RuntimeAllocatorElementGeometry geometry =
        runtime_firebloom_type_element_geometry(resolved.type);
    if (geometry.element_size == 0 || geometry.alignment == 0) {
        runtime_allocator_array_fail();
    }
    return runtime_allocator_allocate_checked(
        resolved.route,
        resolved.type,
        geometry.alignment,
        geometry.element_size,
        1
    );
}

RuntimeFirebloomSpan runtime_allocator_allocate_typed_array_current(
    RuntimeAllocatorTypedRange range,
    size_t element_count
) {
    const RuntimeAllocatorTypedRange resolved =
        runtime_allocator_resolve_typed_range_root_slot(range);
    const RuntimeAllocatorElementGeometry geometry =
        runtime_firebloom_type_element_geometry(resolved.type);
    return runtime_allocator_allocate_checked(
        resolved.route,
        resolved.type,
        geometry.alignment,
        geometry.element_size,
        element_count
    );
}

RuntimeFirebloomSpan runtime_allocator_allocate_typed_array_aligned_impl(
    RuntimeAllocatorTypedRange range,
    size_t element_count,
    size_t alignment
) {
    const RuntimeAllocatorTypedRange resolved =
        runtime_allocator_resolve_typed_range_root_slot(range);
    const RuntimeAllocatorElementGeometry geometry =
        runtime_firebloom_type_element_geometry(resolved.type);
    if (alignment < geometry.alignment) {
        runtime_allocator_array_fail();
    }
    return runtime_allocator_allocate_checked(
        resolved.route,
        resolved.type,
        alignment,
        geometry.element_size,
        element_count
    );
}

RuntimeFirebloomSpan runtime_allocator_allocate_bytes(
    RuntimeAllocatorRoute *route,
    size_t byte_count
) {
    return runtime_allocator_allocate_bytes_impl(route, byte_count);
}

RuntimeFirebloomSpan runtime_allocator_allocate_array(
    RuntimeAllocatorRoute *route,
    size_t element_size,
    size_t element_count
) {
    return runtime_allocator_allocate_array_impl(
        route,
        element_size,
        element_count
    );
}

RuntimeFirebloomSpan runtime_allocator_allocate_array_aligned(
    RuntimeAllocatorRoute *route,
    size_t element_size,
    size_t element_count,
    size_t alignment
) {
    return runtime_allocator_allocate_array_aligned_impl(
        route,
        element_size,
        element_count,
        alignment
    );
}

RuntimeFirebloomSpan runtime_allocator_allocate_typed_instance(
    RuntimeAllocatorTypedRange range
) {
    return runtime_allocator_allocate_typed_instance_impl(range);
}

RuntimeFirebloomSpan runtime_allocator_allocate_typed_array_aligned(
    RuntimeAllocatorTypedRange range,
    size_t element_count,
    size_t alignment
) {
    return runtime_allocator_allocate_typed_array_aligned_impl(
        range,
        element_count,
        alignment
    );
}

RuntimeFirebloomSpan
runtime_allocator_outlined_allocate_one_typed_element_checked(
    RuntimeAllocatorTypedRange range
) {
    return runtime_allocator_allocate_typed_instance_impl(range);
}
