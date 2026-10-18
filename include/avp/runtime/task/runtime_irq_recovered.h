#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if __has_attribute(retain)
#define RUNTIME_IRQ_RECOVERED_ENTRY __attribute__((used, retain))
#else
#define RUNTIME_IRQ_RECOVERED_ENTRY __attribute__((used))
#endif

enum {
  RUNTIME_INTERRUPT_EVENT_EXTERNAL = 1,
  RUNTIME_INTERRUPT_EVENT_TIMER = 2,
};

typedef struct {
  uint32_t nesting_depth;
  uint8_t interrupt_mask_depth;
  uint8_t saved_state_flag;
} RuntimeInterruptTaskState;

typedef struct {
  uint8_t *current;
  uint8_t *lower_bound;
  uint8_t *upper_bound;
  uintptr_t type_token;
} RuntimeInterruptStateFlagsView;

typedef bool (*RuntimeInterruptEventNext)(uint32_t *event, void *context);
typedef void (*RuntimeInterruptOperation)(void *context);
typedef uint64_t (*RuntimeInterruptCounterRead)(void *context);

typedef struct {
  RuntimeInterruptTaskState *current_task;
  uint8_t *state_flags;
  RuntimeInterruptEventNext next_event;
  RuntimeInterruptOperation push_mask;
  RuntimeInterruptOperation pop_mask;
  RuntimeInterruptOperation external_event;
  RuntimeInterruptOperation timer_event;
  RuntimeInterruptOperation wait_for_interrupt;
  RuntimeInterruptCounterRead counter_read;
  uint64_t *worker_runtime;
  void *context;
} RuntimeInterruptConfiguration;

void runtime_interrupt_configure(
    const RuntimeInterruptConfiguration *configuration);
void runtime_interrupt_host_timer_state(uint64_t control, uint32_t tval);

RUNTIME_IRQ_RECOVERED_ENTRY uint64_t runtime_virtual_timer_control_read(void);
RUNTIME_IRQ_RECOVERED_ENTRY void
runtime_virtual_timer_control_write(uint64_t value);
RUNTIME_IRQ_RECOVERED_ENTRY void
runtime_virtual_timer_tval_write(uint32_t value);
RUNTIME_IRQ_RECOVERED_ENTRY void runtime_wait_for_interrupt_once(void);
RUNTIME_IRQ_RECOVERED_ENTRY void runtime_interrupt_mask_push_svc(void);
RUNTIME_IRQ_RECOVERED_ENTRY void runtime_interrupt_mask_pop_svc(void);
RUNTIME_IRQ_RECOVERED_ENTRY void
runtime_virtual_timer_irq_post_initialize_noop(void);
RUNTIME_IRQ_RECOVERED_ENTRY void runtime_interrupt_worker_case1_hook_noop(void);
RUNTIME_IRQ_RECOVERED_ENTRY void
runtime_current_task_interrupt_mask_depth_increment(void);
RUNTIME_IRQ_RECOVERED_ENTRY void
runtime_current_task_interrupt_mask_depth_decrement(void);
RUNTIME_IRQ_RECOVERED_ENTRY bool
runtime_current_task_interrupt_mask_depth_is_zero(void);
RUNTIME_IRQ_RECOVERED_ENTRY uint64_t runtime_interrupt_source_token(void);
RUNTIME_IRQ_RECOVERED_ENTRY bool
runtime_interrupt_state_flags_compare_sp_current_to_upper(
    RuntimeInterruptStateFlagsView view);
RUNTIME_IRQ_RECOVERED_ENTRY RuntimeInterruptStateFlagsView
runtime_interrupt_state_flags_prepare_sp_result_tuple(void);
RUNTIME_IRQ_RECOVERED_ENTRY RuntimeInterruptStateFlagsView
runtime_interrupt_state_flags_bounded_view(void);
RUNTIME_IRQ_RECOVERED_ENTRY void runtime_interrupt_worker(void);
RUNTIME_IRQ_RECOVERED_ENTRY RuntimeInterruptStateFlagsView
runtime_interrupt_worker_state_flags_to_stack(void);
RUNTIME_IRQ_RECOVERED_ENTRY void j_runtime_wait_for_interrupt_once(void);
