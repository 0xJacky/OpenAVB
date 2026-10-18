#include "avp/runtime/task/runtime_timer.h"

#include <assert.h>


static uint64_t counter;
static uint64_t programmed_delta;
static uint32_t program_count;
static uint32_t mask_count;
static uint32_t callback_trace;

void runtime_timer_arch_initialize(void) {
}

uint64_t runtime_timer_arch_counter(void) {
    return counter;
}

uint64_t runtime_timer_arch_frequency(void) {
    return 24000000;
}

void runtime_timer_arch_program(uint64_t ticks_until_deadline) {
    programmed_delta = ticks_until_deadline;
    ++program_count;
}

void runtime_timer_arch_mask(void) {
    ++mask_count;
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

static void record_expiry(RuntimeTimerEvent *event, void *context) {
    RuntimeTask *owner = context;
    assert(event == &owner->timer_event);
    callback_trace = callback_trace << 8 | (uint32_t)owner->frame.x[0];
}

int main(void) {
    static RuntimeTask task_a;
    static RuntimeTask task_b;
    static RuntimeTask task_c;
    static RuntimeTask task_d;
    task_a.frame.x[0] = 1;
    task_b.frame.x[0] = 2;
    task_c.frame.x[0] = 3;
    task_d.frame.x[0] = 4;

    runtime_timer_initialize();
    assert(runtime_timer_count() == 0);
    assert(mask_count == 1);
    assert(runtime_timer_arm(
        &task_a.timer_event,
        30,
        record_expiry,
        &task_a
    ));
    assert(runtime_timer_arm(
        &task_b.timer_event,
        10,
        record_expiry,
        &task_b
    ));
    assert(runtime_timer_arm(
        &task_c.timer_event,
        10,
        record_expiry,
        &task_c
    ));
    assert(runtime_timer_arm(
        &task_d.timer_event,
        20,
        record_expiry,
        &task_d
    ));
    assert(runtime_timer_count() == 4);
    assert(runtime_timer_next_deadline() == 10);
    assert(programmed_delta == 10);

    counter = 9;
    assert(runtime_timer_process_expired() == 0);
    counter = 10;
    assert(runtime_timer_process_expired() == 2);
    assert(callback_trace == UINT32_C(0x0203));
    assert(runtime_timer_next_deadline() == 20);
    counter = 20;
    assert(runtime_timer_process_expired() == 1);
    assert(callback_trace == UINT32_C(0x020304));
    assert(runtime_timer_cancel(&task_a.timer_event));
    assert(runtime_timer_cancel(&task_a.timer_event));
    assert(runtime_timer_count() == 0);

    counter = 100;
    assert(runtime_timer_arm(
        &task_a.timer_event,
        counter + UINT64_C(0x80000000),
        record_expiry,
        &task_a
    ));
    assert(programmed_delta == UINT64_C(0x7fffffff));
    assert(runtime_timer_cancel(&task_a.timer_event));
    assert(program_count >= 5);
    assert(mask_count >= 5);
    return 0;
}
