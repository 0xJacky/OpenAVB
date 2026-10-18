#include "avp/runtime/task/runtime_irq_recovered.h"

enum {
  RUNTIME_INTERRUPT_STATE_TYPE_TOKEN = 0x69727166,
};

static RuntimeInterruptConfiguration interrupt_configuration;
static uint64_t host_timer_control;
static uint32_t host_timer_tval;

static _Noreturn void runtime_interrupt_trap(void) {
  __builtin_trap();
  __builtin_unreachable();
}

static RuntimeInterruptTaskState *runtime_interrupt_current_task(void) {
  if (interrupt_configuration.current_task == NULL) {
    runtime_interrupt_trap();
  }
  return interrupt_configuration.current_task;
}

void runtime_interrupt_configure(
    const RuntimeInterruptConfiguration *configuration) {
  if (configuration == NULL || configuration->state_flags == NULL) {
    runtime_interrupt_trap();
  }
  interrupt_configuration = *configuration;
}

void runtime_interrupt_host_timer_state(uint64_t control, uint32_t tval) {
  host_timer_control = control;
  host_timer_tval = tval;
}

#if !defined(__aarch64__) || !defined(__ELF__)
uint64_t RUNTIME_IRQ_RECOVERED_ENTRY runtime_virtual_timer_control_read(void) {
  return host_timer_control;
}

void RUNTIME_IRQ_RECOVERED_ENTRY
runtime_virtual_timer_control_write(uint64_t value) {
  host_timer_control = value;
}

void RUNTIME_IRQ_RECOVERED_ENTRY
runtime_virtual_timer_tval_write(uint32_t value) {
  host_timer_tval = value;
}

void RUNTIME_IRQ_RECOVERED_ENTRY runtime_wait_for_interrupt_once(void) {
  if (interrupt_configuration.wait_for_interrupt != NULL) {
    interrupt_configuration.wait_for_interrupt(interrupt_configuration.context);
  }
}

void RUNTIME_IRQ_RECOVERED_ENTRY runtime_interrupt_mask_push_svc(void) {
  if (interrupt_configuration.push_mask != NULL) {
    interrupt_configuration.push_mask(interrupt_configuration.context);
  }
}

void RUNTIME_IRQ_RECOVERED_ENTRY runtime_interrupt_mask_pop_svc(void) {
  if (interrupt_configuration.pop_mask != NULL) {
    interrupt_configuration.pop_mask(interrupt_configuration.context);
  }
}
#endif

void RUNTIME_IRQ_RECOVERED_ENTRY
runtime_virtual_timer_irq_post_initialize_noop(void) {}

void RUNTIME_IRQ_RECOVERED_ENTRY
runtime_interrupt_worker_case1_hook_noop(void) {}

void RUNTIME_IRQ_RECOVERED_ENTRY
runtime_current_task_interrupt_mask_depth_increment(void) {
  RuntimeInterruptTaskState *task = runtime_interrupt_current_task();

  ++task->interrupt_mask_depth;
}

void RUNTIME_IRQ_RECOVERED_ENTRY
runtime_current_task_interrupt_mask_depth_decrement(void) {
  RuntimeInterruptTaskState *task = runtime_interrupt_current_task();

  --task->interrupt_mask_depth;
}

bool RUNTIME_IRQ_RECOVERED_ENTRY
runtime_current_task_interrupt_mask_depth_is_zero(void) {
  return runtime_interrupt_current_task()->interrupt_mask_depth == 0;
}

uint64_t RUNTIME_IRQ_RECOVERED_ENTRY runtime_interrupt_source_token(void) {
  return UINT64_C(0x0955b303f8cbfe51);
}

RuntimeInterruptStateFlagsView RUNTIME_IRQ_RECOVERED_ENTRY
runtime_interrupt_state_flags_bounded_view(void) {
  uint8_t *flag = interrupt_configuration.state_flags;

  return (RuntimeInterruptStateFlagsView){
      .current = flag,
      .lower_bound = flag,
      .upper_bound = flag + 1,
      .type_token = RUNTIME_INTERRUPT_STATE_TYPE_TOKEN,
  };
}

bool RUNTIME_IRQ_RECOVERED_ENTRY
runtime_interrupt_state_flags_compare_sp_current_to_upper(
    RuntimeInterruptStateFlagsView view) {
  return view.current < view.upper_bound;
}

RuntimeInterruptStateFlagsView RUNTIME_IRQ_RECOVERED_ENTRY
runtime_interrupt_state_flags_prepare_sp_result_tuple(void) {
  return runtime_interrupt_state_flags_bounded_view();
}

RuntimeInterruptStateFlagsView RUNTIME_IRQ_RECOVERED_ENTRY
runtime_interrupt_worker_state_flags_to_stack(void) {
  return runtime_interrupt_state_flags_bounded_view();
}

void RUNTIME_IRQ_RECOVERED_ENTRY runtime_interrupt_worker(void) {
  uint32_t event;

  if (interrupt_configuration.next_event == NULL) {
    runtime_interrupt_trap();
  }
  while (interrupt_configuration.next_event(&event,
                                            interrupt_configuration.context)) {
    const uint64_t start = interrupt_configuration.counter_read == NULL
                               ? 0
                               : interrupt_configuration.counter_read(
                                     interrupt_configuration.context);
    RuntimeInterruptTaskState *task = runtime_interrupt_current_task();
    RuntimeInterruptStateFlagsView flags =
        runtime_interrupt_worker_state_flags_to_stack();

    if (!runtime_interrupt_state_flags_compare_sp_current_to_upper(flags)) {
      runtime_interrupt_trap();
    }
    const uint8_t saved_flag = *flags.current;
    *flags.current |= 1u;
    runtime_current_task_interrupt_mask_depth_increment();

    if (event == RUNTIME_INTERRUPT_EVENT_EXTERNAL) {
      if (interrupt_configuration.external_event != NULL) {
        interrupt_configuration.external_event(interrupt_configuration.context);
      }
    } else if (event == RUNTIME_INTERRUPT_EVENT_TIMER) {
      if (interrupt_configuration.timer_event != NULL) {
        interrupt_configuration.timer_event(interrupt_configuration.context);
      }
    } else {
      runtime_interrupt_trap();
    }

    runtime_current_task_interrupt_mask_depth_decrement();
    task->saved_state_flag = saved_flag;
    *flags.current = saved_flag;
    if (interrupt_configuration.counter_read != NULL &&
        interrupt_configuration.worker_runtime != NULL) {
      *interrupt_configuration.worker_runtime +=
          interrupt_configuration.counter_read(
              interrupt_configuration.context) -
          start;
    }
  }
}

void RUNTIME_IRQ_RECOVERED_ENTRY j_runtime_wait_for_interrupt_once(void) {
  runtime_wait_for_interrupt_once();
}
