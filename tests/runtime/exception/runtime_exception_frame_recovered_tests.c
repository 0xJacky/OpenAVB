#include "avp/runtime/exception/runtime_exception_frame_recovered.h"

#include <assert.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct {
  unsigned svc_calls;
  unsigned full_calls;
  uint16_t immediate;
  RuntimeTaskFrame *return_frame;
} DispatchProbe;

static RuntimeTaskFrame *svc_dispatch(RuntimeTaskFrame *frame,
                                      uint16_t immediate, void *context) {
  DispatchProbe *probe = context;

  ++probe->svc_calls;
  probe->immediate = immediate;
  assert(frame != NULL);
  return probe->return_frame;
}

static RuntimeTaskFrame *full_dispatch(RuntimeTaskFrame *frame, void *context) {
  DispatchProbe *probe = context;

  ++probe->full_calls;
  assert(frame != NULL);
  return probe->return_frame;
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

static void invalid_stack_lower(void) {
  runtime_exception_validate_handler_stack((uintptr_t)0x10001000);
}

static void invalid_stack_upper(void) {
  runtime_exception_validate_handler_stack((uintptr_t)0x10003001);
}

static RuntimeTaskFrame g_bounds_frame;

static void invalid_mask_lower(void) {
  (void)runtime_task_interrupt_mask_set(
      &g_bounds_frame, (uintptr_t)&g_bounds_frame + 1, UINTPTR_MAX);
}

static void invalid_mask_upper(void) {
  (void)runtime_task_interrupt_mask_clear(&g_bounds_frame, 0,
                                          (uintptr_t)&g_bounds_frame);
}

static void direct_set_panic(void) {
  runtime_task_interrupt_mask_set_bounds_panic();
}

static void direct_clear_panic(void) {
  runtime_task_interrupt_mask_clear_bounds_panic();
}

static void direct_test_panic(void) {
  runtime_task_interrupt_mask_test_bounds_panic();
}

static void test_handler_stack(void) {
  const RuntimeExceptionHandlerStack stack = {
      .base = (uintptr_t)0x10000000,
      .lower_offset = 0x1000,
      .upper_offset = 0x3000,
  };

  runtime_exception_frame_configure_handler_stack(&stack);
  assert(runtime_exception_handler_stack_pointer() == (uintptr_t)0x10003000);
  runtime_exception_validate_handler_stack((uintptr_t)0x10001001);
  runtime_exception_validate_handler_stack((uintptr_t)0x10003000);
  expect_trap(invalid_stack_lower);
  expect_trap(invalid_stack_upper);
}

static void test_exception_dispatch(void) {
  RuntimeTaskFrame frame;
  RuntimeTaskFrame next;
  RuntimeExceptionRegisterCapture capture;
  DispatchProbe probe = {.return_frame = &next};

  memset(&frame, 0xa5, sizeof(frame));
  memset(&capture, 0, sizeof(capture));
  capture.sp_el0 = UINT64_C(0x1111222233334444);
  capture.spsr_el1 = UINT64_C(0x5555666677778888);
  capture.fpsr = UINT64_C(0x01020304);
  capture.fpcr = UINT64_C(0x05060708);
  capture.tpidr_el0 = UINT64_C(0x9999aaaabbbbcccc);
  capture.ttbr0_el1 = UINT64_C(0x1000000080000000);
  capture.ttbr1_el1 = UINT64_C(0x2000000080000000);
  for (size_t index = 0; index < 32; ++index) {
    capture.q[index] = (__uint128_t)(index + 1) << 64 | index;
  }

  runtime_exception_frame_bind_dispatch(svc_dispatch, full_dispatch, &probe);
  assert(runtime_exception_svc_entry(&frame, UINT64_C(0x5600abcd), &capture) ==
         &next);
  assert(probe.svc_calls == 1);
  assert(probe.immediate == UINT16_C(0xabcd));
  assert(frame.sp_el0 == capture.sp_el0);
  assert(frame.spsr_el1 == capture.spsr_el1);
  assert(frame.fpsr == capture.fpsr);
  assert(frame.fpcr == capture.fpcr);
  assert(frame.tpidr_el0 == capture.tpidr_el0);
  assert(frame.ttbr0_el1 == capture.ttbr0_el1);
  assert(frame.ttbr1_el1 == capture.ttbr1_el1);
  for (size_t index = 8; index < 16; ++index) {
    assert(frame.q[index] == capture.q[index]);
  }
  assert(frame.q[0] != capture.q[0]);

  assert(runtime_exception_lower_sync_full_save_entry(&frame, &capture) ==
         &next);
  assert(probe.full_calls == 1);
  assert(frame.full_vector_state == 1);
  for (size_t index = 0; index < 32; ++index) {
    assert(frame.q[index] == capture.q[index]);
  }
}

static void test_ttbr_capture(void) {
  RuntimeTaskFrame frame;

  memset(&frame, 0, sizeof(frame));
  runtime_exception_outlined_save_ttbr0_ttbr1_x21(
      &frame, UINT64_C(0x1122334455667788), UINT64_C(0x8877665544332211));
  assert(frame.ttbr0_el1 == UINT64_C(0x1122334455667788));
  assert(frame.ttbr1_el1 == UINT64_C(0x8877665544332211));
}

static void test_interrupt_mask(void) {
  RuntimeTaskFrame frame;
  const uintptr_t lower = (uintptr_t)&frame;
  const uintptr_t upper = lower + sizeof(frame);

  memset(&frame, 0, sizeof(frame));
  frame.spsr_el1 = UINT64_C(0x12340005);
  assert(!runtime_task_interrupt_mask_is_set(&frame, lower, upper));
  assert(runtime_task_interrupt_mask_set(&frame, lower, upper) == 0);
  assert(frame.spsr_el1 == UINT64_C(0x123400c5));
  assert(runtime_task_interrupt_mask_is_set(&frame, lower, upper));
  assert(runtime_task_interrupt_mask_set(&frame, lower, upper) ==
         RUNTIME_EXCEPTION_INTERRUPT_MASK);
  assert(runtime_task_interrupt_mask_clear(&frame, lower, upper) ==
         RUNTIME_EXCEPTION_INTERRUPT_MASK);
  assert(frame.spsr_el1 == UINT64_C(0x12340005));
  assert(runtime_task_interrupt_mask_clear(&frame, lower, upper) == 0);

  expect_trap(invalid_mask_lower);
  expect_trap(invalid_mask_upper);
  expect_trap(direct_set_panic);
  expect_trap(direct_clear_panic);
  expect_trap(direct_test_panic);
}

int main(void) {
  test_handler_stack();
  test_exception_dispatch();
  test_ttbr_capture();
  test_interrupt_mask();
  return 0;
}
