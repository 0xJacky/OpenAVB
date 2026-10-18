#include "avp/runtime/memory/runtime_bootstrap_memory.h"

#include "avp/runtime/ipc/runtime_domain.h"
#include "avp/runtime/task/runtime_task.h"

#if defined(__ELF__)
#define AVP_BOOTSTRAP_OWNER_RECOVERED \
    __attribute__((noinline, section(".text.runtime_bootstrap_memory_recovered")))
#else
#define AVP_BOOTSTRAP_OWNER_RECOVERED __attribute__((noinline))
#endif

AVP_BOOTSTRAP_OWNER_RECOVERED
bool runtime_bootstrap_memory_is_boot_owner(void) {
    return runtime_current_domain() == AVP_RUNTIME_DOMAIN_BOOT &&
        runtime_bootstrap_memory_is_boot_task();
}

AVP_BOOTSTRAP_OWNER_RECOVERED
bool runtime_bootstrap_memory_is_boot_task(void) {
    return runtime_task_current() == runtime_task_lookup_ordinal(0);
}
