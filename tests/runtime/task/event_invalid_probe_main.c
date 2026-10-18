#include "avp/platform/platform.h"
#include "avp/runtime/exception/pvpanic.h"
#include "avp/runtime/task/runtime_event.h"
#include "avp/runtime/ipc/runtime_svc.h"
#include "avp/runtime/task/runtime_task.h"


enum {
    EVENT_INVALID_PROBE_STAGE = 0x45564e49,
    EVENT_INVALID_TASK_IPA = 0x70010000,
    EVENT_INVALID_STACK_TOP = 0x70100000,
};

static uint16_t invalid_event;

static void masked_waiter(void *context) {
    (void)context;
    platform_progress(0, 1);
    if (runtime_task_mask_interrupts() != 0) {
        pvpanic_trigger();
    }
    platform_progress(0, 2);
    (void)runtime_event_wait(invalid_event, UINT64_MAX);
    platform_progress(0, 3);
    pvpanic_trigger();
}

void firmware_main(void) {
    pvpanic_initialize();
    runtime_scheduler_initialize();
    platform_stage(EVENT_INVALID_PROBE_STAGE);
    invalid_event = runtime_event_create(true, false);
    RuntimeTask *task = (RuntimeTask *)(uintptr_t)EVENT_INVALID_TASK_IPA;
    if (invalid_event == 0 ||
        !runtime_task_register(
            task,
            runtime_domain_lookup(AVP_RUNTIME_DOMAIN_BOOT),
            EVENT_INVALID_STACK_TOP,
            masked_waiter,
            0
        ) || !runtime_task_activate(task)) {
        pvpanic_trigger();
    }
    runtime_task_yield();
    pvpanic_trigger();
}
