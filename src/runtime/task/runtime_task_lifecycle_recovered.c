#include "avp/runtime/task/runtime_task_lifecycle_recovered.h"

static RuntimeTaskLifecycleConfiguration lifecycle_configuration;
static bool lifecycle_configuration_bound;

static _Noreturn void runtime_task_lifecycle_trap(void) {
  __builtin_trap();
  __builtin_unreachable();
}

static bool runtime_task_lifecycle_address_in_bounds(
    uintptr_t current,
    uintptr_t lower_bound,
    uintptr_t upper_bound,
    size_t size
) {
  return current != 0 && lower_bound != 0 && upper_bound != 0 &&
         current >= lower_bound && current < upper_bound &&
         size <= upper_bound - current;
}

static RuntimeTask *
runtime_task_lifecycle_validate_view(RuntimeTaskLifecycleView task) {
  if (!runtime_task_lifecycle_address_in_bounds(
          (uintptr_t)task.current, (uintptr_t)task.lower_bound,
          (uintptr_t)task.upper_bound, sizeof(RuntimeTask))) {
    runtime_task_lifecycle_trap();
  }
  return task.current;
}

void runtime_task_lifecycle_configure(
    const RuntimeTaskLifecycleConfiguration *configuration) {
  if (configuration == NULL) {
    runtime_task_lifecycle_trap();
  }
  lifecycle_configuration = *configuration;
  lifecycle_configuration_bound = true;
}

_Noreturn void RUNTIME_TASK_LIFECYCLE_PUBLIC
runtime_current_task_unset_fatal(void) {
  runtime_task_lifecycle_trap();
}

RuntimeTask *RUNTIME_TASK_LIFECYCLE_PUBLIC
runtime_task_capability_tuple_validate(RuntimeTaskCapabilityTuple tuple) {
  if (tuple.type_token != RUNTIME_TASK_CAPABILITY_TYPE_TOKEN ||
      !runtime_task_lifecycle_address_in_bounds(
          (uintptr_t)tuple.current, (uintptr_t)tuple.lower_bound,
          (uintptr_t)tuple.upper_bound, sizeof(RuntimeTask))) {
    runtime_task_lifecycle_trap();
  }
  return tuple.current;
}

RuntimeTask *RUNTIME_TASK_LIFECYCLE_PUBLIC
runtime_task_capability_tuple_validate_thunk_a(
    RuntimeTaskCapabilityTuple tuple) {
  return runtime_task_capability_tuple_validate(tuple);
}

RuntimeTask *RUNTIME_TASK_LIFECYCLE_PUBLIC
runtime_task_capability_tuple_validate_thunk_b(
    RuntimeTaskCapabilityTuple tuple) {
  return runtime_task_capability_tuple_validate(tuple);
}

uint64_t RUNTIME_TASK_LIFECYCLE_PUBLIC runtime_task_source_token(void) {
  return UINT64_C(0x04940f9f76a3df19);
}

static void *runtime_task_start_validate_allocator_route(
    RuntimeTaskAllocatorRouteView route) {
  if (route.current == NULL && route.lower_bound == NULL &&
      route.upper_bound == NULL) {
    return NULL;
  }
  if (route.type_token != RUNTIME_TASK_ALLOCATOR_ROUTE_TYPE_TOKEN ||
      !runtime_task_lifecycle_address_in_bounds(
          (uintptr_t)route.current, (uintptr_t)route.lower_bound,
          (uintptr_t)route.upper_bound, 1)) {
    runtime_task_lifecycle_trap();
  }
  return route.current;
}

void *RUNTIME_TASK_LIFECYCLE_PUBLIC
runtime_task_start_validate_domain_allocator_route_thunk_a(
    RuntimeTaskAllocatorRouteView route) {
  return runtime_task_start_validate_allocator_route(route);
}

void *RUNTIME_TASK_LIFECYCLE_PUBLIC
runtime_task_start_validate_domain_allocator_route_thunk_b(
    RuntimeTaskAllocatorRouteView route) {
  return runtime_task_start_validate_allocator_route(route);
}

_Noreturn void RUNTIME_TASK_LIFECYCLE_PUBLIC
runtime_task_start_trampoline(uint32_t projected) {
  if (!lifecycle_configuration_bound ||
      !runtime_task_lifecycle_address_in_bounds(
          (uintptr_t)lifecycle_configuration.start_record.current,
          (uintptr_t)lifecycle_configuration.start_record.lower_bound,
          (uintptr_t)lifecycle_configuration.start_record.upper_bound,
          sizeof(RuntimeTaskStartRecord))) {
    runtime_current_task_unset_fatal();
  }
  if (projected != 0) {
    (void)runtime_task_start_validate_domain_allocator_route_thunk_a(
        lifecycle_configuration.allocator_route);
    if (runtime_task_start_validate_domain_allocator_route_thunk_b(
            lifecycle_configuration.allocator_route) != NULL) {
      runtime_task_lifecycle_trap();
    }
    if (lifecycle_configuration.prepare_projected_start != NULL) {
      lifecycle_configuration.prepare_projected_start(
          lifecycle_configuration.context);
    }
  }

  RuntimeTaskStartRecord *record = lifecycle_configuration.start_record.current;
  if (record->entry == NULL || lifecycle_configuration.exit_task == NULL) {
    runtime_task_lifecycle_trap();
  }
  const int32_t result =
      record->entry(
          record->arguments[0],
          record->arguments[1],
          record->arguments[2],
          record->arguments[3]
      );
  lifecycle_configuration.exit_task(result, lifecycle_configuration.context);
  runtime_task_lifecycle_trap();
}

void RUNTIME_TASK_LIFECYCLE_PUBLIC
runtime_task_set_state(RuntimeTaskLifecycleView task, uint32_t state) {
  RuntimeTask *record = runtime_task_lifecycle_validate_view(task);
  if (record == lifecycle_configuration.restricted_task &&
      state >= RUNTIME_TASK_STATE_IPC_WAIT) {
    runtime_task_lifecycle_trap();
  }
  record->state = state;
}

void RUNTIME_TASK_LIFECYCLE_PUBLIC
runtime_task_make_ready(RuntimeTaskLifecycleView task) {
  if (!runtime_task_lifecycle_address_in_bounds(
          (uintptr_t)task.current, (uintptr_t)task.lower_bound,
          (uintptr_t)task.upper_bound, sizeof(RuntimeTask))) {
    runtime_task_make_ready_bounds_panic();
  }
  RuntimeTask *record = task.current;
  record->state = RUNTIME_TASK_STATE_READY;
  if (!lifecycle_configuration_bound ||
      lifecycle_configuration.ready_enqueue == NULL) {
    runtime_task_lifecycle_trap();
  }
  lifecycle_configuration.ready_enqueue(
      record,
      lifecycle_configuration.context
  );
}

_Noreturn void RUNTIME_TASK_LIFECYCLE_PUBLIC
runtime_task_make_ready_bounds_panic(void) {
  runtime_task_lifecycle_trap();
}
