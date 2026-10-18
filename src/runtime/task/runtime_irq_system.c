#include "avp/runtime/exception/pvpanic.h"
#include "avp/runtime/task/runtime_irq.h"
#include "avp/runtime/task/runtime_task.h"


static RuntimeIRQRuntime irq_runtime;
static RuntimeIRQHandlerEntry irq_entries[1];
static RuntimeTask *irq_trapped_task;
static RuntimeTask *irq_selected_task;

static void irq_provider_initialize(void *context) {
    (void)context;
    runtime_irq_arch_controller_initialize();
    runtime_irq_arch_virtual_timer_initialize();
}

static uint32_t irq_provider_acknowledge(
    void *context,
    RuntimeIRQGroup group
) {
    (void)context;
    if (group != RUNTIME_IRQ_GROUP0) {
        __builtin_trap();
    }
    return runtime_irq_arch_acknowledge_group0();
}

static void irq_provider_end(
    void *context,
    RuntimeIRQGroup group,
    uint32_t interrupt_id
) {
    (void)context;
    if (group != RUNTIME_IRQ_GROUP0) {
        __builtin_trap();
    }
    runtime_irq_arch_end_group0(interrupt_id);
}

static void irq_provider_set_enabled(
    void *context,
    RuntimeIRQGroup group,
    uint32_t interrupt_id,
    bool enabled
) {
    (void)context;
    if (group != RUNTIME_IRQ_GROUP0 ||
        interrupt_id != RUNTIME_IRQ_VIRTUAL_TIMER_ID) {
        __builtin_trap();
    }
    runtime_irq_arch_set_virtual_timer_enabled(enabled);
}

static uint64_t irq_provider_mask(void *context) {
    (void)context;
    return runtime_irq_arch_mask();
}

static void irq_provider_restore(void *context, uint64_t token) {
    (void)context;
    runtime_irq_arch_restore(token);
}

__attribute__((noreturn))
static void irq_provider_fatal(
    void *context,
    RuntimeIRQFatalReason reason,
    uint32_t interrupt_id
) {
    (void)context;
    (void)reason;
    (void)interrupt_id;
    pvpanic_trigger();
}

static RuntimeIRQHandlerResult virtual_timer_handler(
    uint32_t interrupt_id,
    void *context
) {
    (void)context;
    if (interrupt_id != RUNTIME_IRQ_VIRTUAL_TIMER_ID ||
        irq_trapped_task == 0 || irq_selected_task != 0) {
        __builtin_trap();
    }
    irq_selected_task = runtime_task_timer_fiq_from_exception(
        irq_trapped_task
    );
    return RUNTIME_IRQ_HANDLER_RESCHEDULE;
}

void runtime_irq_system_initialize(void) {
    const RuntimeIRQProvider provider = {
        .initialize = irq_provider_initialize,
        .acknowledge = irq_provider_acknowledge,
        .end = irq_provider_end,
        .set_enabled = irq_provider_set_enabled,
        .mask = irq_provider_mask,
        .restore = irq_provider_restore,
        .fatal = irq_provider_fatal,
    };
    if (runtime_irq_initialize(
            &irq_runtime,
            &provider,
            irq_entries,
            1
        ) != RUNTIME_IRQ_STATUS_OK ||
        runtime_irq_register(
            &irq_runtime,
            RUNTIME_IRQ_GROUP0,
            RUNTIME_IRQ_VIRTUAL_TIMER_ID,
            virtual_timer_handler,
            0
        ) != RUNTIME_IRQ_STATUS_OK ||
        runtime_irq_enable(
            &irq_runtime,
            RUNTIME_IRQ_VIRTUAL_TIMER_ID
        ) != RUNTIME_IRQ_STATUS_OK) {
        __builtin_trap();
    }
}

RuntimeTask *runtime_irq_system_timer_fiq_from_exception(
    RuntimeTask *trapped_task
) {
    if (trapped_task == 0 || irq_trapped_task != 0 ||
        irq_selected_task != 0) {
        __builtin_trap();
    }
    irq_trapped_task = trapped_task;
    const RuntimeIRQDispatchResult result = runtime_irq_dispatch(
        &irq_runtime,
        RUNTIME_IRQ_GROUP0
    );
    RuntimeTask *selected_task = irq_selected_task;
    irq_trapped_task = 0;
    irq_selected_task = 0;
    if (result == RUNTIME_IRQ_DISPATCH_SPURIOUS) {
        return trapped_task;
    }
    if (result != RUNTIME_IRQ_DISPATCH_RESCHEDULE || selected_task == 0) {
        __builtin_trap();
    }
    return selected_task;
}
