#include "avp/runtime/memory/allocator/runtime_allocator_array.h"


/*
 * Overflow checks follow the same division-free rules used by Zephyr's
 * size_mul_overflow helper at commit 0d65ce4 (Apache-2.0):
 * https://github.com/zephyrproject-rtos/zephyr/commit/0d65ce46dda0626164503c50f37e6e1f59d310a9
 * The code is independently expressed because this freestanding target only
 * needs two operations and carries an AVP-specific bounded-span ABI.
 */

static bool runtime_allocator_is_power_of_two(size_t value) {
    return value != 0 && (value & (value - 1)) == 0;
}

_Noreturn void runtime_allocator_align_up_overflow_fatal(void) {
    __builtin_trap();
}

size_t runtime_allocator_align_up_checked(
    size_t size,
    size_t alignment
) {
    if (!runtime_allocator_is_power_of_two(alignment) || size == 0 ||
        SIZE_MAX - size < alignment - 1) {
        runtime_allocator_align_up_overflow_fatal();
    }
    return (size + alignment - 1) & ~(alignment - 1);
}

void runtime_allocator_compute_array_sizes_checked(
    size_t alignment,
    size_t element_size,
    size_t element_count,
    size_t *allocation_size,
    size_t *payload_size
) {
    if (allocation_size == 0 || payload_size == 0 || element_size == 0 ||
        element_count == 0 ||
        element_count > SIZE_MAX / element_size) {
        runtime_allocator_align_up_overflow_fatal();
    }
    const size_t payload = element_size * element_count;
    const size_t allocation = runtime_allocator_align_up_checked(
        payload,
        alignment
    );
    *payload_size = payload;
    *allocation_size = allocation;
}

static size_t runtime_allocator_natural_alignment(size_t element_size) {
    size_t alignment = element_size & (~element_size + 1);
    if (alignment > 16) {
        alignment = 16;
    }
    return alignment;
}

RuntimeAllocatorElementGeometry runtime_firebloom_type_element_geometry(
    const RuntimeFirebloomType *type
) {
    if (type == 0 || type->element_size == 0) {
        return (RuntimeAllocatorElementGeometry){0};
    }
    return (RuntimeAllocatorElementGeometry){
        .element_size = type->element_size,
        .alignment = runtime_allocator_natural_alignment(
            type->element_size
        ),
    };
}
