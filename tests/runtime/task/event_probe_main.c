#include "avp/platform/platform.h"
#include "avp/runtime/exception/pvpanic.h"
#include "avp/runtime/task/runtime_irq.h"
#include "avp/runtime/task/runtime_event.h"
#include "avp/runtime/ipc/runtime_svc.h"
#include "avp/runtime/task/runtime_task.h"
#include "avp/runtime/task/runtime_timer.h"


enum {
    EVENT_PROBE_STAGE = 0x45564e54,
    EVENT_TASK_BASE = 0x70010000,
    EVENT_TASK_STRIDE = 0x1000,
    EVENT_STACK_BASE = 0x70100000,
    EVENT_STACK_STRIDE = 0x10000,
};

static uint16_t auto_event;
static uint16_t manual_event;
static uint16_t finish_event;
static uint16_t race_event;
static uint16_t timeout_event;
static volatile uint32_t auto_trace;
static volatile uint32_t manual_trace;
static volatile uint32_t race_result;
static volatile uint32_t timeout_result;

static void wait_forever(uint16_t event) {
    (void)runtime_event_wait(event, UINT64_MAX);
    pvpanic_trigger();
}

static void auto_waiter(void *context) {
    const uint32_t token = (uintptr_t)context;
    if (!runtime_event_wait(auto_event, UINT64_MAX)) {
        pvpanic_trigger();
    }
    auto_trace = auto_trace << 4 | token;
    wait_forever(finish_event);
}

static void manual_waiter(void *context) {
    const uint32_t token = (uintptr_t)context;
    if (!runtime_event_wait(manual_event, UINT64_MAX)) {
        pvpanic_trigger();
    }
    manual_trace = manual_trace << 4 | token;
    wait_forever(finish_event);
}

static void race_waiter(void *context) {
    (void)context;
    platform_progress(1, 1);
    const uint64_t deadline = runtime_timer_counter() +
        runtime_timer_frequency() / 100;
    platform_progress(1, 2);
    race_result = runtime_event_wait(race_event, deadline);
    platform_progress(1, 3);
    wait_forever(finish_event);
}

static void timeout_waiter(void *context) {
    (void)context;
    const uint64_t deadline = runtime_timer_counter() +
        runtime_timer_frequency() / 1000;
    timeout_result = runtime_event_wait(timeout_event, deadline);
    wait_forever(finish_event);
}

static void register_task(
    uint32_t task_index,
    RuntimeTaskEntry entry,
    void *context
) {
    RuntimeTask *task = (RuntimeTask *)(uintptr_t)(
        EVENT_TASK_BASE + task_index * EVENT_TASK_STRIDE
    );
    platform_progress(0, 0x100 + task_index);
    if (!runtime_task_register(
            task,
            runtime_domain_lookup(AVP_RUNTIME_DOMAIN_BOOT),
            EVENT_STACK_BASE + task_index * EVENT_STACK_STRIDE,
            entry,
            context
        )) {
        pvpanic_trigger();
    }
    platform_progress(0, 0x200 + task_index);
    if (!runtime_task_activate(task)) {
        pvpanic_trigger();
    }
    platform_progress(0, 0x300 + task_index);
}

void firmware_main(void) {
    pvpanic_initialize();
    runtime_scheduler_initialize();
    runtime_irq_system_initialize();
    platform_stage(EVENT_PROBE_STAGE);

    auto_event = runtime_event_create(true, false);
    platform_progress(0, 1);
    manual_event = runtime_event_create(false, false);
    platform_progress(0, 2);
    finish_event = runtime_event_create(true, false);
    platform_progress(0, 3);
    race_event = runtime_event_create(true, false);
    platform_progress(0, 4);
    timeout_event = runtime_event_create(true, false);
    platform_progress(0, 5);
    if (auto_event == 0 || manual_event == 0 || finish_event == 0 ||
        race_event == 0 || timeout_event == 0) {
        pvpanic_trigger();
    }

    runtime_event_set(auto_event);
    platform_progress(0, 6);
    uint32_t mode_trace = runtime_event_wait(auto_event, 0);
    platform_progress(0, 7);
    mode_trace |= (uint32_t)runtime_event_wait(auto_event, 0) << 1;
    platform_progress(0, 8);
    runtime_event_set(manual_event);
    platform_progress(0, 9);
    mode_trace |= (uint32_t)runtime_event_wait(manual_event, 0) << 2;
    platform_progress(0, 10);
    mode_trace |= (uint32_t)runtime_event_wait(manual_event, 0) << 3;
    platform_progress(0, 11);
    runtime_event_clear(manual_event);
    platform_progress(0, 12);
    mode_trace |= (uint32_t)runtime_event_wait(manual_event, 0) << 4;
    platform_progress(0, 13);

    for (uint32_t index = 0; index < 3; ++index) {
        register_task(
            index + 1,
            auto_waiter,
            (void *)(uintptr_t)(index + 1)
        );
    }
    platform_progress(0, 0x400);
    runtime_task_yield();
    platform_progress(0, 0x401);
    runtime_event_set(auto_event);
    platform_progress(0, 0x402);
    runtime_event_set(auto_event);
    platform_progress(0, 0x403);
    runtime_event_set(auto_event);
    platform_progress(0, 0x404);
    runtime_task_yield();
    platform_progress(0, 0x405);

    for (uint32_t index = 0; index < 2; ++index) {
        register_task(
            index + 4,
            manual_waiter,
            (void *)(uintptr_t)(index + 4)
        );
    }
    platform_progress(0, 0x406);
    runtime_task_yield();
    platform_progress(0, 0x407);
    runtime_event_set(manual_event);
    platform_progress(0, 0x408);
    runtime_event_clear(manual_event);
    platform_progress(0, 0x409);
    runtime_task_yield();
    platform_progress(0, 0x40a);

    register_task(6, race_waiter, 0);
    platform_progress(0, 0x40b);
    runtime_task_yield();
    platform_progress(0, 0x40c);
    runtime_event_set(race_event);
    platform_progress(0, 0x40d);
    runtime_task_yield();
    platform_progress(0, 0x40e);

    register_task(7, timeout_waiter, 0);
    platform_progress(0, 0x40f);
    runtime_task_yield();
    platform_progress(0, 0x410);
    const uint64_t boot_deadline = runtime_timer_counter() +
        runtime_timer_frequency() / 500;
    runtime_task_wait_until(boot_deadline);
    platform_progress(0, 0x411);

    const uint32_t race_and_timeout = timeout_result |
        race_result << 8 |
        runtime_task_switch_count() << 16;
    platform_progress(0, mode_trace);
    platform_progress(1, auto_trace);
    platform_progress(2, manual_trace);
    platform_progress(3, race_and_timeout);
    if (mode_trace != 0xd || auto_trace != 0x123 ||
        manual_trace != 0x45 || timeout_result != 0 || race_result != 1 ||
        runtime_task_switch_count() != 24 || runtime_timer_count() != 0 ||
        runtime_task_ready_count() != 0 ||
        runtime_current_domain() != AVP_RUNTIME_DOMAIN_BOOT) {
        pvpanic_trigger();
    }
    pvpanic_trigger();
}
