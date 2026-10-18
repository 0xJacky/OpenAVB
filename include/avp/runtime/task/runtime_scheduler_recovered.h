#pragma once

#include "avp/runtime/firebloom/runtime_firebloom.h"
#include "avp/runtime/task/runtime_task.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if __has_attribute(retain)
#define RUNTIME_SCHEDULER_RECOVERED_PUBLIC __attribute__((used, retain))
#else
#define RUNTIME_SCHEDULER_RECOVERED_PUBLIC __attribute__((used))
#endif

typedef struct {
  void *current;
  void *lower_bound;
  void *upper_bound;
  const RuntimeFirebloomType *type;
} RuntimeSchedulerRecoveredSpan;

_Static_assert(sizeof(RuntimeSchedulerRecoveredSpan) == 0x20,
               "scheduler spans preserve the recovered four-register ABI");

typedef uint64_t (*RuntimeSchedulerRecoveredCounter)(void *context);
typedef void (*RuntimeSchedulerRecoveredDeadlineProgram)(
  void *context,
  uint64_t deadline
);
typedef void (*RuntimeSchedulerRecoveredTimerMask)(void *context);
typedef void (*RuntimeSchedulerRecoveredTaskPublish)(
  void *context,
  RuntimeTask *task
);
typedef void (*RuntimeSchedulerRecoveredTaskSwitch)(
  void *context,
  RuntimeTask *previous,
  RuntimeTask *next
);
typedef void (*RuntimeSchedulerRecoveredInterruptReschedule)(
  void *context,
  uint64_t deadline,
  void (*callback)(void),
  const void *signature_descriptor
);

typedef struct {
  void *context;
  RuntimeSchedulerRecoveredCounter counter;
  RuntimeSchedulerRecoveredDeadlineProgram program_deadline;
  RuntimeSchedulerRecoveredTimerMask mask_timer;
  RuntimeSchedulerRecoveredTaskPublish publish_task;
  RuntimeSchedulerRecoveredTaskSwitch switch_task;
  RuntimeSchedulerRecoveredInterruptReschedule reschedule_interrupt_drain;
} RuntimeSchedulerRecoveredHooks;

typedef struct {
  RuntimeTaskQueueLink ready_queue_root;
  RuntimeTaskQueueLink timer_queue_root;
  RuntimeTaskQueueLink interrupt_timer_queue_root;
  RuntimeTask *current_task;
  RuntimeTask *idle_task;
  RuntimeTimerEvent *global_deadline_event;
  uintptr_t current_lower_bound;
  uintptr_t current_upper_bound;
  const RuntimeFirebloomType *current_type;
  uint32_t ready_count;
  uint32_t timer_count;
  uint32_t interrupt_timer_count;
  uint32_t switch_count;
  uint32_t timer_callbacks;
  uint32_t interrupt_timer_callbacks;
  bool global_deadline_armed;
  bool global_deadline_complete;
  bool timer_masked;
  uint64_t programmed_deadline;
  uint64_t interrupt_programmed_deadline;
  const void *interrupt_signature_descriptor;
  RuntimeSchedulerRecoveredHooks hooks;
} RuntimeSchedulerRecoveredState;

typedef struct {
  void (*callback)(void);
  const void *signature_descriptor;
} RuntimeSchedulerRecoveredRescheduleCallback;

extern const RuntimeFirebloomType
    runtime_scheduler_recovered_timer_event_type;

void runtime_scheduler_recovered_initialize(
  RuntimeSchedulerRecoveredState *state,
  RuntimeTask *idle_task,
  RuntimeTimerEvent *global_deadline_event,
  RuntimeSchedulerRecoveredHooks hooks
);
void runtime_scheduler_recovered_install(RuntimeSchedulerRecoveredState *state);
RuntimeSchedulerRecoveredState *runtime_scheduler_recovered_state(void);
void runtime_scheduler_recovered_interrupt_timer_arm(
  RuntimeTimerEvent *event,
  uint64_t deadline,
  RuntimeTimerCallback callback,
  void *callback_metadata,
  RuntimeTask *owner
);

RUNTIME_SCHEDULER_RECOVERED_PUBLIC RuntimeSchedulerRecoveredSpan
runtime_scheduler_outlined_prepare_bootstrap_task_u64_store(
  const RuntimeFirebloomType *type_context,
  void *runtime_state
);
RUNTIME_SCHEDULER_RECOVERED_PUBLIC RuntimeSchedulerRecoveredSpan
runtime_timer_event_tuple_validate(
  RuntimeTimerEvent *current,
  const RuntimeTimerEvent *lower_bound,
  const RuntimeTimerEvent *upper_bound,
  const RuntimeFirebloomType *type_context
);
RUNTIME_SCHEDULER_RECOVERED_PUBLIC RuntimeSchedulerRecoveredSpan
runtime_interrupt_timer_event_tuple_validate(
  RuntimeTimerEvent *current,
  const RuntimeTimerEvent *lower_bound,
  const RuntimeTimerEvent *upper_bound,
  const RuntimeFirebloomType *type_context
);
RUNTIME_SCHEDULER_RECOVERED_PUBLIC RuntimeSchedulerRecoveredSpan
runtime_interrupt_timer_queue_front(
  RuntimeTaskQueueLink *root,
  RuntimeTaskQueueLink *lower_bound,
  RuntimeTaskQueueLink *upper_bound
);
RUNTIME_SCHEDULER_RECOVERED_PUBLIC _Noreturn void
runtime_interrupt_timer_queue_front_bounds_panic(void);
RUNTIME_SCHEDULER_RECOVERED_PUBLIC void
runtime_interrupt_timer_event_unlink(
  RuntimeTimerEvent *event,
  RuntimeTimerEvent *lower_bound,
  RuntimeTimerEvent *upper_bound
);
RUNTIME_SCHEDULER_RECOVERED_PUBLIC void
runtime_timer_event_unlink_if_armed(
  RuntimeTimerEvent *event,
  RuntimeTimerEvent *lower_bound,
  RuntimeTimerEvent *upper_bound
);
RUNTIME_SCHEDULER_RECOVERED_PUBLIC void runtime_timer_event_arm(
  RuntimeTimerEvent *event,
  RuntimeTimerEvent *event_lower_bound,
  RuntimeTimerEvent *event_upper_bound,
  RuntimeTimerCallback callback,
  void *callback_metadata,
  RuntimeTask *owner,
  const RuntimeTask *owner_lower_bound,
  const RuntimeTask *owner_upper_bound,
  const RuntimeFirebloomType *owner_type,
  uint64_t deadline
);
RUNTIME_SCHEDULER_RECOVERED_PUBLIC void
runtime_timer_drain_expired_events(void);
RUNTIME_SCHEDULER_RECOVERED_PUBLIC void
runtime_interrupt_timer_drain_expired_events(void);
RUNTIME_SCHEDULER_RECOVERED_PUBLIC RuntimeSchedulerRecoveredRescheduleCallback
runtime_interrupt_timer_outlined_prepare_reschedule_callback(void);
RUNTIME_SCHEDULER_RECOVERED_PUBLIC uint64_t
runtime_interrupt_timer_source_token(void);
RUNTIME_SCHEDULER_RECOVERED_PUBLIC void runtime_global_deadline_expired(
  RuntimeTimerEvent *event,
  void *context
);
RUNTIME_SCHEDULER_RECOVERED_PUBLIC _Noreturn void
runtime_global_deadline_identity_failure(void);
RUNTIME_SCHEDULER_RECOVERED_PUBLIC void runtime_scheduler_ready_enqueue(
  RuntimeTask *task,
  RuntimeTask *task_lower_bound,
  RuntimeTask *task_upper_bound
);
RUNTIME_SCHEDULER_RECOVERED_PUBLIC void runtime_task_publish_current(
  RuntimeTask *task,
  RuntimeTask *lower_bound,
  RuntimeTask *upper_bound,
  const RuntimeFirebloomType *task_type
);
RUNTIME_SCHEDULER_RECOVERED_PUBLIC void runtime_scheduler_reschedule(void);
