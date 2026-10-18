#include "avp/runtime/support/runtime_diagnostics.h"

#include <assert.h>


_Noreturn void pvpanic_trigger(void) {
    __builtin_trap();
}

int main(void) {
    assert(
        runtime_allocator_source_token() ==
        UINT64_C(0x003f12d84a8f73dca)
    );
    assert(
        runtime_address_space_source_token() ==
        UINT64_C(0x01e87eca99c7dc29)
    );

    runtime_diagnostics_record(
        RUNTIME_FATAL_SOURCE_LOCATION,
        runtime_allocator_source_token(),
        0x484,
        UINT64_C(0x123456789abcdef0)
    );
    const RuntimeFatalRecord source_record =
        runtime_diagnostics_last_record();
    assert(source_record.reason == RUNTIME_FATAL_SOURCE_LOCATION);
    assert(
        source_record.source_token == runtime_allocator_source_token()
    );
    assert(source_record.line == 0x484);
    assert(source_record.caller == UINT64_C(0x123456789abcdef0));

    runtime_diagnostics_record(
        RUNTIME_FATAL_STACK_GUARD,
        0,
        0,
        UINT64_C(0x100000)
    );
    const RuntimeFatalRecord guard_record =
        runtime_diagnostics_last_record();
    assert(guard_record.reason == RUNTIME_FATAL_STACK_GUARD);
    assert(guard_record.source_token == 0);
    assert(guard_record.line == 0);
    assert(guard_record.caller == UINT64_C(0x100000));
}
