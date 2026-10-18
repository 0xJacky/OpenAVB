#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "avp/runtime/firebloom/runtime_firebloom.h"
#include "avp/runtime/task/runtime_task.h"


#define RUNTIME_COMPILER_OUTLINER_PUBLIC __attribute__((used, retain))

typedef struct {
    uintptr_t first;
    uintptr_t second;
} RuntimeCompilerOutlinerPair;

typedef struct {
    uintptr_t first;
    uintptr_t second;
    uintptr_t third;
} RuntimeCompilerOutlinerTriplet;

typedef struct {
    void *current;
    void *lower_bound;
    void *upper_bound;
} RuntimeCompilerOutlinerSpanPrefix;

void runtime_compiler_outliners_bind_fixed_state(
    void *global_state,
    const void *platform_configuration
);
bool runtime_outlined_u32_zero_once_is_initialized(void);

void j_runtime_data_memory_barrier_system(void);
RUNTIME_COMPILER_OUTLINER_PUBLIC void
runtime_outlined_initialize_u32_zero_once(uint32_t *output);
RUNTIME_COMPILER_OUTLINER_PUBLIC void
runtime_bootstrap_memory_optional_hook_noop(void);
RUNTIME_COMPILER_OUTLINER_PUBLIC RuntimeFirebloomSpan
runtime_outlined_return_null_span_shared_tail(void);

void runtime_outlined_frame90_restore_x19_x28(void);
void runtime_outlined_frame90_save_x19_x28(void);
void runtime_outlined_frame_c0_save_x19_x28(void);
void runtime_outlined_frame70_restore_x19_x28(void);
void runtime_outlined_frame_a0_restore_x19_x28(void);
void runtime_outlined_frame70_save_x19_x28(void);

RUNTIME_COMPILER_OUTLINER_PUBLIC bool
runtime_task_interrupt_mask_test_bounded(const RuntimeTaskFrame *task);
RUNTIME_COMPILER_OUTLINER_PUBLIC RuntimeCompilerOutlinerTriplet
runtime_address_space_outlined_save_mapping_tuple_x21_x22_x20(
    uintptr_t current,
    uintptr_t lower_bound,
    uintptr_t upper_bound
);
RUNTIME_COMPILER_OUTLINER_PUBLIC RuntimeFirebloomSpan
runtime_physical_region_outlined_restore_bounds_x20_x23_x24(
    void *current,
    void *lower_bound,
    void *upper_bound,
    const RuntimeFirebloomType *type
);
RUNTIME_COMPILER_OUTLINER_PUBLIC RuntimeCompilerOutlinerTriplet
runtime_outlined_save_x0_x2_to_x20_x22(
    uintptr_t value0,
    uintptr_t value1,
    uintptr_t value2
);
RUNTIME_COMPILER_OUTLINER_PUBLIC RuntimeCompilerOutlinerSpanPrefix
runtime_task_stack_outlined_bitmap_span_x21(void *task_stack_state);
RUNTIME_COMPILER_OUTLINER_PUBLIC RuntimeCompilerOutlinerPair
runtime_outlined_save_x0_x1_to_x20_x19(
    uintptr_t value0,
    uintptr_t value1
);
RUNTIME_COMPILER_OUTLINER_PUBLIC RuntimeCompilerOutlinerTriplet
runtime_outlined_save_x0_x2_to_x23_x25(
    uintptr_t value0,
    uintptr_t value1,
    uintptr_t value2
);
RUNTIME_COMPILER_OUTLINER_PUBLIC void *
runtime_outlined_load_global_state_to_x22(void);
RUNTIME_COMPILER_OUTLINER_PUBLIC const void *
runtime_outlined_load_platform_configuration_to_x22(void);
RUNTIME_COMPILER_OUTLINER_PUBLIC RuntimeCompilerOutlinerTriplet
runtime_outlined_save_x0_x2_to_x19_x21(
    uintptr_t value0,
    uintptr_t value1,
    uintptr_t value2
);
RUNTIME_COMPILER_OUTLINER_PUBLIC void *runtime_return_null(void);
RUNTIME_COMPILER_OUTLINER_PUBLIC int runtime_return_minus_one(void);
