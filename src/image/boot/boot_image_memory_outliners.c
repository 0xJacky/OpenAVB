#include "avp/image/boot/boot_image_memory_outliners.h"

#include <stddef.h>

extern uintptr_t __stack_chk_guard;

static _Noreturn void boot_image_memory_outliner_contract_failure(void) {
    __builtin_trap();
    __builtin_unreachable();
}

static const RuntimeFirebloomType source_allocation_type = {
    .identity = UINT64_C(0x00145258),
    .element_size = 0x30,
    .bytewise_copyable = false,
};

RuntimeFirebloomSpan boot_image_validate_source_allocation_span(
    RuntimeFirebloomSpan allocation
) {
    const uintptr_t current = (uintptr_t)allocation.current;
    const uintptr_t lower = (uintptr_t)allocation.lower_bound;
    const uintptr_t upper = (uintptr_t)allocation.upper_bound;
    if (allocation.type == NULL ||
        allocation.type->identity != source_allocation_type.identity ||
        lower > current || current >= upper || upper - current < 0x30) {
        boot_image_memory_outliner_contract_failure();
    }
    return allocation;
}

RuntimeFirebloomSpan boot_image_outlined_allocation_span_x19_x22(
    RuntimeFirebloomSpan allocation
) {
    return allocation;
}

RuntimeFirebloomSpan boot_image_outlined_restore_source_record_bounds_x26_x24(
    void *current,
    void *lower_bound,
    void *upper_bound,
    const RuntimeFirebloomType *type
) {
    return (RuntimeFirebloomSpan){
        .current = current,
        .lower_bound = lower_bound,
        .upper_bound = upper_bound,
        .type = type,
    };
}

bool boot_image_stack_guard_compare_saved_x8_clone(uint64_t saved_guard) {
    return saved_guard == __stack_chk_guard;
}

RuntimeFirebloomSpan boot_image_outlined_save_source_record_bounds_x26_x24(
    RuntimeFirebloomSpan record
) {
    return record;
}

uint64_t boot_image_stack_guard_load_to_x8_clone(void) {
    return __stack_chk_guard;
}

uint32_t boot_image_load_error_code_base_to_w20(void) {
    return UINT32_C(0x40030001);
}

RuntimeFirebloomSpan boot_image_outlined_save_allocation_span_x19_x22(
    RuntimeFirebloomSpan allocation
) {
    return allocation;
}
