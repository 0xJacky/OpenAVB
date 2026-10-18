#pragma once

#include <stdint.h>


typedef enum {
    RUNTIME_FATAL_EMPTY = 0,
    RUNTIME_FATAL_SOURCE_LOCATION = 1,
    RUNTIME_FATAL_STACK_GUARD = 2,
    RUNTIME_FATAL_FIREBLOOM = 3,
} RuntimeFatalReason;

typedef enum {
    RUNTIME_FIREBLOOM_PANIC_DIRECT = 0x10,
    RUNTIME_FIREBLOOM_PANIC_SPILL_A = 0x11,
    RUNTIME_FIREBLOOM_PANIC_SPILL_B = 0x12,
} RuntimeFirebloomPanicClass;

typedef struct {
    uint64_t source_token;
    uintptr_t caller;
    uintptr_t message;
    uint32_t line;
    uint32_t reason;
} RuntimeFatalRecord;

uint64_t runtime_allocator_source_token(void);
uint64_t runtime_address_space_source_token(void);
void runtime_diagnostics_record(
    RuntimeFatalReason reason,
    uint64_t source_token,
    uint32_t line,
    uintptr_t caller
);
RuntimeFatalRecord runtime_diagnostics_last_record(void);
void runtime_diagnostics_record_firebloom(
    RuntimeFirebloomPanicClass panic_class,
    uint32_t detail,
    const char *message,
    uintptr_t caller
);
_Noreturn void runtime_fatal_format(
    RuntimeFatalReason reason,
    uint64_t source_token,
    uint32_t line,
    uintptr_t caller
);
_Noreturn void runtime_fatal(void);
_Noreturn void runtime_fatal_at(uint64_t source_token, uint32_t line);
_Noreturn void runtime_stack_guard_fail(void);
_Noreturn void runtime_firebloom_panic_thunk_direct(
    uint32_t detail,
    const char *message
);
_Noreturn void runtime_firebloom_panic_thunk_spill_a(
    uint32_t detail,
    const char *message
);
_Noreturn void runtime_firebloom_panic_thunk_spill_b(
    uint32_t detail,
    const char *message
);
