#include "avp/runtime/exception/pvpanic.h"
#include "avp/runtime/memory/runtime_memory.h"


_Noreturn void firmware_main(void) {
    pvpanic_initialize();
    runtime_firebloom_panic();
}
