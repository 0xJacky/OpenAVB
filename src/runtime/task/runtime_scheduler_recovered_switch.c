#include "runtime_scheduler_recovered_private.h"


static RuntimeTask *runtime_scheduler_ready_head(
    RuntimeSchedulerRecoveredState *state) {
  return state->ready_queue_root.next;
}

static RuntimeTask *runtime_scheduler_ready_tail(
    RuntimeSchedulerRecoveredState *state) {
  return state->ready_queue_root.previous;
}

void RUNTIME_SCHEDULER_RECOVERED_PUBLIC runtime_scheduler_ready_enqueue(
    RuntimeTask *task,
    RuntimeTask *task_lower_bound,
    RuntimeTask *task_upper_bound
) {
  RuntimeSchedulerRecoveredState *state =
      runtime_scheduler_recovered_require_state();
  if (!runtime_scheduler_recovered_contains(task, task_lower_bound,
                                            task_upper_bound, sizeof(*task)) ||
      task == state->idle_task || task->state != RUNTIME_TASK_STATE_READY ||
      task->ready_link.next != 0 || task->ready_link.previous != 0 ||
      task == runtime_scheduler_ready_head(state) ||
      task == runtime_scheduler_ready_tail(state)) {
    runtime_scheduler_recovered_trap();
  }
  RuntimeTask *tail = runtime_scheduler_ready_tail(state);
  task->ready_link.previous = tail;
  task->ready_link.previous_lower_bound =
      tail == 0 ? 0 : (uintptr_t)tail;
  task->ready_link.previous_upper_bound =
      tail == 0 ? 0 : (uintptr_t)(tail + 1);
  task->ready_link.next = 0;
  task->ready_link.next_lower_bound = 0;
  task->ready_link.next_upper_bound = 0;
  if (tail == 0) {
    state->ready_queue_root.next = task;
  } else {
    tail->ready_link.next = task;
    tail->ready_link.next_lower_bound = (uintptr_t)task;
    tail->ready_link.next_upper_bound = (uintptr_t)(task + 1);
  }
  state->ready_queue_root.previous = task;
  ++state->ready_count;
}

static RuntimeTask *runtime_scheduler_ready_dequeue(
    RuntimeSchedulerRecoveredState *state) {
  RuntimeTask *task = runtime_scheduler_ready_head(state);
  if (task == 0 || state->ready_count == 0) {
    runtime_scheduler_recovered_trap();
  }
  RuntimeTask *next = task->ready_link.next;
  state->ready_queue_root.next = next;
  if (next == 0) {
    if (runtime_scheduler_ready_tail(state) != task) {
      runtime_scheduler_recovered_trap();
    }
    state->ready_queue_root.previous = 0;
  } else {
    if (next->ready_link.previous != task) {
      runtime_scheduler_recovered_trap();
    }
    next->ready_link.previous = 0;
    next->ready_link.previous_lower_bound = 0;
    next->ready_link.previous_upper_bound = 0;
  }
  task->ready_link.previous = 0;
  task->ready_link.previous_lower_bound = 0;
  task->ready_link.previous_upper_bound = 0;
  task->ready_link.next = 0;
  task->ready_link.next_lower_bound = 0;
  task->ready_link.next_upper_bound = 0;
  --state->ready_count;
  return task;
}

void RUNTIME_SCHEDULER_RECOVERED_PUBLIC runtime_task_publish_current(
    RuntimeTask *task,
    RuntimeTask *lower_bound,
    RuntimeTask *upper_bound,
    const RuntimeFirebloomType *task_type
) {
  RuntimeSchedulerRecoveredState *state =
      runtime_scheduler_recovered_require_state();
  if (!runtime_scheduler_recovered_contains(task, lower_bound, upper_bound,
                                            sizeof(*task)) ||
      task_type == 0 || task->domain == 0 ||
      (task->state != RUNTIME_TASK_STATE_READY &&
       task->state != RUNTIME_TASK_STATE_PARKED)) {
    runtime_scheduler_recovered_trap();
  }
  state->current_task = task;
  state->current_lower_bound = (uintptr_t)lower_bound;
  state->current_upper_bound = (uintptr_t)upper_bound;
  state->current_type = task_type;
  task->state = RUNTIME_TASK_STATE_RUNNING;
  if (state->hooks.publish_task != 0) {
    state->hooks.publish_task(state->hooks.context, task);
  }
}

void RUNTIME_SCHEDULER_RECOVERED_PUBLIC runtime_scheduler_reschedule(void) {
  RuntimeSchedulerRecoveredState *state =
      runtime_scheduler_recovered_require_state();
  RuntimeTask *previous = state->current_task;
  if (previous == 0 || previous->domain == 0 || state->idle_task == 0) {
    runtime_scheduler_recovered_trap();
  }

  if (previous->state == RUNTIME_TASK_STATE_RUNNING) {
    if (state->ready_count == 0) {
      return;
    }
    previous->state = RUNTIME_TASK_STATE_READY;
    if (previous != state->idle_task) {
      runtime_scheduler_ready_enqueue(previous, previous, previous + 1);
    }
  }

  RuntimeTask *next = state->ready_count == 0
                          ? state->idle_task
                          : runtime_scheduler_ready_dequeue(state);
  if (next == previous || next->state != RUNTIME_TASK_STATE_READY) {
    runtime_scheduler_recovered_trap();
  }
  runtime_task_publish_current(
      next,
      next,
      next + 1,
      state->current_type
  );
  ++state->switch_count;
  if (state->hooks.switch_task != 0) {
    state->hooks.switch_task(state->hooks.context, previous, next);
  }
}
