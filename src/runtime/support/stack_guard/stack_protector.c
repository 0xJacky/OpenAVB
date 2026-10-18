#include "avp/runtime/support/runtime_diagnostics.h"

#include <stdint.h>


#if defined(__ELF__)
#define STACK_GUARD_STORAGE(section_name) \
    __attribute__((section(section_name), used))
#else
#define STACK_GUARD_STORAGE(section_name)
#endif

uintptr_t __stack_chk_guard
    STACK_GUARD_STORAGE(".stack_guard.active");
uint64_t runtime_staged_stack_guard
    STACK_GUARD_STORAGE(".stack_guard.staging");

_Noreturn __attribute__((no_stack_protector))
void __stack_chk_fail(void) {
    runtime_stack_guard_fail();
}
