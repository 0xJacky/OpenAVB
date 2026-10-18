#include "avp/runtime/firebloom/runtime_firebloom_panic_recovered.h"

#include "avp/runtime/support/runtime_core_recovered.h"

#include <stdbool.h>

static RuntimeFirebloomRecoveredPanicConfiguration panic_configuration;

static _Noreturn void runtime_firebloom_recovered_trap(void) {
  __builtin_trap();
  __builtin_unreachable();
}

static _Noreturn void runtime_firebloom_emit(
    uint8_t panic_class,
    intptr_t callsite_offset,
    const char *message
) {
  if (panic_configuration.panic_sink != NULL) {
    panic_configuration.panic_sink(
        panic_class,
        callsite_offset,
        message,
        panic_configuration.context
    );
  }
  runtime_firebloom_panic_format(message, callsite_offset);
}

static _Noreturn void runtime_firebloom_emit_from_return_address(
    uint8_t panic_class,
    uintptr_t callsite,
    uintptr_t veneer_address
) {
  const intptr_t offset =
      runtime_firebloom_callsite_offset(callsite, veneer_address);

  runtime_firebloom_emit(panic_class, offset, "panic");
}

void runtime_firebloom_recovered_panic_configure(
    const RuntimeFirebloomRecoveredPanicConfiguration *configuration) {
  if (configuration == NULL) {
    runtime_firebloom_recovered_trap();
  }
  panic_configuration = *configuration;
}

_Noreturn void RUNTIME_FIREBLOOM_PANIC_RECOVERED_ENTRY
runtime_firebloom_panic_format(const char *message, intptr_t callsite_offset) {
  const bool negative = callsite_offset < 0;
  const uintptr_t magnitude = negative ? (uintptr_t)(-(callsite_offset + 1)) + 1
                                       : (uintptr_t)callsite_offset;

  runtime_firebloom_panic_format_variadic(
      "[panic%c%#lx] %s\n",
      negative ? '-' : '+',
      (unsigned long)magnitude,
      message
  );
}

_Noreturn void RUNTIME_FIREBLOOM_PANIC_RECOVERED_ENTRY
runtime_firebloom_panic_format_variadic(const char *format, ...) {
  va_list arguments;

  va_start(arguments, format);
  runtime_fatal_vformat("__firebloom_panic", format, false, arguments);
  va_end(arguments);
  __builtin_unreachable();
}

_Noreturn void RUNTIME_FIREBLOOM_PANIC_RECOVERED_ENTRY
runtime_firebloom_panic_thunk_plain(void) {
  const uintptr_t callsite = (uintptr_t)__builtin_return_address(0);

  runtime_firebloom_emit_from_return_address(
      RUNTIME_FIREBLOOM_PANIC_PLAIN,
      callsite,
      (uintptr_t)runtime_firebloom_panic_thunk_plain
  );
}

#define RUNTIME_FIREBLOOM_DEFINE_SPILL_THUNK(name, panic_class)                \
  _Noreturn void RUNTIME_FIREBLOOM_PANIC_RECOVERED_ENTRY name(void) {          \
    const uintptr_t callsite = (uintptr_t)__builtin_return_address(0);         \
    runtime_firebloom_emit_from_return_address((panic_class), callsite,        \
                                               (uintptr_t)(name));             \
  }

RUNTIME_FIREBLOOM_DEFINE_SPILL_THUNK(
    runtime_firebloom_panic_thunk_spill_c,
    RUNTIME_FIREBLOOM_PANIC_SPILL_C
)
RUNTIME_FIREBLOOM_DEFINE_SPILL_THUNK(
    runtime_firebloom_panic_thunk_spill_d,
    RUNTIME_FIREBLOOM_PANIC_SPILL_D
)
RUNTIME_FIREBLOOM_DEFINE_SPILL_THUNK(
    runtime_firebloom_panic_thunk_spill_e,
    RUNTIME_FIREBLOOM_PANIC_SPILL_E
)
RUNTIME_FIREBLOOM_DEFINE_SPILL_THUNK(
    runtime_firebloom_panic_thunk_spill_f,
    RUNTIME_FIREBLOOM_PANIC_SPILL_F
)
RUNTIME_FIREBLOOM_DEFINE_SPILL_THUNK(
    runtime_firebloom_panic_thunk_spill_g,
    RUNTIME_FIREBLOOM_PANIC_SPILL_G
)

_Noreturn void RUNTIME_FIREBLOOM_PANIC_RECOVERED_ENTRY
runtime_firebloom_invoke_failure_hook_and_panic(void) {
  if (panic_configuration.failure_hook != NULL) {
    panic_configuration.failure_hook(panic_configuration.context);
  }
  runtime_firebloom_panic_thunk_plain();
}

intptr_t RUNTIME_FIREBLOOM_PANIC_RECOVERED_ENTRY
runtime_firebloom_callsite_offset(
    uintptr_t callsite,
    uintptr_t formatter_address
) {
  if (callsite >= formatter_address) {
    const uintptr_t difference = callsite - formatter_address;
    if (difference > (uintptr_t)INTPTR_MAX) {
      runtime_firebloom_recovered_trap();
    }
    return (intptr_t)difference;
  }

  const uintptr_t difference = formatter_address - callsite;
  if (difference > (uintptr_t)INTPTR_MAX + 1) {
    runtime_firebloom_recovered_trap();
  }
  if (difference == (uintptr_t)INTPTR_MAX + 1) {
    return INTPTR_MIN;
  }
  return -(intptr_t)difference;
}

RuntimeFirebloomScratchFrame RUNTIME_FIREBLOOM_PANIC_RECOVERED_ENTRY
runtime_firebloom_spill_scratch_registers(
    const uint64_t scratch_registers[7],
    uintptr_t return_address,
    uintptr_t frame_pointer
) {
  if (scratch_registers == NULL) {
    runtime_firebloom_recovered_trap();
  }

  return (RuntimeFirebloomScratchFrame){
      .x9 = scratch_registers[0],
      .x10 = scratch_registers[1],
      .x11 = scratch_registers[2],
      .x12 = scratch_registers[3],
      .x13 = scratch_registers[4],
      .x14 = scratch_registers[5],
      .x15 = scratch_registers[6],
      .reserved = 0,
      .return_address = return_address,
      .frame_pointer = frame_pointer,
  };
}
