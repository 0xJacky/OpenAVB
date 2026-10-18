#include "avp/runtime/memory/allocator/runtime_allocator_route_tuple.h"


/*
 * Related public capability representations:
 * CHERIoT RTOS 923e4a0 (MIT), explicit capability bound operations:
 * https://github.com/CHERIoT-Platform/cheriot-rtos/commit/923e4a061ec8f2cda5b322981f13b108aa5d85b9
 * Zephyr 0d65ce4 (Apache-2.0), sys_mem_blocks region metadata:
 * https://github.com/zephyrproject-rtos/zephyr/commit/0d65ce46dda0626164503c50f37e6e1f59d310a9
 * FreeRTOS-Kernel ce221a8 (MIT), HeapRegion_t bounded heap regions:
 * https://github.com/FreeRTOS/FreeRTOS-Kernel/commit/ce221a8bb468e462ca6b435cef66a9636e00baf4
 * AVP stores bounds and provenance as four ordinary AArch64 registers, so the
 * compatible representation remains explicit and independent of ISA builtins.
 */

static RuntimeAllocatorRouteTuple boot_route_tuple;

static bool runtime_allocator_route_tuple_is_empty(
    RuntimeAllocatorRouteTuple tuple
) {
    return tuple.current == 0 && tuple.lower_bound == 0 &&
        tuple.upper_bound == 0 && tuple.cookie == 0;
}

void runtime_allocator_publish_boot_route_tuple(
    RuntimeAllocatorRouteTuple tuple
) {
    if (runtime_allocator_validate_route_tuple(tuple) == 0) {
        __builtin_trap();
    }
    boot_route_tuple = tuple;
}

RuntimeAllocatorRoute *runtime_allocator_validate_route_tuple(
    RuntimeAllocatorRouteTuple tuple
) {
    if (runtime_allocator_route_tuple_is_empty(tuple)) {
        return 0;
    }
    if (tuple.current == 0) {
        return 0;
    }
    const uintptr_t current = (uintptr_t)tuple.current;
    if (tuple.lower_bound > current || current >= tuple.upper_bound ||
        sizeof(*tuple.current) > tuple.upper_bound - current) {
        return 0;
    }
    return tuple.current;
}

RuntimeAllocatorRoute *runtime_allocator_validate_route_tuple_tail_clone_1(
    RuntimeAllocatorRouteTuple tuple
) {
    return runtime_allocator_validate_route_tuple(tuple);
}

RuntimeAllocatorRoute *runtime_allocator_validate_route_tuple_tail_clone_2(
    RuntimeAllocatorRouteTuple tuple
) {
    return runtime_allocator_validate_route_tuple(tuple);
}

RuntimeAllocatorRoute *runtime_allocator_validate_route_tuple_tail_clone_3(
    RuntimeAllocatorRouteTuple tuple
) {
    return runtime_allocator_validate_route_tuple(tuple);
}

RuntimeAllocatorRoute *runtime_allocator_validate_route_tuple_tail_clone_4(
    RuntimeAllocatorRouteTuple tuple
) {
    return runtime_allocator_validate_route_tuple(tuple);
}

uint64_t runtime_allocator_route_source_token(void) {
    return UINT64_C(0x040ce68155485954);
}

RuntimeAllocatorRouteTuple
runtime_allocator_outlined_prepare_route_result_tuple(void) {
    return (RuntimeAllocatorRouteTuple){0};
}

RuntimeAllocatorRouteTuple
runtime_allocator_outlined_save_route_tuple_x19_x22(
    RuntimeAllocatorRouteTuple tuple
) {
    return tuple;
}

bool runtime_allocator_outlined_compare_route_current_to_upper(
    RuntimeAllocatorRouteTuple tuple
) {
    return (uintptr_t)tuple.current < tuple.upper_bound;
}

RuntimeAllocatorRouteTuple
runtime_allocator_outlined_restore_route_tuple_x19_x22(
    RuntimeAllocatorRouteTuple tuple
) {
    return tuple;
}

static RuntimeAllocatorRouteTuple runtime_allocator_load_record_route(
    const RuntimeAllocatorRouteRecord *record
) {
    if (record == 0) {
        __builtin_trap();
    }
    return record->route;
}

RuntimeAllocatorRouteTuple
runtime_allocator_outlined_load_record_route_to_frame(
    const RuntimeAllocatorRouteRecord *record
) {
    return runtime_allocator_load_record_route(record);
}

RuntimeAllocatorRouteTuple runtime_allocator_outlined_load_record_route(
    const RuntimeAllocatorRouteRecord *record
) {
    return runtime_allocator_load_record_route(record);
}

RuntimeAllocatorRouteTuple runtime_allocator_boot_route_tuple(void) {
    return boot_route_tuple;
}
