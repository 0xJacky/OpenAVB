#pragma once

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#if __has_attribute(retain)
#define RUNTIME_FIREBLOOM_PANIC_RECOVERED_ENTRY __attribute__((used, retain))
#else
#define RUNTIME_FIREBLOOM_PANIC_RECOVERED_ENTRY __attribute__((used))
#endif

typedef enum {
  RUNTIME_FIREBLOOM_PANIC_PLAIN = 0x00,
  RUNTIME_FIREBLOOM_PANIC_SPILL_C = 0x13,
  RUNTIME_FIREBLOOM_PANIC_SPILL_D = 0x14,
  RUNTIME_FIREBLOOM_PANIC_SPILL_E = 0x15,
  RUNTIME_FIREBLOOM_PANIC_SPILL_F = 0x16,
  RUNTIME_FIREBLOOM_PANIC_SPILL_G = 0x17,
} RuntimeFirebloomRecoveredPanicClass;

typedef struct {
  uint64_t x9;
  uint64_t x10;
  uint64_t x11;
  uint64_t x12;
  uint64_t x13;
  uint64_t x14;
  uint64_t x15;
  uint64_t reserved;
  uintptr_t return_address;
  uintptr_t frame_pointer;
} RuntimeFirebloomScratchFrame;

_Static_assert(sizeof(RuntimeFirebloomScratchFrame) == 0x50,
               "Firebloom panic scratch frame must retain its 0x50-byte ABI");

typedef void (*RuntimeFirebloomRecoveredFailureHook)(void *context);
typedef void (*RuntimeFirebloomRecoveredPanicSink)(
  uint8_t panic_class,
  intptr_t callsite_offset,
  const char *message,
  void *context
);

typedef struct {
  RuntimeFirebloomRecoveredFailureHook failure_hook;
  RuntimeFirebloomRecoveredPanicSink panic_sink;
  void *context;
} RuntimeFirebloomRecoveredPanicConfiguration;

void runtime_firebloom_recovered_panic_configure(
    const RuntimeFirebloomRecoveredPanicConfiguration *configuration);

RUNTIME_FIREBLOOM_PANIC_RECOVERED_ENTRY _Noreturn void
runtime_firebloom_panic_format(const char *message, intptr_t callsite_offset);
RUNTIME_FIREBLOOM_PANIC_RECOVERED_ENTRY _Noreturn void
runtime_firebloom_panic_format_variadic(const char *format, ...)
    __attribute__((format(printf, 1, 2)));
RUNTIME_FIREBLOOM_PANIC_RECOVERED_ENTRY _Noreturn void
runtime_firebloom_panic_thunk_plain(void);
RUNTIME_FIREBLOOM_PANIC_RECOVERED_ENTRY _Noreturn void
runtime_firebloom_panic_thunk_spill_c(void);
RUNTIME_FIREBLOOM_PANIC_RECOVERED_ENTRY _Noreturn void
runtime_firebloom_panic_thunk_spill_d(void);
RUNTIME_FIREBLOOM_PANIC_RECOVERED_ENTRY _Noreturn void
runtime_firebloom_panic_thunk_spill_e(void);
RUNTIME_FIREBLOOM_PANIC_RECOVERED_ENTRY _Noreturn void
runtime_firebloom_panic_thunk_spill_f(void);
RUNTIME_FIREBLOOM_PANIC_RECOVERED_ENTRY _Noreturn void
runtime_firebloom_panic_thunk_spill_g(void);
RUNTIME_FIREBLOOM_PANIC_RECOVERED_ENTRY _Noreturn void
runtime_firebloom_invoke_failure_hook_and_panic(void);
RUNTIME_FIREBLOOM_PANIC_RECOVERED_ENTRY intptr_t
runtime_firebloom_callsite_offset(
  uintptr_t callsite,
  uintptr_t formatter_address
);
RUNTIME_FIREBLOOM_PANIC_RECOVERED_ENTRY RuntimeFirebloomScratchFrame
runtime_firebloom_spill_scratch_registers(
  const uint64_t scratch_registers[7],
  uintptr_t return_address,
  uintptr_t frame_pointer
);
