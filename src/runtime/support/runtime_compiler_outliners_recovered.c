#include "avp/runtime/support/runtime_compiler_outliners_recovered.h"

#if defined(__APPLE__) || !defined(__aarch64__)
#include <stdatomic.h>
#endif


static bool u32_zero_once_initialized;
static void *bound_global_state;
static const void *bound_platform_configuration;

static _Noreturn void runtime_compiler_outliner_fail(void) {
    __builtin_trap();
}

static __attribute__((noinline)) RuntimeCompilerOutlinerTriplet
runtime_compiler_make_triplet(
    uintptr_t first,
    uintptr_t second,
    uintptr_t third
) {
    /* One shared constructor keeps the target compiler from re-outlining ABI. */
    return (RuntimeCompilerOutlinerTriplet){first, second, third};
}

void runtime_compiler_outliners_bind_fixed_state(
    void *global_state,
    const void *platform_configuration
) {
    if (global_state == 0 || platform_configuration == 0) {
        runtime_compiler_outliner_fail();
    }
    bound_global_state = global_state;
    bound_platform_configuration = platform_configuration;
}

#if defined(__APPLE__) || !defined(__aarch64__)
void j_runtime_data_memory_barrier_system(void) {
    /* Sequential consistency is the host model of the AArch64 DMB SY leaf. */
    atomic_thread_fence(memory_order_seq_cst);
}
#endif

void runtime_outlined_initialize_u32_zero_once(uint32_t *output) {
    if (output == 0) {
        runtime_compiler_outliner_fail();
    }
    *output = 0;
    if (!u32_zero_once_initialized) {
        u32_zero_once_initialized = true;
    }
    *output = 0;
}

bool runtime_outlined_u32_zero_once_is_initialized(void) {
    return u32_zero_once_initialized;
}

void runtime_bootstrap_memory_optional_hook_noop(void) {
}

RuntimeFirebloomSpan runtime_outlined_return_null_span_shared_tail(void) {
    return (RuntimeFirebloomSpan){0};
}

bool runtime_task_interrupt_mask_test_bounded(const RuntimeTaskFrame *task) {
    if (task == 0) {
        runtime_compiler_outliner_fail();
    }
    return (task->spsr_el1 & UINT64_C(0xc0)) != 0;
}

RuntimeCompilerOutlinerTriplet
runtime_address_space_outlined_save_mapping_tuple_x21_x22_x20(
    uintptr_t current,
    uintptr_t lower_bound,
    uintptr_t upper_bound
) {
    return runtime_compiler_make_triplet(current, lower_bound, upper_bound);
}

RuntimeFirebloomSpan
runtime_physical_region_outlined_restore_bounds_x20_x23_x24(
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

RuntimeCompilerOutlinerTriplet runtime_outlined_save_x0_x2_to_x20_x22(
    uintptr_t value0,
    uintptr_t value1,
    uintptr_t value2
) {
    return runtime_compiler_make_triplet(value0, value1, value2);
}

RuntimeCompilerOutlinerSpanPrefix
runtime_task_stack_outlined_bitmap_span_x21(void *task_stack_state) {
    uint8_t *const bitmap = (uint8_t *)task_stack_state + 0x80;
    return (RuntimeCompilerOutlinerSpanPrefix){
        .current = bitmap,
        .lower_bound = bitmap,
        .upper_bound = bitmap + 0x10,
    };
}

RuntimeCompilerOutlinerPair runtime_outlined_save_x0_x1_to_x20_x19(
    uintptr_t value0,
    uintptr_t value1
) {
    return (RuntimeCompilerOutlinerPair){value0, value1};
}

RuntimeCompilerOutlinerTriplet runtime_outlined_save_x0_x2_to_x23_x25(
    uintptr_t value0,
    uintptr_t value1,
    uintptr_t value2
) {
    return runtime_compiler_make_triplet(value0, value1, value2);
}

void *runtime_outlined_load_global_state_to_x22(void) {
    if (bound_global_state == 0) {
        runtime_compiler_outliner_fail();
    }
    return bound_global_state;
}

const void *runtime_outlined_load_platform_configuration_to_x22(void) {
    if (bound_platform_configuration == 0) {
        runtime_compiler_outliner_fail();
    }
    return bound_platform_configuration;
}

RuntimeCompilerOutlinerTriplet runtime_outlined_save_x0_x2_to_x19_x21(
    uintptr_t value0,
    uintptr_t value1,
    uintptr_t value2
) {
    return runtime_compiler_make_triplet(value0, value1, value2);
}

void *runtime_return_null(void) {
    return 0;
}

int runtime_return_minus_one(void) {
    return -1;
}
