#include "avp/runtime/task/runtime_task_lifecycle_recovered.h"

#include <assert.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct {
  size_t ready_count;
  RuntimeTask *ready_task;
  bool projected_prepared;
} LifecycleProbe;

static LifecycleProbe probe;
static RuntimeTask tasks[2];
static RuntimeTaskStartRecord start_record;

static void ready_enqueue(RuntimeTask *task, void *context) {
  LifecycleProbe *state = context;
  state->ready_task = task;
  ++state->ready_count;
}

static void prepare_projected_start(void *context) {
  ((LifecycleProbe *)context)->projected_prepared = true;
}

static int32_t start_entry(uintptr_t argument0, uintptr_t argument1,
                           uintptr_t argument2, uintptr_t argument3) {
  assert(argument0 == 3);
  assert(argument1 == 5);
  assert(argument2 == 7);
  assert(argument3 == 11);
  return probe.projected_prepared ? 37 : 38;
}

static void exit_task(int32_t result, void *context) {
  (void)context;
  _exit(result);
}

static RuntimeTaskLifecycleConfiguration configuration(void) {
  return (RuntimeTaskLifecycleConfiguration){
      .restricted_task = &tasks[0],
      .start_record =
          {
              .current = &start_record,
              .lower_bound = &start_record,
              .upper_bound = &start_record + 1,
          },
      .allocator_route = {0},
      .prepare_projected_start = prepare_projected_start,
      .ready_enqueue = ready_enqueue,
      .exit_task = exit_task,
      .context = &probe,
  };
}

static void expect_trap(void (*operation)(void)) {
  pid_t child = fork();
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

static void expect_exit_code(int expected, void (*operation)(void)) {
  pid_t child = fork();
  assert(child >= 0);
  if (child == 0) {
    operation();
    _exit(0);
  }
  int status = 0;
  assert(waitpid(child, &status, 0) == child);
  assert(WIFEXITED(status));
  assert(WEXITSTATUS(status) == expected);
}

static RuntimeTaskLifecycleView task_view(RuntimeTask *task) {
  return (RuntimeTaskLifecycleView){
      .current = task,
      .lower_bound = &tasks[0],
      .upper_bound = &tasks[2],
  };
}

static void current_task_fatal(void) { runtime_current_task_unset_fatal(); }
static void ready_bounds_fatal(void) { runtime_task_make_ready_bounds_panic(); }
static void invalid_capability_tuple(void) {
  (void)runtime_task_capability_tuple_validate((RuntimeTaskCapabilityTuple){
      .current = &tasks[0],
      .lower_bound = &tasks[0],
      .upper_bound = &tasks[1],
      .type_token = 0,
  });
}
static void invalid_restricted_state(void) {
  runtime_task_set_state(task_view(&tasks[0]), RUNTIME_TASK_STATE_IPC_WAIT);
}
static void invalid_task_bounds(void) {
  runtime_task_make_ready((RuntimeTaskLifecycleView){
      .current = &tasks[0],
      .lower_bound = &tasks[0],
      .upper_bound = &tasks[0],
  });
}
static void invalid_allocator_route(void) {
  uint8_t route = 0;
  (void)runtime_task_start_validate_domain_allocator_route_thunk_a(
      (RuntimeTaskAllocatorRouteView){
          .current = &route,
          .lower_bound = &route,
          .upper_bound = &route + 1,
          .type_token = 0,
      });
}
static void start_projected(void) { runtime_task_start_trampoline(1); }
static void start_regular(void) { runtime_task_start_trampoline(0); }

static void test_capability_and_routes(void) {
  RuntimeTaskCapabilityTuple tuple = {
      .current = &tasks[1],
      .lower_bound = &tasks[0],
      .upper_bound = &tasks[2],
      .type_token = RUNTIME_TASK_CAPABILITY_TYPE_TOKEN,
  };
  assert(runtime_task_capability_tuple_validate(tuple) == &tasks[1]);
  assert(runtime_task_capability_tuple_validate_thunk_a(tuple) == &tasks[1]);
  assert(runtime_task_capability_tuple_validate_thunk_b(tuple) == &tasks[1]);
  assert(runtime_task_source_token() == UINT64_C(0x04940f9f76a3df19));

  RuntimeTaskAllocatorRouteView empty = {0};
  assert(runtime_task_start_validate_domain_allocator_route_thunk_a(empty) ==
         NULL);
  assert(runtime_task_start_validate_domain_allocator_route_thunk_b(empty) ==
         NULL);
  uint8_t route = 0;
  RuntimeTaskAllocatorRouteView present = {
      .current = &route,
      .lower_bound = &route,
      .upper_bound = &route + 1,
      .type_token = RUNTIME_TASK_ALLOCATOR_ROUTE_TYPE_TOKEN,
  };
  assert(runtime_task_start_validate_domain_allocator_route_thunk_a(present) ==
         &route);
  assert(runtime_task_start_validate_domain_allocator_route_thunk_b(present) ==
         &route);
}

static void test_state_and_ready(void) {
  runtime_task_set_state(task_view(&tasks[1]), RUNTIME_TASK_STATE_EXITED);
  assert(tasks[1].state == RUNTIME_TASK_STATE_EXITED);
  runtime_task_set_state(task_view(&tasks[0]), RUNTIME_TASK_STATE_PARKED);
  assert(tasks[0].state == RUNTIME_TASK_STATE_PARKED);

  runtime_task_make_ready(task_view(&tasks[1]));
  assert(tasks[1].state == RUNTIME_TASK_STATE_READY);
  assert(probe.ready_count == 1);
  assert(probe.ready_task == &tasks[1]);
}

static void test_start_trampoline(void) {
  start_record.entry = start_entry;
  start_record.arguments[0] = 3;
  start_record.arguments[1] = 5;
  start_record.arguments[2] = 7;
  start_record.arguments[3] = 11;

  probe.projected_prepared = false;
  expect_exit_code(38, start_regular);
  probe.projected_prepared = false;
  expect_exit_code(37, start_projected);
}

int main(void) {
  memset(&probe, 0, sizeof(probe));
  memset(tasks, 0, sizeof(tasks));
  memset(&start_record, 0, sizeof(start_record));
  RuntimeTaskLifecycleConfiguration config = configuration();
  runtime_task_lifecycle_configure(&config);

  test_capability_and_routes();
  test_state_and_ready();
  test_start_trampoline();
  expect_trap(current_task_fatal);
  expect_trap(ready_bounds_fatal);
  expect_trap(invalid_capability_tuple);
  expect_trap(invalid_restricted_state);
  expect_trap(invalid_task_bounds);
  expect_trap(invalid_allocator_route);
  return 0;
}
