#include "avp/runtime/memory/allocator/runtime_allocator.h"
#include "avp/runtime/support/runtime_diagnostics.h"
#include "avp/runtime/support/runtime_log.h"

#include <assert.h>
#include <setjmp.h>
#include <stdint.h>


static jmp_buf panic_jump;

_Noreturn void pvpanic_trigger(void) {
    longjmp(panic_jump, 1);
}

static void test_log_function(
    uint32_t level,
    const char *format,
    ...
) {
    (void)level;
    (void)format;
}

typedef void (*PanicThunk)(uint32_t, const char *);

static void verify_panic_thunk(
    PanicThunk thunk,
    RuntimeFirebloomPanicClass expected_class
) {
    static const char message[] = "bounded pointer violation";
    if (setjmp(panic_jump) == 0) {
        thunk(0x456, message);
    }
    const RuntimeFatalRecord record = runtime_diagnostics_last_record();
    assert(record.reason == RUNTIME_FATAL_FIREBLOOM);
    assert(record.source_token == expected_class);
    assert(record.line == 0x456);
    assert(record.message == (uintptr_t)message);
    assert(record.caller != 0);
}

int main(void) {
    RuntimeAllocatorRoute primary;
    RuntimeAllocatorRoute fallback;
    runtime_allocator_publish_routes((RuntimeAllocatorRoutes){
        .primary = &primary,
        .fallback = &fallback,
    });

    RuntimeAllocatorRoute *copied_primary = 0;
    runtime_allocator_copy_routes(&copied_primary, 0);
    assert(copied_primary == &primary);
    RuntimeAllocatorRoute *copied_fallback = 0;
    runtime_allocator_copy_routes(0, &copied_fallback);
    assert(copied_fallback == &fallback);
    assert(runtime_allocator_select_route(true) == &primary);
    assert(runtime_allocator_select_route(false) == &fallback);
    assert(runtime_allocator_select_available_route() == &primary);

    runtime_allocator_publish_routes((RuntimeAllocatorRoutes){
        .primary = 0,
        .fallback = &fallback,
    });
    assert(runtime_allocator_select_available_route() == &fallback);

    assert(runtime_log_get_function() == 0);
    assert(runtime_log_set_function(test_log_function) == test_log_function);
    assert(runtime_log_get_function() == test_log_function);
    assert(runtime_log_set_function(0) == 0);
    assert(runtime_log_get_function() == 0);

    verify_panic_thunk(
        runtime_firebloom_panic_thunk_direct,
        RUNTIME_FIREBLOOM_PANIC_DIRECT
    );
    verify_panic_thunk(
        runtime_firebloom_panic_thunk_spill_a,
        RUNTIME_FIREBLOOM_PANIC_SPILL_A
    );
    verify_panic_thunk(
        runtime_firebloom_panic_thunk_spill_b,
        RUNTIME_FIREBLOOM_PANIC_SPILL_B
    );
}
