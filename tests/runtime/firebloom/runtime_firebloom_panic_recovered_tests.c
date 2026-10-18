#include "avp/runtime/firebloom/runtime_firebloom_panic_recovered.h"

#include <assert.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct {
  uint8_t panic_class;
  intptr_t offset;
  char message[32];
  char source[32];
  char format[64];
  unsigned long magnitude;
  char sign;
  uint32_t hook_count;
  bool bounded_format;
} PanicProbe;

static PanicProbe *active_probe;

static void copy_cstring(char *destination, size_t capacity,
                         const char *source) {
  size_t index = 0;

  assert(capacity != 0);
  while (index + 1 < capacity && source[index] != '\0') {
    destination[index] = source[index];
    ++index;
  }
  destination[index] = '\0';
}

static void failure_hook(void *context) {
  PanicProbe *probe = context;

  ++probe->hook_count;
}

static _Noreturn void panic_sink(uint8_t panic_class, intptr_t offset,
                                 const char *message, void *context) {
  PanicProbe *probe = context;

  probe->panic_class = panic_class;
  probe->offset = offset;
  copy_cstring(probe->message, sizeof(probe->message), message);
  __builtin_trap();
}

_Noreturn void runtime_fatal_vformat(const char *source, const char *format,
                                     bool bounded_format, va_list arguments) {
  copy_cstring(active_probe->source, sizeof(active_probe->source), source);
  copy_cstring(active_probe->format, sizeof(active_probe->format), format);
  active_probe->bounded_format = bounded_format;
  if (strcmp(format, "[panic%c%#lx] %s\n") == 0) {
    active_probe->sign = (char)va_arg(arguments, int);
    active_probe->magnitude = va_arg(arguments, unsigned long);
    copy_cstring(active_probe->message, sizeof(active_probe->message),
                 va_arg(arguments, const char *));
  }
  __builtin_trap();
}

static RuntimeFirebloomRecoveredPanicConfiguration
configuration(PanicProbe *probe) {
  return (RuntimeFirebloomRecoveredPanicConfiguration){
      .failure_hook = failure_hook,
      .panic_sink = panic_sink,
      .context = probe,
  };
}

static void expect_signal(pid_t child) {
  int status;

  assert(waitpid(child, &status, 0) == child);
  assert(WIFSIGNALED(status));
  assert(WTERMSIG(status) == SIGILL || WTERMSIG(status) == SIGTRAP);
}

typedef void (*PanicThunk)(void);

static void invoke_and_verify_class(PanicProbe *probe, PanicThunk thunk,
                                    uint8_t expected_class) {
  pid_t child;

  memset(probe, 0, sizeof(*probe));
  child = fork();
  assert(child >= 0);
  if (child == 0) {
    const RuntimeFirebloomRecoveredPanicConfiguration config =
        configuration(probe);
    runtime_firebloom_recovered_panic_configure(&config);
    thunk();
  }
  expect_signal(child);
  assert(probe->panic_class == expected_class);
  assert(strcmp(probe->message, "panic") == 0);
  assert(probe->offset != 0);
}

static void test_thunk_classes(PanicProbe *probe) {
  invoke_and_verify_class(probe, runtime_firebloom_panic_thunk_plain,
                          RUNTIME_FIREBLOOM_PANIC_PLAIN);
  invoke_and_verify_class(probe, runtime_firebloom_panic_thunk_spill_c,
                          RUNTIME_FIREBLOOM_PANIC_SPILL_C);
  invoke_and_verify_class(probe, runtime_firebloom_panic_thunk_spill_d,
                          RUNTIME_FIREBLOOM_PANIC_SPILL_D);
  invoke_and_verify_class(probe, runtime_firebloom_panic_thunk_spill_e,
                          RUNTIME_FIREBLOOM_PANIC_SPILL_E);
  invoke_and_verify_class(probe, runtime_firebloom_panic_thunk_spill_f,
                          RUNTIME_FIREBLOOM_PANIC_SPILL_F);
  invoke_and_verify_class(probe, runtime_firebloom_panic_thunk_spill_g,
                          RUNTIME_FIREBLOOM_PANIC_SPILL_G);
}

static void test_failure_hook_before_panic(PanicProbe *probe) {
  pid_t child;

  memset(probe, 0, sizeof(*probe));
  child = fork();
  assert(child >= 0);
  if (child == 0) {
    const RuntimeFirebloomRecoveredPanicConfiguration config =
        configuration(probe);
    runtime_firebloom_recovered_panic_configure(&config);
    runtime_firebloom_invoke_failure_hook_and_panic();
  }
  expect_signal(child);
  assert(probe->hook_count == 1);
  assert(probe->panic_class == RUNTIME_FIREBLOOM_PANIC_PLAIN);
}

static void test_callsite_offsets(void) {
  assert(runtime_firebloom_callsite_offset(0x1080, 0x1000) == 0x80);
  assert(runtime_firebloom_callsite_offset(0x0f80, 0x1000) == -0x80);
  assert(runtime_firebloom_callsite_offset(0x1000, 0x1000) == 0);
}

static void test_scratch_frame(void) {
  const uint64_t registers[] = {9, 10, 11, 12, 13, 14, 15};
  const RuntimeFirebloomScratchFrame frame =
      runtime_firebloom_spill_scratch_registers(registers,
                                                UINT64_C(0x123456789abcdef0),
                                                UINT64_C(0x0fedcba987654321));

  assert(frame.x9 == 9);
  assert(frame.x10 == 10);
  assert(frame.x11 == 11);
  assert(frame.x12 == 12);
  assert(frame.x13 == 13);
  assert(frame.x14 == 14);
  assert(frame.x15 == 15);
  assert(frame.reserved == 0);
  assert(frame.return_address == UINT64_C(0x123456789abcdef0));
  assert(frame.frame_pointer == UINT64_C(0x0fedcba987654321));
}

static void test_format_entrypoints_trap(void) {
  PanicProbe *probe = mmap(NULL, sizeof(*probe), PROT_READ | PROT_WRITE,
                           MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  pid_t child;

  assert(probe != MAP_FAILED);
  memset(probe, 0, sizeof(*probe));
  child = fork();
  assert(child >= 0);
  if (child == 0) {
    active_probe = probe;
    runtime_firebloom_panic_format("bounds", -0x20);
  }
  expect_signal(child);
  assert(strcmp(probe->source, "__firebloom_panic") == 0);
  assert(strcmp(probe->format, "[panic%c%#lx] %s\n") == 0);
  assert(strcmp(probe->message, "bounds") == 0);
  assert(probe->sign == '-');
  assert(probe->magnitude == 0x20);
  assert(!probe->bounded_format);

  memset(probe, 0, sizeof(*probe));
  child = fork();
  assert(child >= 0);
  if (child == 0) {
    active_probe = probe;
    runtime_firebloom_panic_format_variadic("failure=%u", 7u);
  }
  expect_signal(child);
  assert(strcmp(probe->source, "__firebloom_panic") == 0);
  assert(strcmp(probe->format, "failure=%u") == 0);
  assert(!probe->bounded_format);
  assert(munmap(probe, sizeof(*probe)) == 0);
}

int main(void) {
  PanicProbe *probe = mmap(NULL, sizeof(*probe), PROT_READ | PROT_WRITE,
                           MAP_SHARED | MAP_ANONYMOUS, -1, 0);

  assert(probe != MAP_FAILED);
  test_thunk_classes(probe);
  test_failure_hook_before_panic(probe);
  test_callsite_offsets();
  test_scratch_frame();
  test_format_entrypoints_trap();
  assert(munmap(probe, sizeof(*probe)) == 0);
  return 0;
}
