#include "avp/platform/platform.h"
#include "avp/runtime/exception/pvpanic.h"
#include "avp/protocol/sep/sep.h"
#include "avp/runtime/support/stack_guard.h"


enum {
    RUNTIME_TRANSPORT_PROBE_STAGE = 0x52545054,
};

/* This probe intentionally installs a new guard after SEP bootstrap. */
__attribute__((no_stack_protector))
void firmware_main(void) {
    pvpanic_initialize();
    platform_stage(RUNTIME_TRANSPORT_PROBE_STAGE);

    const SEPBootstrapResult bootstrap = sep_bootstrap_with_finalize();
    const uint64_t pattern = sep_read_research_pattern_u64();
    runtime_stage_stack_guard(pattern & RUNTIME_STACK_GUARD_MASK);
    runtime_install_stack_guard();
    platform_progress(0, (uint32_t)runtime_stack_guard());
    platform_progress(1, (uint32_t)(runtime_stack_guard() >> 32));
    platform_progress(2, bootstrap.finalize_rounds);
    uint64_t read_only_thread_pointer;
    __asm__ volatile("mrs %0, tpidrro_el0" : "=r"(read_only_thread_pointer));
    platform_progress(3, read_only_thread_pointer == UINT64_MAX);
    pvpanic_trigger();
}
