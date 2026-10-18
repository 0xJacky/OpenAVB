#pragma once

#include "avp/runtime/task/runtime_task.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RUNTIME_TASK_CONTEXT_BOOTSTRAP_SENTINEL UINTPTR_MAX
#define RUNTIME_TASK_CONTEXT_INTERRUPT_MASK UINT64_C(0xc0)

#if __has_attribute(retain)
#define RUNTIME_TASK_CONTEXT_RECOVERED_ENTRY __attribute__((used, retain))
#else
#define RUNTIME_TASK_CONTEXT_RECOVERED_ENTRY __attribute__((used))
#endif

typedef uint64_t (*RuntimeTaskContextGuardSeedGenerate)(void);

typedef struct {
  void *host_current_task;
  RuntimeTask *host_selected_task;
  const RuntimeTask *idle_task;
  const RuntimeFirebloomSpan *task_registry;
  size_t task_registry_count;
  RuntimeTaskContextGuardSeedGenerate guard_seed_generate;
} RuntimeTaskContextConfiguration;

extern const RuntimeFirebloomType runtime_current_task_view_type;
extern const RuntimeFirebloomType runtime_task_context_task_type;

void runtime_task_context_configure(
    const RuntimeTaskContextConfiguration *configuration);

RUNTIME_TASK_CONTEXT_RECOVERED_ENTRY void *runtime_current_task_or_null(void);
RUNTIME_TASK_CONTEXT_RECOVERED_ENTRY RuntimeFirebloomSpan
runtime_current_task_bounded_view(void);
RUNTIME_TASK_CONTEXT_RECOVERED_ENTRY bool
runtime_current_task_is_bootstrap_sentinel(const void *task);
RUNTIME_TASK_CONTEXT_RECOVERED_ENTRY RuntimeFirebloomSpan
runtime_selected_task_bounded_view(void);

RUNTIME_TASK_CONTEXT_RECOVERED_ENTRY void
runtime_task_exception_frame_initialize(
  RuntimeTask *task,
  uintptr_t lower_bound,
  uintptr_t upper_bound,
  uint64_t stack_guard_seed
);
RUNTIME_TASK_CONTEXT_RECOVERED_ENTRY void
runtime_task_require_blocking_allowed(
  RuntimeTask *task,
  uintptr_t lower_bound,
  uintptr_t upper_bound,
  bool blocking_requested
);
RUNTIME_TASK_CONTEXT_RECOVERED_ENTRY bool
runtime_task_shared_page_span_matches(uint32_t lower_page, uint32_t upper_page);
RUNTIME_TASK_CONTEXT_RECOVERED_ENTRY RuntimeFirebloomSpan
runtime_task_tuple_validate(RuntimeFirebloomSpan task);
RUNTIME_TASK_CONTEXT_RECOVERED_ENTRY RuntimeFirebloomSpan
runtime_task_lookup_by_domain_id(uint8_t identifier);

void *runtime_task_context_read_current_arch(void);
RuntimeTask *runtime_task_context_read_selected_arch(void);
