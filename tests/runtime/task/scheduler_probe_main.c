#include "avp/platform/platform.h"
#include "avp/runtime/exception/pvpanic.h"
#include "avp/runtime/ipc/runtime_svc.h"
#include "avp/runtime/task/runtime_task.h"


enum {
    SCHEDULER_PROBE_STAGE = 0x53434844,
    SCHEDULER_TASK_A_IPA = 0x70010000,
    SCHEDULER_TASK_B_IPA = 0x70011000,
    SCHEDULER_TASK_A_STACK_TOP = 0x70030000,
    SCHEDULER_TASK_B_STACK_TOP = 0x70040000,
};

static RuntimeDomain domain_a;
static RuntimeDomain domain_b;
static volatile uint32_t trace;
static volatile uint32_t interrupt_mask_trace;

static void scheduler_task(void *context) {
    const uint32_t token = (uintptr_t)context;
    if (runtime_current_domain() != token) {
        pvpanic_trigger();
    }
    if (token == 2) {
        interrupt_mask_trace =
            (uint32_t)runtime_task_mask_interrupts();
        interrupt_mask_trace = interrupt_mask_trace << 8 |
            (uint32_t)runtime_task_mask_interrupts();
    }
    trace = trace << 8 | (0xa0 | token);
    runtime_task_yield();
    if (runtime_current_domain() != token) {
        pvpanic_trigger();
    }
    if (token == 2) {
        interrupt_mask_trace = interrupt_mask_trace << 8 |
            (uint32_t)runtime_task_unmask_interrupts();
        interrupt_mask_trace = interrupt_mask_trace << 8 |
            (uint32_t)runtime_task_unmask_interrupts();
    }
    trace = trace << 8 | (0xb0 | token);
    runtime_task_yield();
    for (;;) {
        runtime_task_yield();
    }
}

void firmware_main(void) {
    pvpanic_initialize();
    runtime_scheduler_initialize();
    platform_stage(SCHEDULER_PROBE_STAGE);
    if (!runtime_domain_register(&domain_a, 2, AVP_RUNTIME_DOMAIN_BOOT, 2) ||
        !runtime_domain_register(&domain_b, 3, AVP_RUNTIME_DOMAIN_BOOT, 3)) {
        pvpanic_trigger();
    }

    RuntimeTask *task_a = (RuntimeTask *)SCHEDULER_TASK_A_IPA;
    RuntimeTask *task_b = (RuntimeTask *)SCHEDULER_TASK_B_IPA;
    if (!runtime_task_register(
            task_a,
            &domain_a,
            SCHEDULER_TASK_A_STACK_TOP,
            scheduler_task,
            (void *)2
        ) ||
        !runtime_task_register(
            task_b,
            &domain_b,
            SCHEDULER_TASK_B_STACK_TOP,
            scheduler_task,
            (void *)3
        )) {
        pvpanic_trigger();
    }
    runtime_task_activate_capability(
        runtime_task_capability_handle(task_a),
        UINT64_C(0x1122334455667788)
    );
    runtime_task_activate_capability(
        runtime_task_capability_handle(task_b),
        UINT64_C(0x8877665544332211)
    );
    if (task_a->state != RUNTIME_TASK_STATE_READY ||
        task_b->state != RUNTIME_TASK_STATE_READY ||
        task_a->frame.stack_guard != UINT64_C(0x1122334455660088) ||
        task_b->frame.stack_guard != UINT64_C(0x8877665544330011) ||
        runtime_task_ready_count() != 2 ||
        runtime_current_domain() != AVP_RUNTIME_DOMAIN_BOOT) {
        pvpanic_trigger();
    }

    runtime_task_yield();
    runtime_task_yield();
    platform_progress(0, trace);
    platform_progress(1, runtime_task_switch_count());
    platform_progress(2, runtime_task_ready_count());
    platform_progress(3, runtime_current_domain());
    if (trace != UINT32_C(0xa2a3b2b3) ||
        runtime_task_switch_count() != 6 ||
        runtime_task_ready_count() != 2 ||
        interrupt_mask_trace != UINT32_C(0x00c0c000) ||
        runtime_current_domain() != AVP_RUNTIME_DOMAIN_BOOT) {
        pvpanic_trigger();
    }
    pvpanic_trigger();
}
