#include "avp/runtime/task/runtime_scheduler_recovered.h"

#include <assert.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>


typedef struct {
  uint64_t now;
  uint64_t programmed_deadline;
  uint64_t interrupt_deadline;
  uint32_t mask_calls;
  uint32_t publish_calls;
  uint32_t switch_calls;
  uint32_t interrupt_schedule_calls;
  RuntimeTimerEvent *callback_order[8];
  uint32_t callback_count;
} SchedulerOracle;

static const RuntimeFirebloomType task_type = {
    .identity = UINT64_C(0x7461736b),
    .element_size = sizeof(RuntimeTask),
    .bytewise_copyable = false,
};

static uint64_t oracle_counter(void *context) {
  return ((SchedulerOracle *)context)->now;
}

static void oracle_program(void *context, uint64_t deadline) {
  ((SchedulerOracle *)context)->programmed_deadline = deadline;
}

static void oracle_mask(void *context) {
  ++((SchedulerOracle *)context)->mask_calls;
}

static void oracle_publish(void *context, RuntimeTask *task) {
  (void)task;
  ++((SchedulerOracle *)context)->publish_calls;
}

static void oracle_switch(void *context, RuntimeTask *previous,
                          RuntimeTask *next) {
  assert(previous != next);
  ++((SchedulerOracle *)context)->switch_calls;
}

static void oracle_reschedule_interrupt(void *context, uint64_t deadline,
                                        void (*callback)(void),
                                        const void *signature_descriptor) {
  SchedulerOracle *oracle = context;
  assert(callback != NULL);
  assert(signature_descriptor ==
         (const void *)(uintptr_t)UINT64_C(0x145450));
  oracle->interrupt_deadline = deadline;
  ++oracle->interrupt_schedule_calls;
}

static void oracle_timer_callback(RuntimeTimerEvent *event, void *context) {
  RuntimeTask *owner = context;
  SchedulerOracle *oracle = event->callback_metadata;
  assert(owner == event->owner);
  assert(oracle->callback_count < 8);
  oracle->callback_order[oracle->callback_count++] = event;
}

static RuntimeSchedulerRecoveredHooks oracle_hooks(SchedulerOracle *oracle) {
  return (RuntimeSchedulerRecoveredHooks){
      .context = oracle,
      .counter = oracle_counter,
      .program_deadline = oracle_program,
      .mask_timer = oracle_mask,
      .publish_task = oracle_publish,
      .switch_task = oracle_switch,
      .reschedule_interrupt_drain = oracle_reschedule_interrupt,
  };
}

static void initialize_state(RuntimeSchedulerRecoveredState *state,
                             SchedulerOracle *oracle, RuntimeTask *idle,
                             RuntimeTimerEvent *global_deadline) {
  runtime_scheduler_recovered_initialize(state, idle, global_deadline,
                                         oracle_hooks(oracle));
  state->interrupt_signature_descriptor =
      (const void *)(uintptr_t)UINT64_C(0x145450);
  runtime_scheduler_recovered_install(state);
}

static void test_timer_deadline_order_and_cancellation(void) {
  SchedulerOracle oracle = {0};
  RuntimeSchedulerRecoveredState state;
  RuntimeTask idle = {0};
  RuntimeTask owner = {0};
  RuntimeTimerEvent global_deadline = {0};
  RuntimeTimerEvent late = {0};
  RuntimeTimerEvent first_equal = {0};
  RuntimeTimerEvent second_equal = {0};
  initialize_state(&state, &oracle, &idle, &global_deadline);

  runtime_timer_event_arm(&late, &late, &late + 1, oracle_timer_callback,
                          &oracle, &owner, &owner, &owner + 1, &task_type, 20);
  assert(state.timer_count == 1 && state.programmed_deadline == 20);
  runtime_timer_event_arm(&first_equal, &first_equal, &first_equal + 1,
                          oracle_timer_callback, &oracle, &owner, &owner,
                          &owner + 1, &task_type, 10);
  runtime_timer_event_arm(&second_equal, &second_equal, &second_equal + 1,
                          oracle_timer_callback, &oracle, &owner, &owner,
                          &owner + 1, &task_type, 10);
  assert(state.timer_count == 3 && state.programmed_deadline == 10);

  oracle.now = 10;
  runtime_timer_drain_expired_events();
  assert(oracle.callback_count == 2);
  assert(oracle.callback_order[0] == &first_equal);
  assert(oracle.callback_order[1] == &second_equal);
  assert(state.timer_count == 1 && state.programmed_deadline == 20);

  runtime_timer_event_unlink_if_armed(&late, &late, &late + 1);
  assert(state.timer_count == 0 && state.timer_masked);
  assert(oracle.mask_calls == 1);
  runtime_timer_event_unlink_if_armed(&late, &late, &late + 1);
  assert(state.timer_count == 0);
}

static void test_interrupt_timer_drain_and_future_reschedule(void) {
  SchedulerOracle oracle = {.now = 8};
  RuntimeSchedulerRecoveredState state;
  RuntimeTask idle = {0};
  RuntimeTask owner = {0};
  RuntimeTimerEvent global_deadline = {0};
  RuntimeTimerEvent expired = {0};
  RuntimeTimerEvent future = {0};
  initialize_state(&state, &oracle, &idle, &global_deadline);

  runtime_scheduler_recovered_interrupt_timer_arm(
      &expired, 7, oracle_timer_callback, &oracle, &owner);
  runtime_scheduler_recovered_interrupt_timer_arm(
      &future, 12, oracle_timer_callback, &oracle, &owner);
  RuntimeSchedulerRecoveredSpan front = runtime_interrupt_timer_queue_front(
      &state.interrupt_timer_queue_root, &state.interrupt_timer_queue_root,
      &state.interrupt_timer_queue_root + 1);
  assert(front.current == &expired);
  assert(front.type == &runtime_scheduler_recovered_timer_event_type);

  runtime_interrupt_timer_drain_expired_events();
  assert(oracle.callback_count == 1 && oracle.callback_order[0] == &expired);
  assert(state.interrupt_timer_count == 1);
  assert(state.interrupt_programmed_deadline == 12);
  assert(oracle.interrupt_schedule_calls == 1);
  assert(oracle.interrupt_deadline == 12);

  oracle.now = 12;
  runtime_interrupt_timer_drain_expired_events();
  assert(oracle.callback_count == 2 && oracle.callback_order[1] == &future);
  assert(state.interrupt_timer_count == 0);
  front = runtime_interrupt_timer_queue_front(
      &state.interrupt_timer_queue_root, &state.interrupt_timer_queue_root,
      &state.interrupt_timer_queue_root + 1);
  assert(front.current == NULL && front.lower_bound == NULL);
}

static void test_ready_fifo_reschedule_and_publish(void) {
  SchedulerOracle oracle = {0};
  RuntimeSchedulerRecoveredState state;
  RuntimeDomain domain = {0};
  RuntimeTask idle = {.domain = &domain, .state = RUNTIME_TASK_STATE_READY};
  RuntimeTask current = {
      .domain = &domain,
      .state = RUNTIME_TASK_STATE_READY,
  };
  RuntimeTask first = {.domain = &domain, .state = RUNTIME_TASK_STATE_READY};
  RuntimeTask second = {.domain = &domain, .state = RUNTIME_TASK_STATE_READY};
  RuntimeTimerEvent global_deadline = {0};
  initialize_state(&state, &oracle, &idle, &global_deadline);

  runtime_task_publish_current(&current, &current, &current + 1, &task_type);
  runtime_scheduler_ready_enqueue(&first, &first, &first + 1);
  runtime_scheduler_ready_enqueue(&second, &second, &second + 1);
  assert(state.ready_count == 2);
  runtime_scheduler_reschedule();
  assert(state.current_task == &first);
  assert(first.state == RUNTIME_TASK_STATE_RUNNING);
  assert(state.ready_count == 2);
  assert(state.ready_queue_root.next == &second);
  assert(state.ready_queue_root.previous == &current);

  runtime_scheduler_reschedule();
  assert(state.current_task == &second);
  assert(state.ready_queue_root.next == &current);
  assert(state.ready_queue_root.previous == &first);
  assert(state.switch_count == 2 && oracle.switch_calls == 2);
  assert(oracle.publish_calls == 3);
}

static void test_blocked_task_selects_idle(void) {
  SchedulerOracle oracle = {0};
  RuntimeSchedulerRecoveredState state;
  RuntimeDomain domain = {0};
  RuntimeTask idle = {.domain = &domain, .state = RUNTIME_TASK_STATE_READY};
  RuntimeTask blocked = {
      .domain = &domain,
      .state = RUNTIME_TASK_STATE_TIMER_WAIT,
  };
  RuntimeTimerEvent global_deadline = {0};
  initialize_state(&state, &oracle, &idle, &global_deadline);
  state.current_task = &blocked;
  state.current_type = &task_type;

  runtime_scheduler_reschedule();
  assert(state.current_task == &idle);
  assert(idle.state == RUNTIME_TASK_STATE_RUNNING);
  assert(state.switch_count == 1);
}

static void test_global_deadline_and_tuple_helpers(void) {
  SchedulerOracle oracle = {0};
  RuntimeSchedulerRecoveredState state;
  RuntimeTask idle = {0};
  RuntimeTimerEvent global_deadline = {0};
  initialize_state(&state, &oracle, &idle, &global_deadline);
  state.global_deadline_armed = true;
  runtime_global_deadline_expired(&global_deadline, NULL);
  assert(!state.global_deadline_armed && state.global_deadline_complete);

  RuntimeSchedulerRecoveredSpan span = runtime_timer_event_tuple_validate(
      &global_deadline, &global_deadline, &global_deadline + 1,
      &runtime_scheduler_recovered_timer_event_type);
  assert(span.current == &global_deadline);
  assert(span.type == &runtime_scheduler_recovered_timer_event_type);
  span = runtime_interrupt_timer_event_tuple_validate(
      &global_deadline, &global_deadline, &global_deadline + 1,
      &runtime_scheduler_recovered_timer_event_type);
  assert(span.upper_bound == &global_deadline + 1);

  uint8_t runtime_state[0xce0] = {0};
  span = runtime_scheduler_outlined_prepare_bootstrap_task_u64_store(
      &task_type, runtime_state);
  assert(span.current == runtime_state + 0x730);
  assert(span.upper_bound == runtime_state + 0xce0);
  assert(runtime_interrupt_timer_source_token() ==
         UINT64_C(0x0de56c2f7f77c6bc));
  RuntimeSchedulerRecoveredRescheduleCallback callback =
      runtime_interrupt_timer_outlined_prepare_reschedule_callback();
  assert(callback.callback != NULL);
  assert(callback.signature_descriptor ==
         (const void *)(uintptr_t)UINT64_C(0x145450));
}

static void expect_trap(void (*operation)(void)) {
  const pid_t child = fork();
  assert(child >= 0);
  if (child == 0) {
    operation();
    _exit(0);
  }
  int status = 0;
  assert(waitpid(child, &status, 0) == child);
  assert(WIFSIGNALED(status));
  assert(WTERMSIG(status) == SIGILL || WTERMSIG(status) == SIGTRAP);
}

static RuntimeSchedulerRecoveredState fatal_state;
static SchedulerOracle fatal_oracle;
static RuntimeTask fatal_idle;
static RuntimeTimerEvent fatal_global_deadline;

static void prepare_fatal_state(void) {
  fatal_idle = (RuntimeTask){0};
  fatal_global_deadline = (RuntimeTimerEvent){0};
  initialize_state(&fatal_state, &fatal_oracle, &fatal_idle,
                   &fatal_global_deadline);
}

static void fatal_bad_timer_bounds(void) {
  (void)runtime_timer_event_tuple_validate(
      &fatal_global_deadline, &fatal_global_deadline + 1,
      &fatal_global_deadline + 2,
      &runtime_scheduler_recovered_timer_event_type);
}

static void fatal_wrong_global_deadline(void) {
  RuntimeTimerEvent other = {0};
  runtime_global_deadline_expired(&other, NULL);
}

static void fatal_wrong_timer_type(void) {
  (void)runtime_timer_event_tuple_validate(
      &fatal_global_deadline, &fatal_global_deadline,
      &fatal_global_deadline + 1, &task_type);
}

static void fatal_idle_ready_enqueue(void) {
  fatal_idle.state = RUNTIME_TASK_STATE_READY;
  runtime_scheduler_ready_enqueue(&fatal_idle, &fatal_idle, &fatal_idle + 1);
}

static void fatal_corrupt_interrupt_links(void) {
  RuntimeTask owner = {0};
  RuntimeTimerEvent event = {0};
  runtime_scheduler_recovered_interrupt_timer_arm(
      &event, 1, oracle_timer_callback, &fatal_oracle, &owner);
  event.queue_link.previous = &event;
  event.queue_link.previous_lower_bound = (uintptr_t)&event;
  event.queue_link.previous_upper_bound = (uintptr_t)(&event + 1);
  runtime_interrupt_timer_event_unlink(&event, &event, &event + 1);
}

static void test_fatal_contracts(void) {
  prepare_fatal_state();
  expect_trap(fatal_bad_timer_bounds);
  prepare_fatal_state();
  expect_trap(fatal_wrong_global_deadline);
  prepare_fatal_state();
  expect_trap(fatal_wrong_timer_type);
  prepare_fatal_state();
  expect_trap(fatal_idle_ready_enqueue);
  prepare_fatal_state();
  expect_trap(fatal_corrupt_interrupt_links);
}

int main(void) {
  test_timer_deadline_order_and_cancellation();
  test_interrupt_timer_drain_and_future_reschedule();
  test_ready_fifo_reschedule_and_publish();
  test_blocked_task_selects_idle();
  test_global_deadline_and_tuple_helpers();
  test_fatal_contracts();
  puts("runtime_scheduler_recovered_tests: ok");
  return 0;
}
