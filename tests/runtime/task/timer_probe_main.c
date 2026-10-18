#include "avp/platform/platform.h"
#include "avp/runtime/exception/pvpanic.h"
#include "avp/runtime/task/runtime_irq.h"
#include "avp/runtime/ipc/runtime_svc.h"
#include "avp/runtime/task/runtime_task.h"
#include "avp/runtime/task/runtime_timer.h"


enum {
    TIMER_PROBE_STAGE = 0x54494d52,
    TIMER_IDLE_TASK_IPA = 0x7003d7c0,
    TIMER_WORKER_STACK_TOP = 0x703e0000,
};

static RuntimeTask worker_task;
static volatile uint32_t worker_trace;

static void timer_worker(void *context) {
    (void)context;
    worker_trace = 1;
    while (runtime_timer_count() != 0) {
        __asm__ volatile("yield");
    }
    worker_trace = 0x101;
    runtime_task_yield();
    pvpanic_trigger();
}

void firmware_main(void) {
    pvpanic_initialize();
    runtime_scheduler_initialize();
    runtime_irq_system_initialize();
    platform_stage(TIMER_PROBE_STAGE);

    const uint64_t frequency = runtime_timer_frequency();
    const uint64_t start = runtime_timer_counter();
    const uint64_t duration = frequency / 500;
    if (duration == 0 ||
        (uintptr_t)runtime_task_idle() != TIMER_IDLE_TASK_IPA ||
        !runtime_task_register(
            &worker_task,
            runtime_domain_lookup(AVP_RUNTIME_DOMAIN_BOOT),
            TIMER_WORKER_STACK_TOP,
            timer_worker,
            0
        ) || !runtime_task_activate(&worker_task)) {
        pvpanic_trigger();
    }
    const uint64_t deadline = start + duration;
    runtime_task_wait_until(deadline);
    const uint64_t completed = runtime_timer_counter();

    platform_progress(0, completed >= deadline);
    platform_progress(1, runtime_task_switch_count());
    platform_progress(2, worker_trace);
    platform_progress(3, runtime_current_domain());
    if (completed < deadline || runtime_task_switch_count() != 4 ||
        runtime_timer_count() != 0 || runtime_task_ready_count() != 0 ||
        worker_trace != 0x101 ||
        runtime_current_domain() != AVP_RUNTIME_DOMAIN_BOOT) {
        pvpanic_trigger();
    }
    pvpanic_trigger();
}
