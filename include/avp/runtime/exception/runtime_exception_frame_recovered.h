#pragma once

#include "avp/runtime/task/runtime_task.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RUNTIME_EXCEPTION_INTERRUPT_MASK UINT64_C(0xc0)

#if __has_attribute(retain)
#define RUNTIME_EXCEPTION_FRAME_RECOVERED_ENTRY __attribute__((used, retain))
#else
#define RUNTIME_EXCEPTION_FRAME_RECOVERED_ENTRY __attribute__((used))
#endif

typedef struct {
  uintptr_t base;
  size_t lower_offset;
  size_t upper_offset;
} RuntimeExceptionHandlerStack;

typedef struct {
  uint64_t sp_el0;
  uint64_t spsr_el1;
  uint64_t fpsr;
  uint64_t fpcr;
  uint64_t tpidr_el0;
  uint64_t ttbr0_el1;
  uint64_t ttbr1_el1;
  __uint128_t q[32];
} RuntimeExceptionRegisterCapture;

_Static_assert(offsetof(RuntimeExceptionRegisterCapture, q) == 0x40,
               "register capture vector offset");
_Static_assert(sizeof(RuntimeExceptionRegisterCapture) == 0x240,
               "register capture size");

typedef RuntimeTaskFrame *(*RuntimeExceptionSvcDispatch)(
  RuntimeTaskFrame *frame,
  uint16_t immediate,
  void *context
);

typedef RuntimeTaskFrame *(*RuntimeExceptionFullDispatch)(
  RuntimeTaskFrame *frame,
  void *context
);

void runtime_exception_frame_configure_handler_stack(
    const RuntimeExceptionHandlerStack *stack);
void runtime_exception_frame_bind_dispatch(
  RuntimeExceptionSvcDispatch svc_dispatch,
  RuntimeExceptionFullDispatch full_dispatch,
  void *context
);

RUNTIME_EXCEPTION_FRAME_RECOVERED_ENTRY RuntimeTaskFrame *
runtime_exception_svc_entry(
  RuntimeTaskFrame *frame,
  uint64_t esr_el1,
  const RuntimeExceptionRegisterCapture *capture
);
RUNTIME_EXCEPTION_FRAME_RECOVERED_ENTRY RuntimeTaskFrame *
runtime_exception_lower_sync_full_save_entry(
  RuntimeTaskFrame *frame,
  const RuntimeExceptionRegisterCapture *capture
);
RUNTIME_EXCEPTION_FRAME_RECOVERED_ENTRY RuntimeTaskFrame *
runtime_exception_full_frame_dispatch(
  RuntimeTaskFrame *frame,
  const RuntimeExceptionRegisterCapture *capture
);

RUNTIME_EXCEPTION_FRAME_RECOVERED_ENTRY uintptr_t
runtime_exception_handler_stack_pointer(void);
RUNTIME_EXCEPTION_FRAME_RECOVERED_ENTRY void
runtime_exception_validate_handler_stack(uintptr_t stack_pointer);

RUNTIME_EXCEPTION_FRAME_RECOVERED_ENTRY void
runtime_exception_outlined_save_ttbr0_ttbr1_x21(
  RuntimeTaskFrame *frame,
  uint64_t ttbr0_el1,
  uint64_t ttbr1_el1
);

RUNTIME_EXCEPTION_FRAME_RECOVERED_ENTRY uint64_t
runtime_task_interrupt_mask_set(
  RuntimeTaskFrame *frame,
  uintptr_t lower_bound,
  uintptr_t upper_bound
);
RUNTIME_EXCEPTION_FRAME_RECOVERED_ENTRY __attribute__((noreturn)) void
runtime_task_interrupt_mask_set_bounds_panic(void);
RUNTIME_EXCEPTION_FRAME_RECOVERED_ENTRY uint64_t
runtime_task_interrupt_mask_clear(
  RuntimeTaskFrame *frame,
  uintptr_t lower_bound,
  uintptr_t upper_bound
);
RUNTIME_EXCEPTION_FRAME_RECOVERED_ENTRY __attribute__((noreturn)) void
runtime_task_interrupt_mask_clear_bounds_panic(void);
RUNTIME_EXCEPTION_FRAME_RECOVERED_ENTRY bool
runtime_task_interrupt_mask_is_set(
  const RuntimeTaskFrame *frame,
  uintptr_t lower_bound,
  uintptr_t upper_bound
);
RUNTIME_EXCEPTION_FRAME_RECOVERED_ENTRY __attribute__((noreturn)) void
runtime_task_interrupt_mask_test_bounds_panic(void);

void runtime_exception_capture_system_registers_arch(
    RuntimeExceptionRegisterCapture *capture);
