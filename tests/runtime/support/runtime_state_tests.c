#include "avp/runtime/support/runtime_state.h"

#include <assert.h>


int main(void) {
    runtime_state_initialize();
    volatile RuntimeGlobalState *state = runtime_state_get();
    assert(state->task_count == 0);
    assert(state->current_domain == 0);
    assert(state->runtime_handle == 0);
    assert(state->object_count == 1);
    assert(state->capability_count == 1);

    runtime_state_publish_task_count(7);
    runtime_state_publish_domain(3, UINT16_C(0x125));
    runtime_state_publish_registry_counts(11, 19);
    assert(state->task_count == 7);
    assert(state->current_domain == 3);
    assert(state->runtime_handle == UINT16_C(0x125));
    assert(state->object_count == 11);
    assert(state->capability_count == 19);
}
