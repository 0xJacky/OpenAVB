#include "avp/runtime/exception/runtime_exception_dispatch_recovered.h"

#include <assert.h>
#include <setjmp.h>
#include <string.h>


static jmp_buf fatal_jump;
static RuntimeExceptionRecoveredKind observed_kind;
static uint64_t observed_token;
static uint32_t observed_line;
static unsigned breakpoint_count;
static uint64_t observed_elr;

static uint64_t read_esr(void) {
    return UINT64_C(0xf0000042);
}

static uint64_t read_far(void) {
    return UINT64_C(0x1122334455667788);
}

static uint64_t strip_pointer_auth(uint64_t address) {
    return address & UINT64_C(0x0000ffffffffffff);
}

static void fatal_callback(
    RuntimeExceptionRecoveredKind kind,
    RuntimeTaskFrame *frame,
    uint64_t token,
    uint32_t line
) {
    assert(frame != 0);
    observed_kind = kind;
    observed_token = token;
    observed_line = line;
    observed_elr = frame->signed_elr_el1;
    longjmp(fatal_jump, 1);
}

static void breakpoint_callback(RuntimeTaskFrame *frame) {
    assert(frame != 0);
    ++breakpoint_count;
}

static void expect_dispatch(
    void (*dispatch)(RuntimeTaskFrame *),
    RuntimeExceptionRecoveredKind kind,
    uint32_t line
) {
    RuntimeTaskFrame frame;
    memset(&frame, 0, sizeof(frame));
    if (setjmp(fatal_jump) == 0) {
        dispatch(&frame);
        assert(false);
    }
    assert(observed_kind == kind);
    assert(observed_token == UINT64_C(0x04962211b03d706d));
    assert(observed_line == line);
}

int main(void) {
    const RuntimeExceptionRecoveredPlatform platform = {
        .read_esr_el1 = read_esr,
        .read_far_el1 = read_far,
        .strip_pointer_auth = strip_pointer_auth,
        .fatal = fatal_callback,
        .breakpoint = breakpoint_callback,
    };
    runtime_exception_dispatch_recovered_bind_platform(&platform);

    RuntimeTaskFrame frame;
    memset(&frame, 0, sizeof(frame));
    frame.signed_elr_el1 = UINT64_C(0xabcd000012345678);
    if (setjmp(fatal_jump) == 0) {
        runtime_unhandled_exception_fatal(&frame);
        assert(false);
    }
    assert(observed_kind == RUNTIME_EXCEPTION_CURRENT_SYNC);
    assert(observed_line == 0xcb);
    assert(observed_elr == UINT64_C(0x0000000012345678));
    assert(breakpoint_count == 1);

    expect_dispatch(
        runtime_current_el_sync_exception_dispatch,
        RUNTIME_EXCEPTION_CURRENT_SYNC,
        0xcb
    );
    expect_dispatch(
        runtime_current_el_irq_exception_fatal,
        RUNTIME_EXCEPTION_CURRENT_IRQ,
        0xdc
    );
    expect_dispatch(
        runtime_current_el_fiq_exception_fatal,
        RUNTIME_EXCEPTION_CURRENT_FIQ,
        0xe3
    );
    expect_dispatch(
        runtime_current_el_serror_exception_dispatch,
        RUNTIME_EXCEPTION_CURRENT_SERROR,
        0xea
    );
    expect_dispatch(
        runtime_lower_el_sync_exception_dispatch,
        RUNTIME_EXCEPTION_LOWER_SYNC,
        0xf1
    );
    expect_dispatch(
        runtime_lower_el_serror_exception_dispatch,
        RUNTIME_EXCEPTION_LOWER_SERROR,
        0xf8
    );

    runtime_breakpoint_exception_hook_noop(&frame);
    assert(breakpoint_count == 3);
    return 0;
}
