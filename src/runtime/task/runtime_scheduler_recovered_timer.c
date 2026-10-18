#include "runtime_scheduler_recovered_private.h"


static uint64_t runtime_scheduler_counter(RuntimeSchedulerRecoveredState *state) {
  return state->hooks.counter == 0
             ? 0
             : state->hooks.counter(state->hooks.context);
}

void runtime_scheduler_recovered_program_timer_head(void) {
  RuntimeSchedulerRecoveredState *state =
      runtime_scheduler_recovered_require_state();
  RuntimeTimerEvent *head = runtime_scheduler_recovered_timer_head(
      &state->timer_queue_root);
  if (head == 0) {
    state->timer_masked = true;
    state->programmed_deadline = 0;
    if (state->hooks.mask_timer != 0) {
      state->hooks.mask_timer(state->hooks.context);
    }
    return;
  }
  state->timer_masked = false;
  state->programmed_deadline = head->deadline;
  if (state->hooks.program_deadline != 0) {
    state->hooks.program_deadline(state->hooks.context, head->deadline);
  }
}

static void runtime_scheduler_require_event_bounds(
    RuntimeTimerEvent *event,
    RuntimeTimerEvent *lower_bound,
    RuntimeTimerEvent *upper_bound
) {
  if (!runtime_scheduler_recovered_contains(event, lower_bound, upper_bound,
                                            sizeof(*event))) {
    runtime_scheduler_recovered_trap();
  }
}

static bool runtime_scheduler_timer_event_is_armed(
    const RuntimeTimerEvent *event) {
  return event->queue_link.previous != 0 || event->queue_link.next != 0;
}

void RUNTIME_SCHEDULER_RECOVERED_PUBLIC
runtime_timer_event_unlink_if_armed(
    RuntimeTimerEvent *event,
    RuntimeTimerEvent *lower_bound,
    RuntimeTimerEvent *upper_bound
) {
  runtime_scheduler_require_event_bounds(event, lower_bound, upper_bound);
  if (!runtime_scheduler_timer_event_is_armed(event)) {
    return;
  }
  RuntimeSchedulerRecoveredState *state =
      runtime_scheduler_recovered_require_state();
  runtime_scheduler_recovered_unlink_timer(&state->timer_queue_root, event);
  if (state->timer_count == 0) {
    runtime_scheduler_recovered_trap();
  }
  --state->timer_count;
  runtime_scheduler_recovered_program_timer_head();
}

void RUNTIME_SCHEDULER_RECOVERED_PUBLIC runtime_timer_event_arm(
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
) {
  runtime_scheduler_require_event_bounds(
      event,
      event_lower_bound,
      event_upper_bound
  );
  if (callback == 0 || owner == 0 || owner_type == 0 ||
      !runtime_scheduler_recovered_contains(owner, owner_lower_bound,
                                            owner_upper_bound,
                                            sizeof(*owner))) {
    runtime_scheduler_recovered_trap();
  }
  runtime_timer_event_unlink_if_armed(
      event,
      event_lower_bound,
      event_upper_bound
  );
  event->deadline = deadline;
  event->reserved_48 = 0;
  event->callback = callback;
  event->callback_metadata = callback_metadata;
  event->owner = owner;
  event->owner_lower_bound = (uintptr_t)owner_lower_bound;
  event->owner_upper_bound = (uintptr_t)owner_upper_bound;
  event->owner_cookie = (uintptr_t)owner_type;

  RuntimeSchedulerRecoveredState *state =
      runtime_scheduler_recovered_require_state();
  RuntimeTimerEvent *cursor = runtime_scheduler_recovered_timer_head(
      &state->timer_queue_root);
  while (cursor != 0 && cursor->deadline <= deadline) {
    RuntimeTaskQueueLink *next = cursor->queue_link.next;
    cursor = next == &state->timer_queue_root ? 0 : (RuntimeTimerEvent *)next;
  }
  runtime_scheduler_recovered_link_timer_before(
      &state->timer_queue_root,
      cursor,
      event
  );
  ++state->timer_count;
  runtime_scheduler_recovered_program_timer_head();
}

void RUNTIME_SCHEDULER_RECOVERED_PUBLIC
runtime_timer_drain_expired_events(void) {
  RuntimeSchedulerRecoveredState *state =
      runtime_scheduler_recovered_require_state();
  for (;;) {
    RuntimeTimerEvent *event = runtime_scheduler_recovered_timer_head(
        &state->timer_queue_root);
    if (event == 0 || event->deadline > runtime_scheduler_counter(state)) {
      break;
    }
    runtime_scheduler_recovered_unlink_timer(&state->timer_queue_root, event);
    if (state->timer_count == 0 || event->callback == 0) {
      runtime_scheduler_recovered_trap();
    }
    --state->timer_count;
    RuntimeTimerCallback callback = event->callback;
    callback(event, event->owner);
    ++state->timer_callbacks;
  }
  runtime_scheduler_recovered_program_timer_head();
}

void runtime_scheduler_recovered_interrupt_timer_arm(
    RuntimeTimerEvent *event,
    uint64_t deadline,
    RuntimeTimerCallback callback,
    void *callback_metadata,
    RuntimeTask *owner
) {
  if (event == 0 || callback == 0 || owner == 0 ||
      runtime_scheduler_timer_event_is_armed(event)) {
    runtime_scheduler_recovered_trap();
  }
  event->deadline = deadline;
  event->callback = callback;
  event->callback_metadata = callback_metadata;
  event->owner = owner;
  event->owner_lower_bound = (uintptr_t)owner;
  event->owner_upper_bound = (uintptr_t)(owner + 1);
  event->owner_cookie = 0;
  RuntimeSchedulerRecoveredState *state =
      runtime_scheduler_recovered_require_state();
  RuntimeTimerEvent *cursor = runtime_scheduler_recovered_timer_head(
      &state->interrupt_timer_queue_root);
  while (cursor != 0 && cursor->deadline <= deadline) {
    RuntimeTaskQueueLink *next = cursor->queue_link.next;
    cursor = next == &state->interrupt_timer_queue_root
                 ? 0
                 : (RuntimeTimerEvent *)next;
  }
  runtime_scheduler_recovered_link_timer_before(
      &state->interrupt_timer_queue_root,
      cursor,
      event
  );
  ++state->interrupt_timer_count;
}

void RUNTIME_SCHEDULER_RECOVERED_PUBLIC
runtime_interrupt_timer_event_unlink(
    RuntimeTimerEvent *event,
    RuntimeTimerEvent *lower_bound,
    RuntimeTimerEvent *upper_bound
) {
  runtime_scheduler_require_event_bounds(event, lower_bound, upper_bound);
  RuntimeSchedulerRecoveredState *state =
      runtime_scheduler_recovered_require_state();
  runtime_scheduler_recovered_unlink_timer(
      &state->interrupt_timer_queue_root,
      event
  );
  if (state->interrupt_timer_count == 0) {
    runtime_scheduler_recovered_trap();
  }
  --state->interrupt_timer_count;
}

static void runtime_scheduler_interrupt_drain_callback(void) {
  runtime_interrupt_timer_drain_expired_events();
}

RuntimeSchedulerRecoveredRescheduleCallback
    RUNTIME_SCHEDULER_RECOVERED_PUBLIC
runtime_interrupt_timer_outlined_prepare_reschedule_callback(void) {
  RuntimeSchedulerRecoveredState *state =
      runtime_scheduler_recovered_require_state();
  return (RuntimeSchedulerRecoveredRescheduleCallback){
      .callback = runtime_scheduler_interrupt_drain_callback,
      .signature_descriptor = state->interrupt_signature_descriptor,
  };
}

void RUNTIME_SCHEDULER_RECOVERED_PUBLIC
runtime_interrupt_timer_drain_expired_events(void) {
  RuntimeSchedulerRecoveredState *state =
      runtime_scheduler_recovered_require_state();
  for (;;) {
    RuntimeSchedulerRecoveredSpan front = runtime_interrupt_timer_queue_front(
        &state->interrupt_timer_queue_root,
        &state->interrupt_timer_queue_root,
        &state->interrupt_timer_queue_root + 1
    );
    if (front.current == 0) {
      state->interrupt_programmed_deadline = 0;
      return;
    }
    RuntimeTimerEvent *event = front.current;
    if (event->deadline > runtime_scheduler_counter(state)) {
      RuntimeSchedulerRecoveredRescheduleCallback reschedule =
          runtime_interrupt_timer_outlined_prepare_reschedule_callback();
      state->interrupt_programmed_deadline = event->deadline;
      if (state->hooks.reschedule_interrupt_drain != 0) {
        state->hooks.reschedule_interrupt_drain(
            state->hooks.context,
            event->deadline,
            reschedule.callback,
            reschedule.signature_descriptor
        );
      }
      return;
    }
    runtime_interrupt_timer_event_unlink(event, event, event + 1);
    if (event->callback == 0) {
      runtime_scheduler_recovered_trap();
    }
    RuntimeTimerCallback callback = event->callback;
    callback(event, event->owner);
    ++state->interrupt_timer_callbacks;
  }
}

void RUNTIME_SCHEDULER_RECOVERED_PUBLIC runtime_global_deadline_expired(
    RuntimeTimerEvent *event,
    void *context
) {
  (void)context;
  RuntimeSchedulerRecoveredState *state =
      runtime_scheduler_recovered_require_state();
  if (event != state->global_deadline_event) {
    runtime_global_deadline_identity_failure();
  }
  state->global_deadline_armed = false;
  state->global_deadline_complete = true;
}

_Noreturn void RUNTIME_SCHEDULER_RECOVERED_PUBLIC
runtime_global_deadline_identity_failure(void) {
  runtime_scheduler_recovered_trap();
}
