#pragma once

#include "avp/runtime/task/runtime_task.h"

#include <stdbool.h>
#include <stdint.h>


void runtime_timer_initialize(void);
uint64_t runtime_timer_counter(void);
uint64_t runtime_timer_frequency(void);
bool runtime_timer_arm(
    RuntimeTimerEvent *event,
    uint64_t deadline,
    RuntimeTimerCallback callback,
    RuntimeTask *owner
);
bool runtime_timer_cancel(RuntimeTimerEvent *event);
bool runtime_timer_is_armed(const RuntimeTimerEvent *event);
uint32_t runtime_timer_process_expired(void);
uint32_t runtime_timer_count(void);
uint64_t runtime_timer_next_deadline(void);
bool runtime_timer_interrupt_pending(void);
bool runtime_timer_irq_pending(void);
void runtime_timer_wait_for_interrupt(void);
