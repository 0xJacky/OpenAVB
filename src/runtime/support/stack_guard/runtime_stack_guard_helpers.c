#include "avp/runtime/support/runtime_stack_guard_helpers.h"

#include "avp/runtime/support/stack_guard.h"

#include <stdint.h>


/*
 * Related public stack protector runtimes:
 * LLVM compiler-rt c1a4cee (Apache-2.0 WITH LLVM-exception):
 * https://github.com/llvm/llvm-project/commit/c1a4ceefe5c24373edd63d1a8073a95587f072f5
 * musl f21a965 __stack_chk_fail (MIT):
 * https://github.com/ifduyue/musl/commit/f21a96538f78fa8e2040831b4209b35f2fb581da
 * AVP switches one active guard with task context, so all compatible clones
 * deliberately read the same project-owned __stack_chk_guard storage.
 */

extern uintptr_t __stack_chk_guard;

#if defined(__ELF__)
#define RUNTIME_STACK_GUARD_HELPER \
    __attribute__((noinline, section(".text.runtime_stack_guard_helpers")))
#else
#define RUNTIME_STACK_GUARD_HELPER __attribute__((noinline))
#endif

RUNTIME_STACK_GUARD_HELPER
void runtime_dynamic_stack_allocation_probe_noop(void) {
}

RUNTIME_STACK_GUARD_HELPER
uint64_t runtime_stack_guard_load(void) {
    return __stack_chk_guard;
}

RUNTIME_STACK_GUARD_HELPER
uint64_t runtime_stack_guard_load_to_x8(void) {
    return runtime_stack_guard_load();
}

RUNTIME_STACK_GUARD_HELPER
uint64_t runtime_stack_guard_load_to_x8_clone(void) {
    return runtime_stack_guard_load();
}

RUNTIME_STACK_GUARD_HELPER
bool runtime_stack_guard_compare(uint64_t saved_guard) {
    return runtime_stack_guard_load() == saved_guard;
}

RUNTIME_STACK_GUARD_HELPER
bool boot_image_stack_guard_compare_saved_x8(uint64_t saved_guard) {
    return runtime_stack_guard_compare(saved_guard);
}

RUNTIME_STACK_GUARD_HELPER
bool runtime_stack_guard_compare_saved_x8_clone(uint64_t saved_guard) {
    return runtime_stack_guard_compare(saved_guard);
}

RUNTIME_STACK_GUARD_HELPER
bool runtime_stack_guard_compare_saved_x8(uint64_t saved_guard) {
    return runtime_stack_guard_compare(saved_guard);
}

RUNTIME_STACK_GUARD_HELPER
bool runtime_stack_guard_compare_saved_x9(uint64_t saved_guard) {
    return runtime_stack_guard_compare(saved_guard);
}

RUNTIME_STACK_GUARD_HELPER
uint64_t runtime_stack_guard_seed_normalize(uint64_t seed) {
    if (seed == 0) {
        seed = runtime_generate_stack_guard_seed();
    }
    return seed & UINT64_C(0xffffffffffff00ff);
}
