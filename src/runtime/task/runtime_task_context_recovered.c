#include "avp/runtime/task/runtime_task_context_recovered.h"

enum {
  RUNTIME_CURRENT_TASK_VIEW_SIZE = 0x70,
};

const RuntimeFirebloomType runtime_current_task_view_type = {
    .identity = UINT64_C(0x63757272656e7474),
    .element_size = RUNTIME_CURRENT_TASK_VIEW_SIZE,
    .bytewise_copyable = false,
};

const RuntimeFirebloomType runtime_task_context_task_type = {
    .identity = UINT64_C(0x72756e7461736b00),
    .element_size = sizeof(RuntimeTask),
    .bytewise_copyable = false,
};

static RuntimeTaskContextConfiguration g_runtime_task_context_configuration;

static void runtime_task_context_trap(void) { __builtin_trap(); }

static void *runtime_task_context_current_pointer(void) {
#if defined(__aarch64__) && defined(__ELF__)
  return runtime_task_context_read_current_arch();
#else
  return g_runtime_task_context_configuration.host_current_task;
#endif
}

static RuntimeTask *runtime_task_context_selected_pointer(void) {
#if defined(__aarch64__) && defined(__ELF__)
  return runtime_task_context_read_selected_arch();
#else
  return g_runtime_task_context_configuration.host_selected_task;
#endif
}

static bool runtime_task_context_pointer_is_bounded(
    const void *pointer,
    uintptr_t lower_bound,
    uintptr_t upper_bound
) {
  const uintptr_t address = (uintptr_t)pointer;

  return address >= lower_bound && address < upper_bound;
}

static RuntimeTask *runtime_task_context_require_selected(void) {
  RuntimeFirebloomSpan selected = runtime_selected_task_bounded_view();
  RuntimeFirebloomSpan validated = runtime_task_tuple_validate(selected);
  RuntimeTask *task = validated.current;

  if (task->frame.signed_elr_el1 == 0) {
    runtime_task_context_trap();
  }
  return task;
}

void runtime_task_context_configure(
    const RuntimeTaskContextConfiguration *configuration) {
  if (configuration == NULL || (configuration->task_registry_count != 0 &&
                                configuration->task_registry == NULL)) {
    runtime_task_context_trap();
  }

  g_runtime_task_context_configuration.host_current_task =
      configuration->host_current_task;
  g_runtime_task_context_configuration.host_selected_task =
      configuration->host_selected_task;
  g_runtime_task_context_configuration.idle_task = configuration->idle_task;
  g_runtime_task_context_configuration.task_registry =
      configuration->task_registry;
  g_runtime_task_context_configuration.task_registry_count =
      configuration->task_registry_count;
  g_runtime_task_context_configuration.guard_seed_generate =
      configuration->guard_seed_generate;
}

void *runtime_current_task_or_null(void) {
  RuntimeFirebloomSpan current = runtime_current_task_bounded_view();

  if (runtime_current_task_is_bootstrap_sentinel(current.current)) {
    return NULL;
  }
  if (!runtime_task_context_pointer_is_bounded(
          current.current, (uintptr_t)current.lower_bound,
          (uintptr_t)current.upper_bound)) {
    return NULL;
  }
  return current.current;
}

RuntimeFirebloomSpan runtime_current_task_bounded_view(void) {
  void *current = runtime_task_context_current_pointer();
  const uintptr_t address = (uintptr_t)current;

  if (current == NULL) {
    runtime_task_context_trap();
  }

  return (RuntimeFirebloomSpan){
      .current = current,
      .lower_bound = current,
      .upper_bound = (void *)(address + RUNTIME_CURRENT_TASK_VIEW_SIZE),
      .type = &runtime_current_task_view_type,
  };
}

bool runtime_current_task_is_bootstrap_sentinel(const void *task) {
  return (uintptr_t)task == RUNTIME_TASK_CONTEXT_BOOTSTRAP_SENTINEL;
}

RuntimeFirebloomSpan runtime_selected_task_bounded_view(void) {
  RuntimeTask *selected = runtime_task_context_selected_pointer();

  if (selected == NULL) {
    runtime_task_context_trap();
  }

  return (RuntimeFirebloomSpan){
      .current = selected,
      .lower_bound = selected,
      .upper_bound = selected + 1,
      .type = &runtime_task_context_task_type,
  };
}

void runtime_task_exception_frame_initialize(
    RuntimeTask *task,
    uintptr_t lower_bound,
    uintptr_t upper_bound,
    uint64_t stack_guard_seed
) {
  RuntimeDomain *domain;

  if (!runtime_task_context_pointer_is_bounded(task, lower_bound,
                                               upper_bound)) {
    runtime_task_context_trap();
  }
  if (task->state != RUNTIME_TASK_STATE_INITIALIZED &&
      task->state != RUNTIME_TASK_STATE_EXITED) {
    runtime_task_context_trap();
  }
  if (task->stack_top == 0) {
    runtime_task_context_trap();
  }

  domain = task->domain;
  if (!runtime_task_context_pointer_is_bounded(domain, task->domain_lower_bound,
                                               task->domain_upper_bound)) {
    runtime_task_context_trap();
  }
  if (stack_guard_seed == 0) {
    if (g_runtime_task_context_configuration.guard_seed_generate == NULL) {
      runtime_task_context_trap();
    }
    stack_guard_seed =
        g_runtime_task_context_configuration.guard_seed_generate();
  }

  /* These task fields feed X4-X8 in the frame initializer. */
  task->frame.x[30] = UINT64_C(0xdeadbeefcafef00d);
  task->frame.signed_elr_el1 = task->startup_sp;
  task->frame.sp_el0 = task->stack_guard_base;
  task->frame.spsr_el1 = 0;
  task->frame.tpidrro_el0 = task->frame_context;
  task->frame.ttbr1_el1 = domain->address_space_root;
  task->frame.ttbr0_el1 = domain->address_space_cookie;
  task->frame.full_vector_state = 1;
  task->frame.stack_guard = stack_guard_seed & UINT64_C(0xffffffffffff00ff);
  task->state = RUNTIME_TASK_STATE_INITIALIZED;
}

void runtime_task_require_blocking_allowed(
    RuntimeTask *task,
    uintptr_t lower_bound,
    uintptr_t upper_bound,
    bool blocking_requested
) {
  if (!blocking_requested) {
    return;
  }
  if (task == g_runtime_task_context_configuration.idle_task) {
    runtime_task_context_trap();
  }
  if (!runtime_task_context_pointer_is_bounded(task, lower_bound,
                                               upper_bound)) {
    runtime_task_context_trap();
  }
  if ((task->frame.spsr_el1 & RUNTIME_TASK_CONTEXT_INTERRUPT_MASK) != 0) {
    runtime_task_context_trap();
  }
}

bool runtime_task_shared_page_span_matches(
    uint32_t lower_page,
    uint32_t upper_page
) {
  RuntimeTask *task = runtime_task_context_require_selected();
  const uint32_t shared_page = (uint32_t)((uintptr_t)task->lifecycle_aux >> 14);

  return shared_page != 0 && lower_page == shared_page &&
         upper_page == shared_page + 1;
}

RuntimeFirebloomSpan runtime_task_tuple_validate(RuntimeFirebloomSpan task) {
  const uintptr_t current = (uintptr_t)task.current;
  const uintptr_t lower = (uintptr_t)task.lower_bound;
  const uintptr_t upper = (uintptr_t)task.upper_bound;

  if (task.type != &runtime_task_context_task_type || current < lower ||
      current >= upper || upper - current < sizeof(RuntimeTask)) {
    runtime_task_context_trap();
  }
  return task;
}

RuntimeFirebloomSpan runtime_task_lookup_by_domain_id(uint8_t identifier) {
  for (size_t index = 0;
       index < g_runtime_task_context_configuration.task_registry_count;
       ++index) {
    RuntimeFirebloomSpan task = runtime_task_tuple_validate(
        g_runtime_task_context_configuration.task_registry[index]);
    RuntimeTask *record = task.current;

    /* The lookup compares the 32-bit field at task +0x400. */
    if ((uint8_t)record->global_ordinal == identifier) {
      return task;
    }
  }

  return (RuntimeFirebloomSpan){0};
}
