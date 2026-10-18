#include "runtime_bootstrap_memory_recovered_private.h"


static RuntimeBootstrapMemoryRecoveredState recovered_state;

RuntimeBootstrapMemoryRecoveredState *
runtime_bootstrap_memory_recovered_state(void) {
    return &recovered_state;
}
