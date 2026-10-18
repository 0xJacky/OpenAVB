#include "avp/runtime/task/runtime_task_context_recovered.h"

#include <assert.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static RuntimeTask g_current_task;
static RuntimeTask g_selected_task;
static RuntimeTask g_idle_task;
static RuntimeTask g_registry_tasks[3];
static RuntimeFirebloomSpan g_registry[3];

static uint64_t generate_guard_seed(void) {
  return UINT64_C(0x1122334455667788);
}

static RuntimeFirebloomSpan task_span(RuntimeTask *task) {
  return (RuntimeFirebloomSpan){
      .current = task,
      .lower_bound = task,
      .upper_bound = task + 1,
      .type = &runtime_task_context_task_type,
  };
}

static void configure(void *current, RuntimeTask *selected) {
  const RuntimeTaskContextConfiguration configuration = {
      .host_current_task = current,
      .host_selected_task = selected,
      .idle_task = &g_idle_task,
      .task_registry = g_registry,
      .task_registry_count = 3,
      .guard_seed_generate = generate_guard_seed,
  };

  runtime_task_context_configure(&configuration);
}

static void expect_trap(void (*operation)(void)) {
  const pid_t child = fork();
  int status;

  assert(child >= 0);
  if (child == 0) {
    operation();
    _exit(0);
  }
  assert(waitpid(child, &status, 0) == child);
  assert(WIFSIGNALED(status));
  assert(WTERMSIG(status) == SIGILL || WTERMSIG(status) == SIGTRAP);
}

static void unset_current(void) {
  configure(NULL, &g_selected_task);
  (void)runtime_current_task_bounded_view();
}

static void invalid_task_tuple(void) {
  RuntimeFirebloomSpan span = task_span(&g_selected_task);

  span.upper_bound = span.current;
  (void)runtime_task_tuple_validate(span);
}

static void blocking_idle(void) {
  runtime_task_require_blocking_allowed(&g_idle_task, (uintptr_t)&g_idle_task,
                                        (uintptr_t)(&g_idle_task + 1), true);
}

static void blocking_masked(void) {
  g_selected_task.frame.spsr_el1 = RUNTIME_TASK_CONTEXT_INTERRUPT_MASK;
  runtime_task_require_blocking_allowed(
      &g_selected_task, (uintptr_t)&g_selected_task,
      (uintptr_t)(&g_selected_task + 1), true);
}

static void frame_wrong_state(void) {
  g_selected_task.state = RUNTIME_TASK_STATE_RUNNING;
  runtime_task_exception_frame_initialize(&g_selected_task,
                                          (uintptr_t)&g_selected_task,
                                          (uintptr_t)(&g_selected_task + 1), 1);
}

static void test_current_views(void) {
  RuntimeFirebloomSpan current;
  RuntimeFirebloomSpan selected;

  configure(&g_current_task, &g_selected_task);
  current = runtime_current_task_bounded_view();
  assert(current.current == &g_current_task);
  assert(current.lower_bound == &g_current_task);
  assert((uintptr_t)current.upper_bound == (uintptr_t)&g_current_task + 0x70);
  assert(current.type == &runtime_current_task_view_type);
  assert(runtime_current_task_or_null() == &g_current_task);
  assert(!runtime_current_task_is_bootstrap_sentinel(&g_current_task));

  configure((void *)RUNTIME_TASK_CONTEXT_BOOTSTRAP_SENTINEL, &g_selected_task);
  assert(runtime_current_task_is_bootstrap_sentinel(
      (void *)RUNTIME_TASK_CONTEXT_BOOTSTRAP_SENTINEL));
  assert(runtime_current_task_or_null() == NULL);

  configure(&g_current_task, &g_selected_task);
  selected = runtime_selected_task_bounded_view();
  assert(selected.current == &g_selected_task);
  assert(selected.lower_bound == &g_selected_task);
  assert(selected.upper_bound == &g_selected_task + 1);
  assert(selected.type == &runtime_task_context_task_type);
  expect_trap(unset_current);
}

static void test_tuple_and_lookup(void) {
  RuntimeFirebloomSpan result;

  for (size_t index = 0; index < 3; ++index) {
    memset(&g_registry_tasks[index], 0, sizeof(g_registry_tasks[index]));
    g_registry_tasks[index].global_ordinal = (uint32_t)(4 + index * 3);
    g_registry[index] = task_span(&g_registry_tasks[index]);
    assert(runtime_task_tuple_validate(g_registry[index]).current ==
           &g_registry_tasks[index]);
  }
  configure(&g_current_task, &g_selected_task);
  result = runtime_task_lookup_by_domain_id(7);
  assert(result.current == &g_registry_tasks[1]);
  result = runtime_task_lookup_by_domain_id(0xff);
  assert(result.current == NULL);
  assert(result.lower_bound == NULL);
  assert(result.upper_bound == NULL);
  assert(result.type == NULL);
  expect_trap(invalid_task_tuple);
}

static void test_frame_initialize(void) {
  RuntimeDomain domain;
  RuntimeTask *task = &g_selected_task;
  const uint64_t explicit_seed = UINT64_C(0xaabbccddeeff1234);

  memset(&domain, 0, sizeof(domain));
  memset(task, 0, sizeof(*task));
  task->state = RUNTIME_TASK_STATE_EXITED;
  task->stack_top = UINT64_C(0x70040000);
  task->stack_guard_base = UINT64_C(0x7003c000);
  task->startup_sp = UINT64_C(0x10012340);
  task->frame_context = UINT64_C(0x7002a000);
  task->domain = &domain;
  task->domain_lower_bound = (uintptr_t)&domain;
  task->domain_upper_bound = (uintptr_t)(&domain + 1);
  domain.address_space_root = UINT64_C(0x70080000);
  domain.address_space_cookie = UINT64_C(0x70084000);

  runtime_task_exception_frame_initialize(task, (uintptr_t)task,
                                          (uintptr_t)(task + 1), explicit_seed);
  assert(task->frame.x[30] == UINT64_C(0xdeadbeefcafef00d));
  assert(task->frame.signed_elr_el1 == task->startup_sp);
  assert(task->frame.sp_el0 == task->stack_guard_base);
  assert(task->frame.spsr_el1 == 0);
  assert(task->frame.tpidrro_el0 == task->frame_context);
  assert(task->frame.ttbr1_el1 == domain.address_space_root);
  assert(task->frame.ttbr0_el1 == domain.address_space_cookie);
  assert(task->frame.full_vector_state == 1);
  assert(task->frame.stack_guard == UINT64_C(0xaabbccddeeff0034));
  assert(task->state == RUNTIME_TASK_STATE_INITIALIZED);

  task->state = RUNTIME_TASK_STATE_EXITED;
  runtime_task_exception_frame_initialize(task, (uintptr_t)task,
                                          (uintptr_t)(task + 1), 0);
  assert(task->frame.stack_guard == UINT64_C(0x1122334455660088));
  expect_trap(frame_wrong_state);
}

static void test_blocking_policy(void) {
  memset(&g_selected_task, 0, sizeof(g_selected_task));
  configure(&g_current_task, &g_selected_task);

  runtime_task_require_blocking_allowed(
      &g_selected_task, (uintptr_t)&g_selected_task,
      (uintptr_t)(&g_selected_task + 1), true);
  g_selected_task.frame.spsr_el1 = RUNTIME_TASK_CONTEXT_INTERRUPT_MASK;
  runtime_task_require_blocking_allowed(&g_idle_task, 0, 0, false);
  expect_trap(blocking_idle);
  expect_trap(blocking_masked);
}

static void test_shared_page_span(void) {
  const uintptr_t shared_page_address = UINT64_C(0x7006c000);
  const uint32_t shared_page = (uint32_t)(shared_page_address >> 14);

  memset(&g_selected_task, 0, sizeof(g_selected_task));
  g_selected_task.frame.signed_elr_el1 = UINT64_C(0x1000);
  g_selected_task.lifecycle_aux = (void *)shared_page_address;
  configure(&g_current_task, &g_selected_task);
  assert(runtime_task_shared_page_span_matches(shared_page, shared_page + 1));
  assert(!runtime_task_shared_page_span_matches(shared_page, shared_page + 2));
  assert(
      !runtime_task_shared_page_span_matches(shared_page + 1, shared_page + 2));
  g_selected_task.lifecycle_aux = NULL;
  assert(!runtime_task_shared_page_span_matches(0, 1));
}

int main(void) {
  test_current_views();
  test_tuple_and_lookup();
  test_frame_initialize();
  test_blocking_policy();
  test_shared_page_span();
  return 0;
}
