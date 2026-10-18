#include "avp/runtime/ipc/runtime_capability.h"
#include "avp/runtime/task/runtime_event.h"
#include "avp/runtime/ipc/runtime_svc.h"
#include "avp/runtime/task/runtime_task.h"
#include "avp/runtime/task/runtime_timer.h"

#include <assert.h>


static uint64_t counter;

void runtime_task_arch_set_current(RuntimeTask *task) {
    (void)task;
}

void runtime_task_arch_initialize_frame(RuntimeTaskFrame *frame) {
    (void)frame;
}

uint64_t runtime_task_arch_generate_guard_seed(void) {
    return UINT64_C(0x1122334455667788);
}

void runtime_timer_arch_initialize(void) {
}

uint64_t runtime_timer_arch_counter(void) {
    return counter;
}

uint64_t runtime_timer_arch_frequency(void) {
    return 24000000;
}

void runtime_timer_arch_program(uint64_t ticks_until_deadline) {
    (void)ticks_until_deadline;
}

void runtime_timer_arch_mask(void) {
}

bool runtime_timer_arch_interrupt_pending(void) {
    return false;
}

bool runtime_timer_arch_irq_pending(void) {
    return false;
}

void runtime_timer_arch_wait(void) {
    assert(false);
}

uint64_t runtime_idle_pump(void) {
    assert(false);
}

void runtime_task_yield(void) {
    assert(false);
}

static void task_entry(void *context) {
    (void)context;
}

static RuntimeTask *boot_task(void) {
    return (RuntimeTask *)runtime_task_current();
}

static void register_ready_task(RuntimeTask *task, uintptr_t stack_top) {
    assert(runtime_task_register(
        task,
        runtime_domain_lookup(AVP_RUNTIME_DOMAIN_BOOT),
        stack_top,
        task_entry,
        0
    ));
    assert(runtime_task_activate(task));
}

static void test_event_modes(void) {
    RuntimeEvent auto_event = {0};
    RuntimeEvent manual_event = {0};
    runtime_scheduler_initialize();
    const uint16_t auto_handle = runtime_event_initialize(
        &auto_event,
        true,
        false
    );
    const uint16_t manual_handle = runtime_event_initialize(
        &manual_event,
        false,
        false
    );
    RuntimeTask *boot = boot_task();
    assert(auto_handle != 0 && manual_handle != 0);
    assert(runtime_event_signal_from_exception(
        boot,
        auto_handle,
        true
    ) == boot);
    assert(auto_event.signaled == 1);
    assert(runtime_event_wait_from_exception(boot, auto_handle, 0) == boot);
    assert(boot->frame.x[0] == 1 && auto_event.signaled == 0);
    assert(runtime_event_wait_from_exception(boot, auto_handle, 0) == boot);
    assert(boot->frame.x[0] == 0);

    assert(runtime_event_signal_from_exception(
        boot,
        manual_handle,
        true
    ) == boot);
    assert(runtime_event_wait_from_exception(boot, manual_handle, 0) == boot);
    assert(boot->frame.x[0] == 1 && manual_event.signaled == 1);
    assert(runtime_event_signal_from_exception(
        boot,
        manual_handle,
        false
    ) == boot);
    assert(runtime_event_wait_from_exception(boot, manual_handle, 0) == boot);
    assert(boot->frame.x[0] == 0 && manual_event.signaled == 0);
}

static void test_auto_reset_fifo(void) {
    RuntimeEvent event = {0};
    RuntimeTask tasks[3];
    runtime_scheduler_initialize();
    const uint16_t handle = runtime_event_initialize(&event, true, false);
    for (uint32_t index = 0; index < 3; ++index) {
        register_ready_task(&tasks[index], 0x70020000 + index * 0x10000);
    }
    RuntimeTask *boot = boot_task();
    RuntimeTask *current = runtime_task_schedule_from_exception(boot);
    for (uint32_t index = 0; index < 3; ++index) {
        assert(current == &tasks[index]);
        current = runtime_event_wait_from_exception(
            current,
            handle,
            UINT64_MAX
        );
    }
    assert(current == boot && runtime_task_ready_count() == 0);
    for (uint32_t index = 0; index < 3; ++index) {
        assert(runtime_event_signal_from_exception(
            current,
            handle,
            true
        ) == current);
        assert(event.signaled == 0 &&
            runtime_task_ready_count() == index + 1 &&
            tasks[index].frame.x[0] == 1);
    }
    for (uint32_t index = 0; index < 3; ++index) {
        current = runtime_task_schedule_from_exception(current);
        assert(current == &tasks[index]);
    }
}

static void test_manual_reset_wake_all(void) {
    RuntimeEvent event = {0};
    RuntimeTask first;
    RuntimeTask second;
    runtime_scheduler_initialize();
    const uint16_t handle = runtime_event_initialize(&event, false, false);
    register_ready_task(&first, 0x70020000);
    register_ready_task(&second, 0x70030000);
    RuntimeTask *boot = boot_task();
    RuntimeTask *current = runtime_task_schedule_from_exception(boot);
    assert(current == &first);
    current = runtime_event_wait_from_exception(current, handle, UINT64_MAX);
    assert(current == &second);
    current = runtime_event_wait_from_exception(current, handle, UINT64_MAX);
    assert(current == boot);
    assert(runtime_event_signal_from_exception(current, handle, true) == current);
    assert(event.signaled == 1 && runtime_task_ready_count() == 2);
    current = runtime_task_schedule_from_exception(current);
    assert(current == &first && current->frame.x[0] == 1);
    current = runtime_task_schedule_from_exception(current);
    assert(current == &second && current->frame.x[0] == 1);
}

static void test_timeout_and_signal_race(void) {
    RuntimeEvent timeout_event = {0};
    RuntimeTask timeout_task;
    runtime_scheduler_initialize();
    const uint16_t timeout_handle = runtime_event_initialize(
        &timeout_event,
        true,
        false
    );
    register_ready_task(&timeout_task, 0x70020000);
    RuntimeTask *boot = boot_task();
    RuntimeTask *current = runtime_task_schedule_from_exception(boot);
    counter = 10;
    current = runtime_event_wait_from_exception(current, timeout_handle, 20);
    assert(current == boot && runtime_timer_count() == 1);
    counter = 20;
    assert(runtime_timer_process_expired() == 1);
    assert(runtime_timer_count() == 0 && runtime_task_ready_count() == 1);
    current = runtime_task_schedule_from_exception(current);
    assert(current == &timeout_task && current->frame.x[0] == 0);

    RuntimeEvent signal_event = {0};
    RuntimeTask signal_task;
    runtime_scheduler_initialize();
    const uint16_t signal_handle = runtime_event_initialize(
        &signal_event,
        true,
        false
    );
    register_ready_task(&signal_task, 0x70030000);
    boot = boot_task();
    current = runtime_task_schedule_from_exception(boot);
    counter = 30;
    current = runtime_event_wait_from_exception(current, signal_handle, 40);
    assert(current == boot && runtime_timer_count() == 1);
    assert(runtime_event_signal_from_exception(
        current,
        signal_handle,
        true
    ) == current);
    assert(runtime_timer_count() == 0 && runtime_task_ready_count() == 1);
    current = runtime_task_schedule_from_exception(current);
    assert(current == &signal_task && current->frame.x[0] == 1);
}

static void test_capability_types(void) {
    RuntimeTask task;
    runtime_scheduler_initialize();
    register_ready_task(&task, 0x70020000);
    RuntimeTask *boot = boot_task();
    boot->frame.x[0] = 0;
    boot->frame.x[1] = 0;
    boot->frame.x[2] = RUNTIME_CAPABILITY_RIGHT_WAIT;
    assert(runtime_event_create_from_exception(boot) == boot);
    const uint16_t wait_handle = (uint16_t)boot->frame.x[0];
    assert(runtime_capability_resolve(
        wait_handle,
        RUNTIME_CAPABILITY_TYPE_EVENT,
        RUNTIME_CAPABILITY_RIGHT_WAIT
    ) != 0);
    assert(runtime_capability_resolve(
        wait_handle,
        RUNTIME_CAPABILITY_TYPE_EVENT,
        RUNTIME_CAPABILITY_RIGHT_SIGNAL
    ) == 0);
    assert(runtime_capability_resolve(
        runtime_task_capability_handle(&task),
        RUNTIME_CAPABILITY_TYPE_EVENT,
        RUNTIME_CAPABILITY_RIGHT_WAIT
    ) == 0);
}

int main(void) {
    test_event_modes();
    test_auto_reset_fifo();
    test_manual_reset_wake_all();
    test_timeout_and_signal_race();
    test_capability_types();
    return 0;
}
