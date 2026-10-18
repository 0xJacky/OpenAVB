#include "avp/runtime/exception/pvpanic.h"


_Noreturn void firmware_main(void) {
    pvpanic_initialize();
    __asm__ volatile("brk #0");
    __builtin_unreachable();
}
