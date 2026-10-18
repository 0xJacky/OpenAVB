#include "avp/runtime/support/runtime_stack_guard_helpers.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>


uintptr_t __stack_chk_guard;
static uint64_t generated_seed;
static uint32_t generate_count;

uint64_t runtime_generate_stack_guard_seed(void) {
    ++generate_count;
    return generated_seed;
}

static void test_load_and_compare_clones(void) {
    const uint64_t guard = UINT64_C(0x123456789abc00ef);
    __stack_chk_guard = guard;
    assert(runtime_stack_guard_load() == guard);
    assert(runtime_stack_guard_load_to_x8() == guard);
    assert(runtime_stack_guard_load_to_x8_clone() == guard);

    assert(runtime_stack_guard_compare(guard));
    assert(boot_image_stack_guard_compare_saved_x8(guard));
    assert(runtime_stack_guard_compare_saved_x8_clone(guard));
    assert(runtime_stack_guard_compare_saved_x8(guard));
    assert(runtime_stack_guard_compare_saved_x9(guard));

    const uint64_t corrupted = guard ^ UINT64_C(1);
    assert(!runtime_stack_guard_compare(corrupted));
    assert(!boot_image_stack_guard_compare_saved_x8(corrupted));
    assert(!runtime_stack_guard_compare_saved_x8_clone(corrupted));
    assert(!runtime_stack_guard_compare_saved_x8(corrupted));
    assert(!runtime_stack_guard_compare_saved_x9(corrupted));
}

static void test_seed_normalization(void) {
    generate_count = 0;
    assert(runtime_stack_guard_seed_normalize(
        UINT64_C(0xaabbccddeeff7788)
    ) == UINT64_C(0xaabbccddeeff0088));
    assert(generate_count == 0);

    generated_seed = UINT64_C(0x11223344556677aa);
    assert(runtime_stack_guard_seed_normalize(0) ==
        UINT64_C(0x11223344556600aa));
    assert(generate_count == 1);
}

int main(void) {
    runtime_dynamic_stack_allocation_probe_noop();
    test_load_and_compare_clones();
    test_seed_normalization();
    puts("runtime stack guard helper tests passed");
    return 0;
}
