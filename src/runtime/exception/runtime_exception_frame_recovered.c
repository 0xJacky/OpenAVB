#include "avp/runtime/exception/runtime_exception_frame_recovered.h"

typedef struct {
  RuntimeExceptionHandlerStack handler_stack;
  RuntimeExceptionSvcDispatch svc_dispatch;
  RuntimeExceptionFullDispatch full_dispatch;
  void *dispatch_context;
  bool handler_stack_configured;
} RuntimeExceptionFrameState;

static RuntimeExceptionFrameState g_runtime_exception_frame_state;

static void runtime_exception_frame_trap(void) { __builtin_trap(); }

static uintptr_t runtime_exception_checked_stack_address(size_t offset) {
  const uintptr_t base = g_runtime_exception_frame_state.handler_stack.base;

  if (offset > UINTPTR_MAX - base) {
    runtime_exception_frame_trap();
  }
  return base + offset;
}

static void runtime_exception_require_frame(
    const RuntimeTaskFrame *frame,
    uintptr_t lower_bound,
    uintptr_t upper_bound,
    void (*bounds_panic)(void)
) {
  const uintptr_t address = (uintptr_t)frame;

  /* The capability check validates the frame start address. */
  if (address < lower_bound || address >= upper_bound) {
    bounds_panic();
  }
}

static void runtime_exception_capture_common(
    RuntimeTaskFrame *frame,
    const RuntimeExceptionRegisterCapture *capture
) {
  if (frame == NULL || capture == NULL) {
    runtime_exception_frame_trap();
  }

  frame->sp_el0 = capture->sp_el0;
  frame->spsr_el1 = capture->spsr_el1;
  frame->fpsr = capture->fpsr;
  frame->fpcr = capture->fpcr;
  frame->tpidr_el0 = capture->tpidr_el0;
  runtime_exception_outlined_save_ttbr0_ttbr1_x21(
      frame,
      capture->ttbr0_el1,
      capture->ttbr1_el1
  );
}

void runtime_exception_frame_configure_handler_stack(
    const RuntimeExceptionHandlerStack *stack) {
  if (stack == NULL || stack->lower_offset >= stack->upper_offset ||
      stack->upper_offset > UINTPTR_MAX - stack->base) {
    runtime_exception_frame_trap();
  }

  g_runtime_exception_frame_state.handler_stack = *stack;
  g_runtime_exception_frame_state.handler_stack_configured = true;
}

void runtime_exception_frame_bind_dispatch(
    RuntimeExceptionSvcDispatch svc_dispatch,
    RuntimeExceptionFullDispatch full_dispatch,
    void *context
) {
  if (svc_dispatch == NULL || full_dispatch == NULL) {
    runtime_exception_frame_trap();
  }

  g_runtime_exception_frame_state.svc_dispatch = svc_dispatch;
  g_runtime_exception_frame_state.full_dispatch = full_dispatch;
  g_runtime_exception_frame_state.dispatch_context = context;
}

RuntimeTaskFrame *
runtime_exception_svc_entry(
    RuntimeTaskFrame *frame,
    uint64_t esr_el1,
    const RuntimeExceptionRegisterCapture *capture
) {
  if (g_runtime_exception_frame_state.svc_dispatch == NULL) {
    runtime_exception_frame_trap();
  }

  runtime_exception_capture_common(frame, capture);
  for (size_t index = 8; index < 16; ++index) {
    frame->q[index] = capture->q[index];
  }

  return g_runtime_exception_frame_state.svc_dispatch(
      frame,
      (uint16_t)esr_el1,
      g_runtime_exception_frame_state.dispatch_context
  );
}

RuntimeTaskFrame *runtime_exception_lower_sync_full_save_entry(
    RuntimeTaskFrame *frame,
    const RuntimeExceptionRegisterCapture *capture
) {
  if (frame == NULL) {
    runtime_exception_frame_trap();
  }

  frame->full_vector_state = 1;
  return runtime_exception_full_frame_dispatch(frame, capture);
}

RuntimeTaskFrame *runtime_exception_full_frame_dispatch(
    RuntimeTaskFrame *frame,
    const RuntimeExceptionRegisterCapture *capture
) {
  if (g_runtime_exception_frame_state.full_dispatch == NULL) {
    runtime_exception_frame_trap();
  }

  runtime_exception_capture_common(frame, capture);
  for (size_t index = 0; index < 32; ++index) {
    frame->q[index] = capture->q[index];
  }

  return g_runtime_exception_frame_state.full_dispatch(
      frame,
      g_runtime_exception_frame_state.dispatch_context
  );
}

uintptr_t runtime_exception_handler_stack_pointer(void) {
  if (!g_runtime_exception_frame_state.handler_stack_configured) {
    runtime_exception_frame_trap();
  }

  return runtime_exception_checked_stack_address(
      g_runtime_exception_frame_state.handler_stack.upper_offset);
}

void runtime_exception_validate_handler_stack(uintptr_t stack_pointer) {
  uintptr_t lower;
  uintptr_t upper;

  if (!g_runtime_exception_frame_state.handler_stack_configured) {
    runtime_exception_frame_trap();
  }

  lower = runtime_exception_checked_stack_address(
      g_runtime_exception_frame_state.handler_stack.lower_offset);
  upper = runtime_exception_checked_stack_address(
      g_runtime_exception_frame_state.handler_stack.upper_offset);

  /* A downward-growing handler stack owns the interval (lower, upper]. */
  if (stack_pointer <= lower || stack_pointer > upper) {
    runtime_exception_frame_trap();
  }
}

void runtime_exception_outlined_save_ttbr0_ttbr1_x21(
    RuntimeTaskFrame *frame,
    uint64_t ttbr0_el1,
    uint64_t ttbr1_el1
) {
  if (frame == NULL) {
    runtime_exception_frame_trap();
  }

  frame->ttbr0_el1 = ttbr0_el1;
  frame->ttbr1_el1 = ttbr1_el1;
}

uint64_t runtime_task_interrupt_mask_set(
    RuntimeTaskFrame *frame,
    uintptr_t lower_bound,
    uintptr_t upper_bound
) {
  uint64_t previous;

  runtime_exception_require_frame(
      frame,
      lower_bound,
      upper_bound,
      runtime_task_interrupt_mask_set_bounds_panic
  );
  previous = frame->spsr_el1 & RUNTIME_EXCEPTION_INTERRUPT_MASK;
  frame->spsr_el1 |= RUNTIME_EXCEPTION_INTERRUPT_MASK;
  return previous;
}

__attribute__((noreturn)) void
runtime_task_interrupt_mask_set_bounds_panic(void) {
  runtime_exception_frame_trap();
  __builtin_unreachable();
}

uint64_t runtime_task_interrupt_mask_clear(
    RuntimeTaskFrame *frame,
    uintptr_t lower_bound,
    uintptr_t upper_bound
) {
  uint64_t previous;

  runtime_exception_require_frame(
      frame,
      lower_bound,
      upper_bound,
      runtime_task_interrupt_mask_clear_bounds_panic
  );
  previous = frame->spsr_el1 & RUNTIME_EXCEPTION_INTERRUPT_MASK;
  frame->spsr_el1 &= ~RUNTIME_EXCEPTION_INTERRUPT_MASK;
  return previous;
}

__attribute__((noreturn)) void
runtime_task_interrupt_mask_clear_bounds_panic(void) {
  runtime_exception_frame_trap();
  __builtin_unreachable();
}

bool runtime_task_interrupt_mask_is_set(
    const RuntimeTaskFrame *frame,
    uintptr_t lower_bound,
    uintptr_t upper_bound
) {
  runtime_exception_require_frame(
      frame,
      lower_bound,
      upper_bound,
      runtime_task_interrupt_mask_test_bounds_panic
  );
  return (frame->spsr_el1 & RUNTIME_EXCEPTION_INTERRUPT_MASK) != 0;
}

__attribute__((noreturn)) void
runtime_task_interrupt_mask_test_bounds_panic(void) {
  runtime_exception_frame_trap();
  __builtin_unreachable();
}
