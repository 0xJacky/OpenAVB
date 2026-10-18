#pragma once

#include "avp/runtime/task/runtime_scheduler_recovered.h"

_Noreturn void runtime_scheduler_recovered_trap(void);
RuntimeSchedulerRecoveredState *runtime_scheduler_recovered_require_state(void);
bool runtime_scheduler_recovered_contains(
    const void *current,
    const void *lower_bound,
    const void *upper_bound,
    size_t size
);
RuntimeTaskQueueLink *runtime_scheduler_recovered_event_link(
    RuntimeTimerEvent *event);
RuntimeTimerEvent *runtime_scheduler_recovered_timer_head(
    RuntimeTaskQueueLink *root);
void runtime_scheduler_recovered_link_timer_before(
    RuntimeTaskQueueLink *root,
    RuntimeTimerEvent *position,
    RuntimeTimerEvent *event
);
void runtime_scheduler_recovered_unlink_timer(
    RuntimeTaskQueueLink *root,
    RuntimeTimerEvent *event
);
void runtime_scheduler_recovered_program_timer_head(void);
