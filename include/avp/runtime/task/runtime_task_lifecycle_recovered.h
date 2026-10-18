#pragma once

#include "avp/runtime/task/runtime_task.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if __has_attribute(retain)
#define RUNTIME_TASK_LIFECYCLE_PUBLIC __attribute__((used, retain))
#else
#define RUNTIME_TASK_LIFECYCLE_PUBLIC __attribute__((used))
#endif

#define RUNTIME_TASK_CAPABILITY_TYPE_TOKEN UINT64_C(0x00147d00)
#define RUNTIME_TASK_ALLOCATOR_ROUTE_TYPE_TOKEN UINT64_C(0x001476b0)

typedef struct {
  RuntimeTask *current;
  RuntimeTask *lower_bound;
  RuntimeTask *upper_bound;
} RuntimeTaskLifecycleView;

typedef struct {
  RuntimeTask *current;
  RuntimeTask *lower_bound;
  RuntimeTask *upper_bound;
  uintptr_t type_token;
} RuntimeTaskCapabilityTuple;

typedef int32_t (*RuntimeTaskStartEntry)(
  uintptr_t argument0,
  uintptr_t argument1,
  uintptr_t argument2,
  uintptr_t argument3
);

typedef struct {
  uint8_t reserved_00[0x30];
  RuntimeTaskStartEntry entry;
  uint8_t reserved_38[0x08];
  uintptr_t arguments[4];
} RuntimeTaskStartRecord;

typedef struct {
  RuntimeTaskStartRecord *current;
  RuntimeTaskStartRecord *lower_bound;
  RuntimeTaskStartRecord *upper_bound;
} RuntimeTaskStartRecordView;

typedef struct {
  void *current;
  void *lower_bound;
  void *upper_bound;
  uintptr_t type_token;
} RuntimeTaskAllocatorRouteView;

_Static_assert(sizeof(RuntimeTaskCapabilityTuple) == 0x20,
               "task capability tuple size");
_Static_assert(sizeof(RuntimeTaskStartRecord) == 0x60,
               "task start record size");
_Static_assert(offsetof(RuntimeTaskStartRecord, entry) == 0x30,
               "task start entry offset");
_Static_assert(offsetof(RuntimeTaskStartRecord, arguments) == 0x40,
               "task start arguments offset");

typedef void (*RuntimeTaskLifecycleOperation)(void *context);
typedef void (*RuntimeTaskLifecycleReadyEnqueue)(
  RuntimeTask *task,
  void *context
);
typedef void (*RuntimeTaskLifecycleExit)(int32_t result, void *context);

typedef struct {
  RuntimeTask *restricted_task;
  RuntimeTaskStartRecordView start_record;
  RuntimeTaskAllocatorRouteView allocator_route;
  RuntimeTaskLifecycleOperation prepare_projected_start;
  RuntimeTaskLifecycleReadyEnqueue ready_enqueue;
  RuntimeTaskLifecycleExit exit_task;
  void *context;
} RuntimeTaskLifecycleConfiguration;

void runtime_task_lifecycle_configure(
    const RuntimeTaskLifecycleConfiguration *configuration);

RUNTIME_TASK_LIFECYCLE_PUBLIC _Noreturn void
runtime_current_task_unset_fatal(void);
RUNTIME_TASK_LIFECYCLE_PUBLIC RuntimeTask *
runtime_task_capability_tuple_validate_thunk_a(
    RuntimeTaskCapabilityTuple tuple);
RUNTIME_TASK_LIFECYCLE_PUBLIC RuntimeTask *
runtime_task_capability_tuple_validate_thunk_b(
    RuntimeTaskCapabilityTuple tuple);
RUNTIME_TASK_LIFECYCLE_PUBLIC uint64_t runtime_task_source_token(void);
RUNTIME_TASK_LIFECYCLE_PUBLIC RuntimeTask *
runtime_task_capability_tuple_validate(RuntimeTaskCapabilityTuple tuple);
RUNTIME_TASK_LIFECYCLE_PUBLIC _Noreturn void
runtime_task_start_trampoline(uint32_t projected);
RUNTIME_TASK_LIFECYCLE_PUBLIC void *
runtime_task_start_validate_domain_allocator_route_thunk_a(
    RuntimeTaskAllocatorRouteView route);
RUNTIME_TASK_LIFECYCLE_PUBLIC void *
runtime_task_start_validate_domain_allocator_route_thunk_b(
    RuntimeTaskAllocatorRouteView route);
RUNTIME_TASK_LIFECYCLE_PUBLIC void
runtime_task_set_state(RuntimeTaskLifecycleView task, uint32_t state);
RUNTIME_TASK_LIFECYCLE_PUBLIC void
runtime_task_make_ready(RuntimeTaskLifecycleView task);
RUNTIME_TASK_LIFECYCLE_PUBLIC _Noreturn void
runtime_task_make_ready_bounds_panic(void);
