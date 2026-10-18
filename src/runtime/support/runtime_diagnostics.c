#include "avp/runtime/exception/pvpanic.h"
#include "avp/runtime/support/recovered_symbol.h"
#include "avp/runtime/support/runtime_diagnostics.h"


static const uint64_t runtime_allocator_token =
    UINT64_C(0x003f12d84a8f73dca);
static const uint64_t runtime_address_space_token =
    UINT64_C(0x01e87eca99c7dc29);

static volatile RuntimeFatalRecord last_fatal_record;

AVP_RECOVERED_ENTRY uint64_t runtime_allocator_source_token(void) {
    return runtime_allocator_token;
}

uint64_t runtime_address_space_source_token(void) {
    return runtime_address_space_token;
}

void runtime_diagnostics_record(
    RuntimeFatalReason reason,
    uint64_t source_token,
    uint32_t line,
    uintptr_t caller
) {
    last_fatal_record = (RuntimeFatalRecord){
        .source_token = source_token,
        .caller = caller,
        .message = 0,
        .line = line,
        .reason = reason,
    };
}

RuntimeFatalRecord runtime_diagnostics_last_record(void) {
    return last_fatal_record;
}

void runtime_diagnostics_record_firebloom(
    RuntimeFirebloomPanicClass panic_class,
    uint32_t detail,
    const char *message,
    uintptr_t caller
) {
    last_fatal_record = (RuntimeFatalRecord){
        .source_token = (uint64_t)panic_class,
        .caller = caller,
        .message = (uintptr_t)message,
        .line = detail,
        .reason = RUNTIME_FATAL_FIREBLOOM,
    };
}

_Noreturn __attribute__((no_stack_protector))
static void runtime_firebloom_panic_thunk(
    RuntimeFirebloomPanicClass panic_class,
    uint32_t detail,
    const char *message,
    uintptr_t caller
) {
    runtime_diagnostics_record_firebloom(
        panic_class,
        detail,
        message,
        caller
    );
    pvpanic_trigger();
}

#if defined(__ELF__)
#define RUNTIME_FIREBLOOM_DIAGNOSTIC_RECOVERED \
    __attribute__((noinline, section(".text.runtime_support_recovered")))
#else
#define RUNTIME_FIREBLOOM_DIAGNOSTIC_RECOVERED __attribute__((noinline))
#endif

_Noreturn RUNTIME_FIREBLOOM_DIAGNOSTIC_RECOVERED
__attribute__((no_stack_protector))
void runtime_firebloom_panic_thunk_direct(
    uint32_t detail,
    const char *message
) {
    runtime_firebloom_panic_thunk(
        RUNTIME_FIREBLOOM_PANIC_DIRECT,
        detail,
        message,
        (uintptr_t)__builtin_return_address(0)
    );
}

_Noreturn RUNTIME_FIREBLOOM_DIAGNOSTIC_RECOVERED
__attribute__((no_stack_protector))
void runtime_firebloom_panic_thunk_spill_a(
    uint32_t detail,
    const char *message
) {
    runtime_firebloom_panic_thunk(
        RUNTIME_FIREBLOOM_PANIC_SPILL_A,
        detail,
        message,
        (uintptr_t)__builtin_return_address(0)
    );
}

_Noreturn RUNTIME_FIREBLOOM_DIAGNOSTIC_RECOVERED
__attribute__((no_stack_protector))
void runtime_firebloom_panic_thunk_spill_b(
    uint32_t detail,
    const char *message
) {
    runtime_firebloom_panic_thunk(
        RUNTIME_FIREBLOOM_PANIC_SPILL_B,
        detail,
        message,
        (uintptr_t)__builtin_return_address(0)
    );
}

_Noreturn __attribute__((no_stack_protector))
void runtime_fatal_format(
    RuntimeFatalReason reason,
    uint64_t source_token,
    uint32_t line,
    uintptr_t caller
) {
    runtime_diagnostics_record(reason, source_token, line, caller);
    pvpanic_trigger();
}

_Noreturn __attribute__((no_stack_protector))
void runtime_fatal(void) {
    runtime_fatal_format(
        RUNTIME_FATAL_EMPTY,
        0,
        0,
        (uintptr_t)__builtin_return_address(0)
    );
}

_Noreturn __attribute__((no_stack_protector))
AVP_RECOVERED_ENTRY void runtime_fatal_at(
    uint64_t source_token,
    uint32_t line
) {
    runtime_fatal_format(
        RUNTIME_FATAL_SOURCE_LOCATION,
        source_token,
        line,
        (uintptr_t)__builtin_return_address(0)
    );
}

_Noreturn __attribute__((no_stack_protector))
void runtime_stack_guard_fail(void) {
    runtime_fatal_format(
        RUNTIME_FATAL_STACK_GUARD,
        0,
        0,
        (uintptr_t)__builtin_return_address(0)
    );
}
