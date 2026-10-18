#include "avp/runtime/ipc/runtime_capability.h"
#include "avp/runtime/ipc/runtime_svc.h"
#include "avp/runtime/task/runtime_task.h"

#include <assert.h>


__attribute__((noreturn)) void runtime_firebloom_panic(void) {
    assert(false);
    __builtin_unreachable();
}

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
    return 0;
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

static void dynamic_entry(void *context) {
    (void)context;
}

int main(void) {
    RuntimeDomain domain;
    runtime_scheduler_initialize();
    assert(runtime_domain_register(
        &domain,
        AVP_RUNTIME_DOMAIN_SIMPLE_VIRTIO,
        AVP_RUNTIME_DOMAIN_BOOT,
        3
    ));

    RuntimeTaskDescriptor descriptor = {
        .entry = dynamic_entry,
        .entry_lower_bound = (uintptr_t)dynamic_entry,
        .entry_upper_bound = (uintptr_t)dynamic_entry + 4,
        .entry_cookie = UINT64_C(0xabcddcba),
        .entry_context = (void *)UINT64_C(0x12345678),
        .context_cookie = UINT64_C(0x55aa55aa),
    };
    RuntimeTask *boot = (RuntimeTask *)runtime_task_current();
    boot->frame.x[0] = domain.runtime_handle;
    boot->frame.x[1] = RUNTIME_CAPABILITY_RIGHT_WAIT |
        RUNTIME_CAPABILITY_RIGHT_SIGNAL;
    boot->frame.x[2] = (uintptr_t)&descriptor;
    boot->frame.x[3] = (uintptr_t)&descriptor;
    boot->frame.x[4] = (uintptr_t)(&descriptor + 1);
    boot->frame.x[5] = 1;
    assert(runtime_task_create_from_exception(boot) == boot);

    const uint16_t handle = (uint16_t)boot->frame.x[0];
    RuntimeTask *task = runtime_task_lookup_ordinal(1);
    assert(handle != 0);
    assert(task != 0);
    assert(task->state == RUNTIME_TASK_STATE_INITIALIZED);
    assert(task->descriptor.entry == dynamic_entry);
    assert(task->descriptor.entry_context == (void *)UINT64_C(0x12345678));
    assert(task->projected_flag == 1);
    assert(task->stack_allocation == 0);
    assert(task->stack_base == 0);
    assert(task->stack_top == 0);

    assert(runtime_task_activate_from_exception(
        boot,
        handle,
        UINT64_C(0x9988776655443322)
    ) == boot);
    assert(task->state == RUNTIME_TASK_STATE_READY);
    assert(task->stack_mapping == task->stack_allocation);
    assert((uintptr_t)task->stack_allocation == task->stack_base);
    assert(task->stack_top - task->stack_base == 0x4000);
    assert(task->frame.sp_el0 == task->stack_top);
    assert(task->frame.signed_elr_el1 == (uintptr_t)dynamic_entry);
    assert(task->frame.x[0] == UINT64_C(0x12345678));
    assert(task->frame.stack_guard == UINT64_C(0x9988776655440022));

    uint8_t *owned_stack = task->stack_allocation;
    owned_stack[0] = 0xa5;
    owned_stack[0x3fff] = 0x5a;
    assert(runtime_task_join_from_exception(boot, handle) == task);
    assert(boot->state == RUNTIME_TASK_STATE_IPC_WAIT);
    assert(boot->join_target == task);
    assert(task->state == RUNTIME_TASK_STATE_RUNNING);
    assert(runtime_task_current() == task);

    assert(runtime_task_exit_from_exception(task, -37) == boot);
    assert(runtime_task_current() == boot);
    assert(boot->state == RUNTIME_TASK_STATE_RUNNING);
    assert(boot->join_target == 0);
    assert(boot->frame.x[0] == (uint64_t)(int64_t)-37);
    assert(task->state == RUNTIME_TASK_STATE_EXITED);
    assert(task->exit_result == -37);
    assert(task->stack_mapping == 0);
    assert(task->stack_allocation == 0);
    assert(task->stack_base == 0);
    assert(task->stack_top == 0);
    assert(owned_stack[0] == 0);
    assert(owned_stack[0x3fff] == 0);
    assert(runtime_task_ready_count() == 0);

    boot->frame.x[0] = 0;
    assert(runtime_task_join_from_exception(boot, handle) == boot);
    assert(boot->frame.x[0] == (uint64_t)(int64_t)-37);
    assert(runtime_task_activate_from_exception(boot, handle, 0) == boot);
    assert(task->state == RUNTIME_TASK_STATE_READY);
    assert(task->stack_allocation != 0);
    assert(task->stack_top - task->stack_base == 0x4000);
    assert(task->frame.stack_guard == UINT64_C(0x1122334455660088));
    return 0;
}
