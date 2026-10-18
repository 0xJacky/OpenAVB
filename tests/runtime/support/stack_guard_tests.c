#include "avp/runtime/support/stack_guard.h"
#include "avp/runtime/prng/runtime_prng.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <string.h>


static uintptr_t reset_entry = 1;
static uint32_t research_pattern_reads;
static uint32_t read_only_thread_pointer_initializations;
static uint64_t generated_guard = UINT64_C(0x3f2800d6569e01b4);
static RuntimePRNG mock_prng;

extern uint64_t runtime_xorshift_state;
uintptr_t __stack_chk_guard;
uint64_t runtime_staged_stack_guard;

uintptr_t runtime_reset_entry_pointer(void) {
    return reset_entry;
}

uint64_t avp_time_counter(void) {
    return 0x0123456789abcdef;
}

uint64_t sep_read_research_pattern_u64(void) {
    ++research_pattern_reads;
    return 0xfedcba9876543210;
}

RuntimePRNG *runtime_prng_global(void) {
    return &mock_prng;
}

RuntimePRNGStatus runtime_prng_generate(
    RuntimePRNG *prng,
    uint8_t *output,
    size_t size
) {
    assert(prng == &mock_prng);
    assert(size == sizeof(generated_guard));
    memcpy(output, &generated_guard, size);
    return RUNTIME_PRNG_STATUS_OK;
}

void runtime_initialize_read_only_thread_pointer(void) {
    ++read_only_thread_pointer_initializations;
}

int main(void) {
    const uint64_t first = runtime_generate_stack_guard_seed();
    assert(first == 0x3f2800d6569e01b4);
    assert(runtime_generate_stack_guard_seed() == first);
    assert(research_pattern_reads == 0);

    reset_entry = 0;
    assert(runtime_generate_stack_guard_seed() == 0xfedcba9876543210);
    assert(research_pattern_reads == 1);

    reset_entry = 1;
    generated_guard = UINT64_C(0x606f949a3cebd0b7);
    assert(runtime_generate_stack_guard_seed() == generated_guard);
    assert(runtime_xorshift_state == generated_guard);

    runtime_stage_stack_guard(RUNTIME_BOOTSTRAP_STACK_GUARD);
    runtime_install_stack_guard();
    assert(runtime_stack_guard() == RUNTIME_BOOTSTRAP_STACK_GUARD);

    runtime_stage_stack_guard(
        0x0123456789abcdef & RUNTIME_STACK_GUARD_MASK
    );
    runtime_install_stack_guard();
    assert(runtime_stack_guard() == 0x0123456789ab00ef);
    assert(read_only_thread_pointer_initializations == 2);
}
