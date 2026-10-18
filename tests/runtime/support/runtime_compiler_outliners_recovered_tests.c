#include "avp/runtime/support/runtime_compiler_outliners_recovered.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>


static void expect_trap(void (*operation)(void)) {
    const pid_t child = fork();
    assert(child >= 0);
    if (child == 0) {
        operation();
        _exit(0);
    }
    int status = 0;
    assert(waitpid(child, &status, 0) == child);
    assert(WIFSIGNALED(status));
}

static void invoke_unbound_global_state(void) {
    (void)runtime_outlined_load_global_state_to_x22();
}

static void test_basic_atoms(void) {
    uint32_t output = UINT32_MAX;
    assert(!runtime_outlined_u32_zero_once_is_initialized());
    runtime_outlined_initialize_u32_zero_once(&output);
    assert(output == 0);
    assert(runtime_outlined_u32_zero_once_is_initialized());
    output = UINT32_MAX;
    runtime_outlined_initialize_u32_zero_once(&output);
    assert(output == 0);
    runtime_bootstrap_memory_optional_hook_noop();
    j_runtime_data_memory_barrier_system();
    assert(runtime_return_null() == 0);
    assert(runtime_return_minus_one() == -1);

    const RuntimeFirebloomSpan null_span =
        runtime_outlined_return_null_span_shared_tail();
    assert(null_span.current == 0);
    assert(null_span.lower_bound == 0);
    assert(null_span.upper_bound == 0);
    assert(null_span.type == 0);
}

static void test_triplets(void) {
    const uintptr_t values[] = {0x11, 0x22, 0x33};
    RuntimeCompilerOutlinerTriplet triplet =
        runtime_address_space_outlined_save_mapping_tuple_x21_x22_x20(
            values[0], values[1], values[2]
        );
    assert(triplet.first == values[0]);
    assert(triplet.second == values[1]);
    assert(triplet.third == values[2]);

    triplet = runtime_outlined_save_x0_x2_to_x20_x22(
        values[0], values[1], values[2]
    );
    assert(triplet.first == values[0]);
    assert(triplet.second == values[1]);
    assert(triplet.third == values[2]);
    triplet = runtime_outlined_save_x0_x2_to_x23_x25(
        values[0], values[1], values[2]
    );
    assert(triplet.first == values[0]);
    assert(triplet.second == values[1]);
    assert(triplet.third == values[2]);
    triplet = runtime_outlined_save_x0_x2_to_x19_x21(
        values[0], values[1], values[2]
    );
    assert(triplet.first == values[0]);
    assert(triplet.second == values[1]);
    assert(triplet.third == values[2]);

    const RuntimeCompilerOutlinerPair pair =
        runtime_outlined_save_x0_x1_to_x20_x19(values[0], values[1]);
    assert(pair.first == values[0]);
    assert(pair.second == values[1]);
}

static void test_spans_and_fixed_state(void) {
    uint8_t task_stack_state[0xa0] = {0};
    const RuntimeCompilerOutlinerSpanPrefix bitmap =
        runtime_task_stack_outlined_bitmap_span_x21(task_stack_state);
    assert(bitmap.current == task_stack_state + 0x80);
    assert(bitmap.lower_bound == task_stack_state + 0x80);
    assert(bitmap.upper_bound == task_stack_state + 0x90);

    const RuntimeFirebloomType type = {
        .identity = 1,
        .element_size = 1,
        .bytewise_copyable = true,
    };
    const RuntimeFirebloomSpan span =
        runtime_physical_region_outlined_restore_bounds_x20_x23_x24(
            task_stack_state + 4,
            task_stack_state,
            task_stack_state + sizeof(task_stack_state),
            &type
        );
    assert(span.current == task_stack_state + 4);
    assert(span.lower_bound == task_stack_state);
    assert(span.upper_bound == task_stack_state + sizeof(task_stack_state));
    assert(span.type == &type);

    uint64_t global_state = 0;
    const uint64_t platform_configuration = 1;
    runtime_compiler_outliners_bind_fixed_state(
        &global_state,
        &platform_configuration
    );
    assert(runtime_outlined_load_global_state_to_x22() == &global_state);
    assert(runtime_outlined_load_platform_configuration_to_x22() ==
        &platform_configuration);
}

static void test_task_interrupt_mask(void) {
    RuntimeTaskFrame frame = {0};
    assert(!runtime_task_interrupt_mask_test_bounded(&frame));
    frame.spsr_el1 = UINT64_C(0x40);
    assert(runtime_task_interrupt_mask_test_bounded(&frame));
    frame.spsr_el1 = UINT64_C(0x80);
    assert(runtime_task_interrupt_mask_test_bounded(&frame));
    frame.spsr_el1 = UINT64_C(0xc0);
    assert(runtime_task_interrupt_mask_test_bounded(&frame));
}

int main(void) {
    expect_trap(invoke_unbound_global_state);
    test_basic_atoms();
    test_triplets();
    test_spans_and_fixed_state();
    test_task_interrupt_mask();
    puts("runtime recovered compiler outliner tests passed");
    return 0;
}
