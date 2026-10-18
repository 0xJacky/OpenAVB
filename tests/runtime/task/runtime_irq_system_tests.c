#include "avp/runtime/task/runtime_irq.h"
#include "avp/runtime/task/runtime_task.h"

#include <assert.h>


static uint32_t acknowledged_interrupt = RUNTIME_IRQ_SPURIOUS_ID;
static uint32_t controller_initialize_count;
static uint32_t timer_initialize_count;
static uint32_t enable_count;
static uint32_t acknowledge_count;
static uint32_t end_count;
static uint32_t ended_interrupt;
static RuntimeTask selected_task;

void runtime_irq_arch_controller_initialize(void) {
    ++controller_initialize_count;
}

void runtime_irq_arch_virtual_timer_initialize(void) {
    ++timer_initialize_count;
}

uint32_t runtime_irq_arch_acknowledge_group0(void) {
    ++acknowledge_count;
    return acknowledged_interrupt;
}

void runtime_irq_arch_end_group0(uint32_t interrupt_id) {
    ++end_count;
    ended_interrupt = interrupt_id;
}

void runtime_irq_arch_set_virtual_timer_enabled(bool enabled) {
    assert(enabled);
    ++enable_count;
}

uint64_t runtime_irq_arch_mask(void) {
    return UINT64_C(0xc0);
}

void runtime_irq_arch_restore(uint64_t token) {
    assert(token == UINT64_C(0xc0));
}

RuntimeTask *runtime_task_timer_fiq_from_exception(RuntimeTask *trapped_task) {
    assert(trapped_task != 0);
    return &selected_task;
}

_Noreturn void pvpanic_trigger(void) {
    __builtin_trap();
}

int main(void) {
    RuntimeTask trapped_task = {0};
    runtime_irq_system_initialize();
    assert(controller_initialize_count == 1);
    assert(timer_initialize_count == 1);
    assert(enable_count == 1);

    assert(runtime_irq_system_timer_fiq_from_exception(&trapped_task) ==
        &trapped_task);
    assert(acknowledge_count == 1);
    assert(end_count == 0);

    acknowledged_interrupt = RUNTIME_IRQ_VIRTUAL_TIMER_ID;
    assert(runtime_irq_system_timer_fiq_from_exception(&trapped_task) ==
        &selected_task);
    assert(acknowledge_count == 2);
    assert(end_count == 1);
    assert(ended_interrupt == RUNTIME_IRQ_VIRTUAL_TIMER_ID);
}
