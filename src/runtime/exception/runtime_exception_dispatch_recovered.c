#include "avp/runtime/exception/runtime_exception_dispatch_recovered.h"


#define RUNTIME_EXCEPTION_SOURCE_TOKEN UINT64_C(0x04962211b03d706d)
#define RUNTIME_EXCEPTION_BREAKPOINT_CLASS UINT64_C(0x3c)

static RuntimeExceptionRecoveredPlatform recovered_platform;

static _Noreturn void exception_contract_fail(void) {
    __builtin_trap();
}

void runtime_exception_dispatch_recovered_bind_platform(
    const RuntimeExceptionRecoveredPlatform *platform
) {
    recovered_platform = platform == 0
        ? (RuntimeExceptionRecoveredPlatform){0}
        : *platform;
}

void runtime_breakpoint_exception_hook_noop(RuntimeTaskFrame *frame) {
    if (recovered_platform.breakpoint != 0) {
        recovered_platform.breakpoint(frame);
    }
}

static uint64_t read_esr(void) {
#if defined(__aarch64__) && !defined(__APPLE__) && !defined(__linux__)
    uint64_t value = 0;
    __asm__ volatile("mrs %0, esr_el1" : "=r"(value));
    return value;
#else
    return recovered_platform.read_esr_el1 == 0
        ? 0
        : recovered_platform.read_esr_el1();
#endif
}

static uint64_t read_far(void) {
#if defined(__aarch64__) && !defined(__APPLE__) && !defined(__linux__)
    uint64_t value = 0;
    __asm__ volatile("mrs %0, far_el1" : "=r"(value));
    return value;
#else
    return recovered_platform.read_far_el1 == 0
        ? 0
        : recovered_platform.read_far_el1();
#endif
}

static _Noreturn void publish_fatal(
    RuntimeExceptionRecoveredKind kind,
    RuntimeTaskFrame *frame,
    uint32_t source_line
) {
    if (frame == 0) {
        exception_contract_fail();
    }
    frame->reserved_348[8] = (uint8_t)kind;
    if (recovered_platform.fatal != 0) {
        recovered_platform.fatal(
            kind,
            frame,
            RUNTIME_EXCEPTION_SOURCE_TOKEN,
            source_line
        );
    }
    exception_contract_fail();
}

_Noreturn void runtime_unhandled_exception_fatal(RuntimeTaskFrame *frame) {
    if (frame == 0) {
        exception_contract_fail();
    }
    const uint64_t esr = read_esr();
    frame->reserved_348[0] = (uint8_t)esr;
    frame->reserved_348[1] = (uint8_t)(esr >> 8);
    frame->reserved_348[2] = (uint8_t)(esr >> 16);
    frame->reserved_348[3] = (uint8_t)(esr >> 24);
    const uint64_t far = read_far();
    for (size_t index = 0; index < sizeof(far); ++index) {
        frame->reserved_348[8 + index] = (uint8_t)(far >> (index * 8));
    }
    if ((esr >> 26) == RUNTIME_EXCEPTION_BREAKPOINT_CLASS) {
        runtime_breakpoint_exception_hook_noop(frame);
    }
    if (recovered_platform.strip_pointer_auth != 0) {
        frame->signed_elr_el1 = recovered_platform.strip_pointer_auth(
            frame->signed_elr_el1
        );
    }
    publish_fatal(RUNTIME_EXCEPTION_CURRENT_SYNC, frame, 0xcb);
}

_Noreturn void runtime_current_el_sync_exception_dispatch(
    RuntimeTaskFrame *frame
) {
    runtime_unhandled_exception_fatal(frame);
}

_Noreturn void runtime_current_el_irq_exception_fatal(
    RuntimeTaskFrame *frame
) {
    publish_fatal(RUNTIME_EXCEPTION_CURRENT_IRQ, frame, 0xdc);
}

_Noreturn void runtime_current_el_fiq_exception_fatal(
    RuntimeTaskFrame *frame
) {
    publish_fatal(RUNTIME_EXCEPTION_CURRENT_FIQ, frame, 0xe3);
}

_Noreturn void runtime_current_el_serror_exception_dispatch(
    RuntimeTaskFrame *frame
) {
    publish_fatal(RUNTIME_EXCEPTION_CURRENT_SERROR, frame, 0xea);
}

_Noreturn void runtime_lower_el_sync_exception_dispatch(
    RuntimeTaskFrame *frame
) {
    publish_fatal(RUNTIME_EXCEPTION_LOWER_SYNC, frame, 0xf1);
}

_Noreturn void runtime_lower_el_serror_exception_dispatch(
    RuntimeTaskFrame *frame
) {
    publish_fatal(RUNTIME_EXCEPTION_LOWER_SERROR, frame, 0xf8);
}
