#include "avp/runtime/ipc/runtime_svc.h"
#include "avp/runtime/task/runtime_task.h"

#include <assert.h>


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

static void task_entry(void *context) {
    (void)context;
}

int main(void) {
    RuntimeTask task;
    RuntimeTask peer_task;
    RuntimeTask restart_task;
    RuntimeDomain domain;
    runtime_scheduler_initialize();
    assert(runtime_task_current() != 0);
    assert(runtime_task_current()->domain->domain_id == AVP_RUNTIME_DOMAIN_BOOT);
    assert(runtime_task_current()->state == RUNTIME_TASK_STATE_RUNNING);
    assert(runtime_domain_register(
        &domain,
        AVP_RUNTIME_DOMAIN_SIMPLE_VIRTIO,
        AVP_RUNTIME_DOMAIN_BOOT,
        3
    ));
    assert(runtime_task_register(
        &task,
        &domain,
        0x70020000,
        task_entry,
        0
    ));
    assert(task.global_ordinal == 1);
    assert(task.domain_ordinal == 0);
    assert(task.domain == &domain);
    assert(task.state == RUNTIME_TASK_STATE_INITIALIZED);
    assert(runtime_task_register(
        &peer_task,
        &domain,
        0x70030000,
        task_entry,
        0
    ));
    assert(peer_task.global_ordinal == 2);
    assert(peer_task.domain_ordinal == 1);
    assert(runtime_task_register(
        &restart_task,
        &domain,
        0x70040000,
        task_entry,
        (void *)UINT64_C(0x1234)
    ));
    restart_task.frame.x[0] = 0;
    restart_task.state = RUNTIME_TASK_STATE_EXITED;
    assert(runtime_task_capability_handle(&task) == 1);
    assert(runtime_task_capability_handle(&peer_task) == 2);
    RuntimeTask *boot = (RuntimeTask *)runtime_task_current();
    boot->frame.signed_elr_el1 = 1;
    assert(runtime_task_require_exception_context() == boot);
    assert(runtime_task_activate_from_exception(
        boot,
        runtime_task_capability_handle(&task),
        UINT64_C(0x1122334455667788)
    ) == boot);
    assert(runtime_task_activate_from_exception(
        boot,
        runtime_task_capability_handle(&peer_task),
        0
    ) == boot);
    assert(runtime_task_activate_from_exception(
        boot,
        runtime_task_capability_handle(&restart_task),
        UINT64_C(0x9988776655443322)
    ) == boot);
    assert(task.frame.stack_guard == UINT64_C(0x1122334455660088));
    assert(peer_task.frame.stack_guard == UINT64_C(0x1122334455660088));
    assert(restart_task.frame.x[0] == UINT64_C(0x1234));
    assert(restart_task.frame.stack_guard == UINT64_C(0x9988776655440022));
    assert(runtime_task_ready_count() == 3);
    assert(!runtime_task_activate(&task));
    assert(!runtime_task_register_parked(
        &task,
        &domain,
        0x70020000,
        task_entry,
        0
    ));
    assert(runtime_task_switch_count() == 0);
    return 0;
}
