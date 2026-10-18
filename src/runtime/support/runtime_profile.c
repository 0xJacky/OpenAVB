#include "avp/runtime/support/stack_guard.h"


void _start(void);

uintptr_t runtime_reset_entry_pointer(void) {
    return (uintptr_t)&_start;
}
