#include "avp/runtime/support/runtime_state.h"


#if defined(__ELF__)
#define RUNTIME_STATE_SECTION \
    __attribute__((section(".runtime_scheduler.state")))
#else
#define RUNTIME_STATE_SECTION
#endif

static RuntimeGlobalState runtime_global_state RUNTIME_STATE_SECTION;

void runtime_state_initialize(void) {
    runtime_global_state = (RuntimeGlobalState){
        .object_count = 1,
        .capability_count = 1,
    };
}

void runtime_state_publish_task_count(uint8_t task_count) {
    runtime_global_state.task_count = task_count;
}

void runtime_state_publish_domain(uint8_t domain, uint16_t runtime_handle) {
    runtime_global_state.current_domain = domain;
    runtime_global_state.runtime_handle = runtime_handle;
}

void runtime_state_publish_registry_counts(
    uint16_t object_count,
    uint16_t capability_count
) {
    runtime_global_state.object_count = object_count;
    runtime_global_state.capability_count = capability_count;
}

volatile RuntimeGlobalState *runtime_state_get(void) {
    return &runtime_global_state;
}
