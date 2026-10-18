#pragma once

#include "avp/runtime/task/runtime_task.h"

#include <stdint.h>


#define RUNTIME_EXCEPTION_DISPATCH_PUBLIC __attribute__((used, retain))

typedef enum {
    RUNTIME_EXCEPTION_CURRENT_SYNC = 0,
    RUNTIME_EXCEPTION_CURRENT_IRQ = 1,
    RUNTIME_EXCEPTION_CURRENT_FIQ = 2,
    RUNTIME_EXCEPTION_CURRENT_SERROR = 3,
    RUNTIME_EXCEPTION_LOWER_SYNC = 4,
    RUNTIME_EXCEPTION_LOWER_SERROR = 5,
} RuntimeExceptionRecoveredKind;

typedef struct {
    uint64_t (*read_esr_el1)(void);
    uint64_t (*read_far_el1)(void);
    uint64_t (*strip_pointer_auth)(uint64_t address);
    void (*fatal)(
        RuntimeExceptionRecoveredKind kind,
        RuntimeTaskFrame *frame,
        uint64_t source_token,
        uint32_t source_line
    );
    void (*breakpoint)(RuntimeTaskFrame *frame);
} RuntimeExceptionRecoveredPlatform;

void runtime_exception_dispatch_recovered_bind_platform(
    const RuntimeExceptionRecoveredPlatform *platform
);

RUNTIME_EXCEPTION_DISPATCH_PUBLIC void
runtime_breakpoint_exception_hook_noop(RuntimeTaskFrame *frame);
RUNTIME_EXCEPTION_DISPATCH_PUBLIC _Noreturn void
runtime_current_el_sync_exception_dispatch(RuntimeTaskFrame *frame);
RUNTIME_EXCEPTION_DISPATCH_PUBLIC _Noreturn void
runtime_unhandled_exception_fatal(RuntimeTaskFrame *frame);
RUNTIME_EXCEPTION_DISPATCH_PUBLIC _Noreturn void
runtime_current_el_irq_exception_fatal(RuntimeTaskFrame *frame);
RUNTIME_EXCEPTION_DISPATCH_PUBLIC _Noreturn void
runtime_current_el_fiq_exception_fatal(RuntimeTaskFrame *frame);
RUNTIME_EXCEPTION_DISPATCH_PUBLIC _Noreturn void
runtime_current_el_serror_exception_dispatch(RuntimeTaskFrame *frame);
RUNTIME_EXCEPTION_DISPATCH_PUBLIC _Noreturn void
runtime_lower_el_sync_exception_dispatch(RuntimeTaskFrame *frame);
RUNTIME_EXCEPTION_DISPATCH_PUBLIC _Noreturn void
runtime_lower_el_serror_exception_dispatch(RuntimeTaskFrame *frame);

RUNTIME_EXCEPTION_DISPATCH_PUBLIC _Noreturn void runtime_halt_forever(void);
RUNTIME_EXCEPTION_DISPATCH_PUBLIC void
runtime_vector_current_el_sp0_sync_unsupported(void);
RUNTIME_EXCEPTION_DISPATCH_PUBLIC void
runtime_vector_current_el_sp0_irq_unsupported(void);
RUNTIME_EXCEPTION_DISPATCH_PUBLIC void
runtime_vector_current_el_sp0_fiq_unsupported(void);
RUNTIME_EXCEPTION_DISPATCH_PUBLIC void
runtime_vector_current_el_sp0_serror_unsupported(void);
RUNTIME_EXCEPTION_DISPATCH_PUBLIC void runtime_vector_current_el_spx_sync(void);
RUNTIME_EXCEPTION_DISPATCH_PUBLIC void runtime_vector_current_el_spx_irq(void);
RUNTIME_EXCEPTION_DISPATCH_PUBLIC void runtime_vector_current_el_spx_fiq(void);
RUNTIME_EXCEPTION_DISPATCH_PUBLIC void
runtime_vector_current_el_spx_serror(void);
RUNTIME_EXCEPTION_DISPATCH_PUBLIC void
runtime_vector_lower_el_aarch64_sync(void);
RUNTIME_EXCEPTION_DISPATCH_PUBLIC void
runtime_vector_lower_el_aarch64_irq(void);
RUNTIME_EXCEPTION_DISPATCH_PUBLIC void
runtime_vector_lower_el_aarch64_fiq(void);
RUNTIME_EXCEPTION_DISPATCH_PUBLIC void
runtime_vector_lower_el_aarch64_serror(void);
RUNTIME_EXCEPTION_DISPATCH_PUBLIC void
runtime_vector_lower_el_aarch32_sync_unsupported(void);
RUNTIME_EXCEPTION_DISPATCH_PUBLIC void
runtime_vector_lower_el_aarch32_irq_unsupported(void);
RUNTIME_EXCEPTION_DISPATCH_PUBLIC void
runtime_vector_lower_el_aarch32_fiq_unsupported(void);
RUNTIME_EXCEPTION_DISPATCH_PUBLIC void
runtime_vector_lower_el_aarch32_serror_unsupported(void);
