#include "avp/runtime/firebloom/runtime_firebloom_remaining_recovered.h"

RuntimeFirebloomSpan j_runtime_firebloom_return_empty_tuple(
    const RuntimeFirebloomType *type
) {
    return (RuntimeFirebloomSpan){.type = type};
}

RuntimeFirebloomSpan j_runtime_firebloom_return_empty_tuple_clone(
    const RuntimeFirebloomType *type
) {
    return j_runtime_firebloom_return_empty_tuple(type);
}

size_t runtime_firebloom_descriptor_byte_size(
    const void *descriptor,
    const void *lower_bound,
    const void *upper_bound
) {
    const uintptr_t current = (uintptr_t)descriptor;
    const uintptr_t lower = (uintptr_t)lower_bound;
    const uintptr_t upper = (uintptr_t)upper_bound;
    if (current < lower || upper < current || upper - current < 0x28u) {
        __builtin_trap();
    }
    const uint8_t *bytes = descriptor;
    size_t byte_size = 0;
    for (size_t index = 0; index < sizeof(uint64_t); ++index) {
        byte_size |= (size_t)bytes[0x20u + index] << (index * 8u);
    }
    return byte_size;
}

RuntimeFirebloomDescriptorBounds
runtime_firebloom_outlined_save_descriptor_bounds_x21_x20(
    const void *lower_bound,
    const void *upper_bound
) {
    return (RuntimeFirebloomDescriptorBounds){
        .lower_bound = (uintptr_t)lower_bound,
        .upper_bound = (uintptr_t)upper_bound,
    };
}

