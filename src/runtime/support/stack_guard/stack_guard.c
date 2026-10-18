#include "avp/protocol/sep/sep.h"
#include "avp/runtime/prng/runtime_prng.h"
#include "avp/runtime/support/stack_guard.h"
#if defined(__ELF__)
#define STACK_GUARD_STORAGE(section_name) \
    __attribute__((section(section_name), used))
#else
#define STACK_GUARD_STORAGE(section_name)
#endif

uint64_t runtime_xorshift_state
    STACK_GUARD_STORAGE(".stack_guard.xorshift");
extern uintptr_t __stack_chk_guard;
extern uint64_t runtime_staged_stack_guard;

void runtime_initialize_read_only_thread_pointer(void);

uint64_t runtime_generate_stack_guard_seed(void) {
    if (runtime_reset_entry_pointer() == 0) {
        return sep_read_research_pattern_u64();
    }

    uint64_t value = 0;
    if (runtime_prng_generate(
            runtime_prng_global(),
            (uint8_t *)&value,
            sizeof(value)
        ) != RUNTIME_PRNG_STATUS_OK || value == 0) {
        __builtin_trap();
    }
    runtime_xorshift_state = value;
    return value;
}

void runtime_stage_stack_guard(uint64_t guard) {
    runtime_staged_stack_guard = guard;
}

__attribute__((no_stack_protector))
void runtime_install_stack_guard(void) {
    __stack_chk_guard = runtime_staged_stack_guard;
    runtime_initialize_read_only_thread_pointer();
}

uint64_t runtime_stack_guard(void) {
    return __stack_chk_guard;
}
