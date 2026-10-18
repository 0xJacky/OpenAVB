#pragma once

#include <stdbool.h>
#include <stdint.h>

#define RUNTIME_STACK_GUARD_PUBLIC __attribute__((used, retain))

RUNTIME_STACK_GUARD_PUBLIC void runtime_dynamic_stack_allocation_probe_noop(void);
RUNTIME_STACK_GUARD_PUBLIC bool boot_image_stack_guard_compare_saved_x8(
    uint64_t saved_guard
);
RUNTIME_STACK_GUARD_PUBLIC uint64_t runtime_stack_guard_load_to_x8_clone(void);
RUNTIME_STACK_GUARD_PUBLIC bool runtime_stack_guard_compare_saved_x8_clone(
    uint64_t saved_guard
);
RUNTIME_STACK_GUARD_PUBLIC bool runtime_stack_guard_compare(uint64_t saved_guard);
RUNTIME_STACK_GUARD_PUBLIC uint64_t runtime_stack_guard_load(void);
RUNTIME_STACK_GUARD_PUBLIC bool runtime_stack_guard_compare_saved_x8(
    uint64_t saved_guard
);
RUNTIME_STACK_GUARD_PUBLIC uint64_t runtime_stack_guard_load_to_x8(void);
RUNTIME_STACK_GUARD_PUBLIC bool runtime_stack_guard_compare_saved_x9(
    uint64_t saved_guard
);
RUNTIME_STACK_GUARD_PUBLIC uint64_t runtime_stack_guard_seed_normalize(
    uint64_t seed
);
