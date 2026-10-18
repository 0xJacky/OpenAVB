#include "runtime_scheduler_recovered_private.h"


#if defined(__ELF__)
static RuntimeSchedulerRecoveredState *runtime_scheduler_active_state
    __attribute__((section(".runtime_task.state")));
#else
static RuntimeSchedulerRecoveredState *runtime_scheduler_active_state;
#endif

const RuntimeFirebloomType runtime_scheduler_recovered_timer_event_type = {
    .identity = UINT64_C(0x74696d65722d6576),
    .element_size = sizeof(RuntimeTimerEvent),
    .bytewise_copyable = false,
};

_Noreturn void runtime_scheduler_recovered_trap(void) {
  __builtin_trap();
}

RuntimeSchedulerRecoveredState *runtime_scheduler_recovered_require_state(void) {
  if (runtime_scheduler_active_state == 0) {
    runtime_scheduler_recovered_trap();
  }
  return runtime_scheduler_active_state;
}

bool runtime_scheduler_recovered_contains(
    const void *current,
    const void *lower_bound,
    const void *upper_bound,
    size_t size
) {
  const uintptr_t current_address = (uintptr_t)current;
  const uintptr_t lower_address = (uintptr_t)lower_bound;
  const uintptr_t upper_address = (uintptr_t)upper_bound;
  return current != 0 && lower_address <= current_address &&
         current_address < upper_address && size <= upper_address - current_address;
}

static void runtime_scheduler_clear_words(void *storage, size_t size) {
  uint64_t *words = storage;
  for (size_t index = 0; index < size / sizeof(*words); ++index) {
    words[index] = 0;
  }
}

static void runtime_scheduler_initialize_timer_root(RuntimeTaskQueueLink *root) {
  root->previous = root;
  root->previous_lower_bound = (uintptr_t)root;
  root->previous_upper_bound = (uintptr_t)(root + 1);
  root->previous_cookie = 0;
  root->next = root;
  root->next_lower_bound = (uintptr_t)root;
  root->next_upper_bound = (uintptr_t)(root + 1);
  root->next_cookie = 0;
}

void runtime_scheduler_recovered_initialize(
    RuntimeSchedulerRecoveredState *state,
    RuntimeTask *idle_task,
    RuntimeTimerEvent *global_deadline_event,
    RuntimeSchedulerRecoveredHooks hooks
) {
  if (state == 0 || idle_task == 0) {
    runtime_scheduler_recovered_trap();
  }
  runtime_scheduler_clear_words(state, sizeof(*state));
  runtime_scheduler_initialize_timer_root(&state->timer_queue_root);
  runtime_scheduler_initialize_timer_root(&state->interrupt_timer_queue_root);
  state->idle_task = idle_task;
  state->global_deadline_event = global_deadline_event;
  state->hooks = hooks;
  state->timer_masked = true;
}

void runtime_scheduler_recovered_install(RuntimeSchedulerRecoveredState *state) {
  if (state == 0 || state->idle_task == 0) {
    runtime_scheduler_recovered_trap();
  }
  runtime_scheduler_active_state = state;
}

RuntimeSchedulerRecoveredState *runtime_scheduler_recovered_state(void) {
  return runtime_scheduler_active_state;
}

RuntimeTaskQueueLink *runtime_scheduler_recovered_event_link(
    RuntimeTimerEvent *event) {
  return event == 0 ? 0 : &event->queue_link;
}

static bool runtime_scheduler_link_is_root(
    RuntimeTaskQueueLink *root,
    RuntimeTaskQueueLink *link
) {
  return link == root;
}

static void runtime_scheduler_set_previous(
    RuntimeTaskQueueLink *root,
    RuntimeTaskQueueLink *link,
    RuntimeTaskQueueLink *previous
) {
  link->previous = previous;
  link->previous_lower_bound = (uintptr_t)previous;
  link->previous_upper_bound = (uintptr_t)previous +
      (runtime_scheduler_link_is_root(root, previous)
           ? sizeof(*root)
           : sizeof(RuntimeTimerEvent));
  link->previous_cookie = runtime_scheduler_link_is_root(root, previous)
                              ? 0
                              : (uintptr_t)&runtime_scheduler_recovered_timer_event_type;
}

static void runtime_scheduler_set_next(
    RuntimeTaskQueueLink *root,
    RuntimeTaskQueueLink *link,
    RuntimeTaskQueueLink *next
) {
  link->next = next;
  link->next_lower_bound = (uintptr_t)next;
  link->next_upper_bound = (uintptr_t)next +
      (runtime_scheduler_link_is_root(root, next)
           ? sizeof(*root)
           : sizeof(RuntimeTimerEvent));
  link->next_cookie = runtime_scheduler_link_is_root(root, next)
                          ? 0
                          : (uintptr_t)&runtime_scheduler_recovered_timer_event_type;
}

RuntimeTimerEvent *runtime_scheduler_recovered_timer_head(
    RuntimeTaskQueueLink *root) {
  RuntimeTaskQueueLink *head = root->next;
  return head == root ? 0 : (RuntimeTimerEvent *)head;
}

void runtime_scheduler_recovered_link_timer_before(
    RuntimeTaskQueueLink *root,
    RuntimeTimerEvent *position,
    RuntimeTimerEvent *event
) {
  RuntimeTaskQueueLink *event_link = &event->queue_link;
  RuntimeTaskQueueLink *next = position == 0 ? root : &position->queue_link;
  RuntimeTaskQueueLink *previous = next->previous;
  runtime_scheduler_set_previous(root, event_link, previous);
  runtime_scheduler_set_next(root, event_link, next);
  runtime_scheduler_set_next(root, previous, event_link);
  runtime_scheduler_set_previous(root, next, event_link);
}

static void runtime_scheduler_validate_timer_neighbor(
    RuntimeTaskQueueLink *root,
    RuntimeTaskQueueLink *neighbor,
    uintptr_t lower_bound,
    uintptr_t upper_bound,
    size_t size
) {
  if (!runtime_scheduler_recovered_contains(neighbor,
                                            (const void *)lower_bound,
                                            (const void *)upper_bound, size) ||
      (neighbor == root && size != sizeof(*root))) {
    runtime_scheduler_recovered_trap();
  }
}

void runtime_scheduler_recovered_unlink_timer(
    RuntimeTaskQueueLink *root,
    RuntimeTimerEvent *event
) {
  RuntimeTaskQueueLink *link = &event->queue_link;
  RuntimeTaskQueueLink *previous = link->previous;
  RuntimeTaskQueueLink *next = link->next;
  if (previous == 0 || next == 0) {
    runtime_scheduler_recovered_trap();
  }
  runtime_scheduler_validate_timer_neighbor(
      root,
      previous,
      link->previous_lower_bound,
      link->previous_upper_bound,
      previous == root ? sizeof(*root) : sizeof(RuntimeTimerEvent)
  );
  runtime_scheduler_validate_timer_neighbor(
      root,
      next,
      link->next_lower_bound,
      link->next_upper_bound,
      next == root ? sizeof(*root) : sizeof(RuntimeTimerEvent)
  );
  if (previous->next != link || next->previous != link) {
    runtime_scheduler_recovered_trap();
  }
  runtime_scheduler_set_next(root, previous, next);
  runtime_scheduler_set_previous(root, next, previous);
  runtime_scheduler_clear_words(link, sizeof(*link));
}

RuntimeSchedulerRecoveredSpan RUNTIME_SCHEDULER_RECOVERED_PUBLIC
runtime_scheduler_outlined_prepare_bootstrap_task_u64_store(
    const RuntimeFirebloomType *type_context,
    void *runtime_state
) {
  if (runtime_state == 0 || type_context == 0) {
    runtime_scheduler_recovered_trap();
  }
  uint8_t *base = runtime_state;
  return (RuntimeSchedulerRecoveredSpan){
      .current = base + 0x730,
      .lower_bound = base + 0x730,
      .upper_bound = base + 0xce0,
      .type = type_context,
  };
}

static RuntimeSchedulerRecoveredSpan runtime_scheduler_validate_timer_tuple(
    RuntimeTimerEvent *current,
    const RuntimeTimerEvent *lower_bound,
    const RuntimeTimerEvent *upper_bound,
    const RuntimeFirebloomType *type_context
) {
  if (type_context == 0 ||
      type_context->identity !=
          runtime_scheduler_recovered_timer_event_type.identity ||
      type_context->element_size != sizeof(*current) ||
      !runtime_scheduler_recovered_contains(current, lower_bound, upper_bound,
                                            sizeof(*current))) {
    runtime_scheduler_recovered_trap();
  }
  return (RuntimeSchedulerRecoveredSpan){
      .current = current,
      .lower_bound = (void *)(uintptr_t)lower_bound,
      .upper_bound = (void *)(uintptr_t)upper_bound,
      .type = type_context,
  };
}

RuntimeSchedulerRecoveredSpan RUNTIME_SCHEDULER_RECOVERED_PUBLIC
runtime_timer_event_tuple_validate(
    RuntimeTimerEvent *current,
    const RuntimeTimerEvent *lower_bound,
    const RuntimeTimerEvent *upper_bound,
    const RuntimeFirebloomType *type_context
) {
  return runtime_scheduler_validate_timer_tuple(
      current,
      lower_bound,
      upper_bound,
      type_context
  );
}

RuntimeSchedulerRecoveredSpan RUNTIME_SCHEDULER_RECOVERED_PUBLIC
runtime_interrupt_timer_event_tuple_validate(
    RuntimeTimerEvent *current,
    const RuntimeTimerEvent *lower_bound,
    const RuntimeTimerEvent *upper_bound,
    const RuntimeFirebloomType *type_context
) {
  return runtime_scheduler_validate_timer_tuple(
      current,
      lower_bound,
      upper_bound,
      type_context
  );
}

RuntimeSchedulerRecoveredSpan RUNTIME_SCHEDULER_RECOVERED_PUBLIC
runtime_interrupt_timer_queue_front(
    RuntimeTaskQueueLink *root,
    RuntimeTaskQueueLink *lower_bound,
    RuntimeTaskQueueLink *upper_bound
) {
  if (!runtime_scheduler_recovered_contains(root, lower_bound, upper_bound,
                                            sizeof(*root))) {
    runtime_interrupt_timer_queue_front_bounds_panic();
  }
  RuntimeTaskQueueLink *front = root->next;
  if (front == root) {
    return (RuntimeSchedulerRecoveredSpan){0};
  }
  if (!runtime_scheduler_recovered_contains(
          front, (const void *)root->next_lower_bound,
          (const void *)root->next_upper_bound, sizeof(RuntimeTimerEvent))) {
    runtime_scheduler_recovered_trap();
  }
  return (RuntimeSchedulerRecoveredSpan){
      .current = front,
      .lower_bound = (void *)root->next_lower_bound,
      .upper_bound = (void *)root->next_upper_bound,
      .type = (const RuntimeFirebloomType *)root->next_cookie,
  };
}

_Noreturn void RUNTIME_SCHEDULER_RECOVERED_PUBLIC
runtime_interrupt_timer_queue_front_bounds_panic(void) {
  runtime_scheduler_recovered_trap();
}

uint64_t RUNTIME_SCHEDULER_RECOVERED_PUBLIC
runtime_interrupt_timer_source_token(void) {
  return UINT64_C(0x0de56c2f7f77c6bc);
}
