#pragma once

#include "avp/runtime/ipc/runtime_svc_dispatch_recovered.h"

_Noreturn void runtime_svc_dispatch_recovered_trap(void);
RuntimeSVCDispatchRecoveredState *runtime_svc_dispatch_recovered_state(void);
bool runtime_svc_dispatch_recovered_contains(const void *current,
                                             const void *lower_bound,
                                             const void *upper_bound,
                                             size_t size);
