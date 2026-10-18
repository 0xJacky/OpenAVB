#include "avp/runtime/task/runtime_irq_recovered.h"

#include <assert.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct {
  uint32_t events[4];
  size_t event_count;
  size_t event_index;
  uint64_t counter;
  unsigned push_count;
  unsigned pop_count;
  unsigned external_count;
  unsigned timer_count;
  unsigned wait_count;
} IrqProbe;

static RuntimeInterruptTaskState task;
static uint8_t state_flag;
static uint64_t worker_runtime;

static bool next_event(uint32_t *event, void *context) {
  IrqProbe *probe = context;

  if (probe->event_index == probe->event_count) {
    return false;
  }
  *event = probe->events[probe->event_index++];
  return true;
}

static uint64_t counter_read(void *context) {
  IrqProbe *probe = context;

  probe->counter += 7;
  return probe->counter;
}

static void push_mask(void *context) { ++((IrqProbe *)context)->push_count; }
static void pop_mask(void *context) { ++((IrqProbe *)context)->pop_count; }
static void external_event(void *context) {
  ++((IrqProbe *)context)->external_count;
}
static void timer_event(void *context) { ++((IrqProbe *)context)->timer_count; }
static void wait_once(void *context) { ++((IrqProbe *)context)->wait_count; }

static RuntimeInterruptConfiguration configuration(IrqProbe *probe) {
  return (RuntimeInterruptConfiguration){
      .current_task = &task,
      .state_flags = &state_flag,
      .next_event = next_event,
      .push_mask = push_mask,
      .pop_mask = pop_mask,
      .external_event = external_event,
      .timer_event = timer_event,
      .wait_for_interrupt = wait_once,
      .counter_read = counter_read,
      .worker_runtime = &worker_runtime,
      .context = probe,
  };
}

static void test_timer_and_svc_hooks(IrqProbe *probe) {
  runtime_interrupt_host_timer_state(5, 9);
  assert(runtime_virtual_timer_control_read() == 5);
  runtime_virtual_timer_control_write(2);
  runtime_virtual_timer_tval_write(0x1234);
  assert(runtime_virtual_timer_control_read() == 2);
  runtime_interrupt_mask_push_svc();
  runtime_interrupt_mask_pop_svc();
  runtime_wait_for_interrupt_once();
  j_runtime_wait_for_interrupt_once();
  assert(probe->push_count == 1);
  assert(probe->pop_count == 1);
  assert(probe->wait_count == 2);
}

static void test_depth_and_views(void) {
  RuntimeInterruptStateFlagsView view =
      runtime_interrupt_state_flags_bounded_view();

  assert(view.current == &state_flag);
  assert(view.lower_bound == &state_flag);
  assert(view.upper_bound == &state_flag + 1);
  assert(runtime_interrupt_state_flags_compare_sp_current_to_upper(view));
  assert(runtime_interrupt_state_flags_compare_sp_current_to_upper(
      runtime_interrupt_state_flags_prepare_sp_result_tuple()));
  assert(runtime_interrupt_state_flags_compare_sp_current_to_upper(
      runtime_interrupt_worker_state_flags_to_stack()));
  assert(runtime_current_task_interrupt_mask_depth_is_zero());
  runtime_current_task_interrupt_mask_depth_increment();
  assert(task.interrupt_mask_depth == 1);
  assert(!runtime_current_task_interrupt_mask_depth_is_zero());
  runtime_current_task_interrupt_mask_depth_decrement();
  assert(runtime_current_task_interrupt_mask_depth_is_zero());
  assert(runtime_interrupt_source_token() == UINT64_C(0x0955b303f8cbfe51));
}

static void test_worker(IrqProbe *probe) {
  probe->events[0] = RUNTIME_INTERRUPT_EVENT_EXTERNAL;
  probe->events[1] = RUNTIME_INTERRUPT_EVENT_TIMER;
  probe->event_count = 2;
  state_flag = 0x80;
  runtime_interrupt_worker();
  assert(probe->external_count == 1);
  assert(probe->timer_count == 1);
  assert(task.interrupt_mask_depth == 0);
  assert(task.saved_state_flag == 0x80);
  assert(state_flag == 0x80);
  assert(worker_runtime == 14);
}

int main(void) {
  IrqProbe probe;
  RuntimeInterruptConfiguration config;

  memset(&probe, 0, sizeof(probe));
  memset(&task, 0, sizeof(task));
  config = configuration(&probe);
  runtime_interrupt_configure(&config);
  test_timer_and_svc_hooks(&probe);
  test_depth_and_views();
  test_worker(&probe);
  runtime_virtual_timer_irq_post_initialize_noop();
  runtime_interrupt_worker_case1_hook_noop();
  return 0;
}
