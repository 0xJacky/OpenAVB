#include "avp/runtime/ipc/runtime_service.h"
#include "avp/runtime/ipc/runtime_svc.h"

#include <assert.h>


typedef struct {
    uint64_t token;
    uint64_t result;
} TestPacket;

void runtime_task_arch_set_current(RuntimeTask *task) { (void)task; }
void runtime_task_arch_initialize_frame(RuntimeTaskFrame *frame) { (void)frame; }
uint64_t runtime_task_arch_generate_guard_seed(void) {
    return UINT64_C(0x1122334455667788);
}
void runtime_timer_arch_initialize(void) {}
uint64_t runtime_timer_arch_counter(void) { return 0; }
uint64_t runtime_timer_arch_frequency(void) { return 24000000; }
void runtime_timer_arch_program(uint64_t ticks) { (void)ticks; }
void runtime_timer_arch_mask(void) {}
bool runtime_timer_arch_interrupt_pending(void) { return false; }
bool runtime_timer_arch_irq_pending(void) { return false; }
void runtime_timer_arch_wait(void) { assert(false); }
uint64_t runtime_idle_pump(void) { assert(false); }
void runtime_task_yield(void) { assert(false); }
void runtime_service_complete(void) { assert(false); }
void runtime_service_receive(void) { assert(false); }

static void task_entry(void *context) { (void)context; }

static void service_method(
    void *handler_context,
    uint16_t method,
    uint16_t encoding_depth,
    void *packet,
    uint32_t packet_size
) {
    (void)handler_context;
    assert(method == 1);
    assert(encoding_depth == 1);
    assert(packet_size == sizeof(TestPacket));
    TestPacket *request = packet;
    request->result = UINT64_C(0x9000) | request->token;
}

static void prepare_call(RuntimeTask *task, TestPacket *packet) {
    task->frame.x[0] = AVP_RUNTIME_SERVICE_STORAGE;
    task->frame.x[1] = (uintptr_t)packet;
    task->frame.x[2] = sizeof(*packet);
    task->frame.x[3] = 1;
    task->frame.x[4] = 1;
}

static void test_provider_first(void) {
    static RuntimeServiceOwner owner;
    static RuntimeDomain domain;
    static uint8_t provider_packet[AVP_RUNTIME_SERVICE_PACKET_LIMIT];
    TestPacket packet = {.token = 0xa};

    runtime_service_runtime_initialize();
    assert(runtime_domain_register(
        &domain,
        AVP_RUNTIME_DOMAIN_SIMPLE_VIRTIO,
        AVP_RUNTIME_DOMAIN_BOOT,
        3
    ));
    assert(runtime_service_owner_initialize(
        &owner,
        &domain,
        0x70020000,
        provider_packet
    ));
    assert(runtime_service_register(
        &owner,
        AVP_RUNTIME_SERVICE_STORAGE,
        service_method,
        0
    ));
    assert(runtime_service_owner_start(&owner));

    RuntimeTask *boot = (RuntimeTask *)runtime_task_current();
    RuntimeTask *current = runtime_task_schedule_from_exception(boot);
    assert(current == &owner.task);
    current = runtime_service_receive_from_exception(current);
    assert(current == boot);
    assert(runtime_service_is_armed(&owner));

    prepare_call(boot, &packet);
    current = runtime_service_call_from_exception(boot);
    assert(current == &owner.task);
    assert(runtime_service_has_active_call(&owner));
    service_method(0, 1, 1, domain.service_packet, sizeof(packet));
    current = runtime_service_complete_from_exception(current);
    assert(current == &owner.task);
    current = runtime_service_receive_from_exception(current);
    assert(current == boot);
    assert(packet.result == UINT64_C(0x900a));
    assert(boot->frame.x[0] == AVP_RUNTIME_SERVICE_STORAGE);
    assert(boot->ipc_inflight_token == 0);
    assert(runtime_service_is_armed(&owner));
}

static void test_caller_first_capacity_pressure(void) {
    static RuntimeServiceOwner owner;
    static RuntimeDomain domain;
    static RuntimeTask callers[4];
    static uint8_t provider_packet[AVP_RUNTIME_SERVICE_PACKET_LIMIT];
    TestPacket packets[4];

    runtime_service_runtime_initialize();
    assert(runtime_domain_register(&domain, 4, AVP_RUNTIME_DOMAIN_BOOT, 4));
    assert(runtime_service_owner_initialize(
        &owner,
        &domain,
        0x70020000,
        provider_packet
    ));
    assert(runtime_service_register(
        &owner,
        AVP_RUNTIME_SERVICE_STORAGE,
        service_method,
        0
    ));
    for (uint32_t index = 0; index < 4; ++index) {
        packets[index] = (TestPacket){.token = index + 1};
        assert(runtime_task_register(
            &callers[index],
            runtime_domain_lookup(AVP_RUNTIME_DOMAIN_BOOT),
            0x70030000 + index * 0x1000,
            task_entry,
            0
        ));
        prepare_call(&callers[index], &packets[index]);
        assert(runtime_task_activate(&callers[index]));
    }

    RuntimeTask *boot = (RuntimeTask *)runtime_task_current();
    RuntimeTask *current = runtime_task_schedule_from_exception(boot);
    for (uint32_t index = 0; index < 4; ++index) {
        assert(current == &callers[index]);
        current = runtime_service_call_from_exception(current);
    }
    assert(current == boot);
    assert(runtime_service_pending_count(&owner) == 2);
    assert(runtime_service_blocked_count(&owner) == 2);
    assert(runtime_service_pending_head(&owner) == &callers[0]);
    assert(runtime_service_blocked_head(&owner) == &callers[2]);
    for (uint32_t index = 0; index < 4; ++index) {
        assert(callers[index].state == RUNTIME_TASK_STATE_IPC_WAIT);
    }

    assert(runtime_service_owner_start(&owner));
    current = runtime_task_schedule_from_exception(boot);
    assert(current == &owner.task);
    for (uint32_t index = 0; index < 4; ++index) {
        current = runtime_service_receive_from_exception(current);
        assert(current == &owner.task);
        assert(((TestPacket *)domain.service_packet)->token == index + 1);
        service_method(0, 1, 1, domain.service_packet, sizeof(TestPacket));
        current = runtime_service_complete_from_exception(current);
        assert(current == &owner.task);
        assert(packets[index].result == (UINT64_C(0x9000) | (index + 1)));
        assert(callers[index].ipc_inflight_token == 0);
    }
    assert(runtime_service_pending_count(&owner) == 0);
    assert(runtime_service_blocked_count(&owner) == 0);
    current = runtime_service_receive_from_exception(current);
    assert(current == boot);
    assert(runtime_service_is_armed(&owner));

    current = runtime_task_schedule_from_exception(current);
    for (uint32_t index = 0; index < 4; ++index) {
        assert(current == &callers[index]);
        assert(current->frame.x[0] == AVP_RUNTIME_SERVICE_STORAGE);
        current = runtime_task_schedule_from_exception(current);
    }
    assert(current == boot);
}

static void test_registry_reset_cleans_inflight_ownership(void) {
    static RuntimeServiceOwner owner;
    static RuntimeDomain domain;
    static RuntimeTask callers[3];
    static uint8_t provider_packet[AVP_RUNTIME_SERVICE_PACKET_LIMIT];
    TestPacket packets[3];

    runtime_service_runtime_initialize();
    assert(runtime_domain_register(&domain, 4, AVP_RUNTIME_DOMAIN_BOOT, 4));
    assert(runtime_service_owner_initialize(
        &owner,
        &domain,
        0x70020000,
        provider_packet
    ));
    assert(runtime_service_register(
        &owner,
        AVP_RUNTIME_SERVICE_STORAGE,
        service_method,
        0
    ));
    for (uint32_t index = 0; index < 3; ++index) {
        packets[index] = (TestPacket){.token = index + 1};
        assert(runtime_task_register(
            &callers[index],
            runtime_domain_lookup(AVP_RUNTIME_DOMAIN_BOOT),
            0x70030000 + index * 0x1000,
            task_entry,
            0
        ));
        prepare_call(&callers[index], &packets[index]);
        assert(runtime_task_activate(&callers[index]));
    }
    RuntimeTask *current = runtime_task_schedule_from_exception(
        (RuntimeTask *)runtime_task_current()
    );
    for (uint32_t index = 0; index < 3; ++index) {
        current = runtime_service_call_from_exception(current);
    }
    assert(runtime_service_pending_count(&owner) == 2);
    assert(runtime_service_blocked_count(&owner) == 1);

    runtime_service_runtime_initialize();
    assert(!owner.initialized && !owner.started && owner.domain == 0);
    assert(domain.service_packet == 0 && domain.service_key == 0 &&
        domain.service_active_token == 0);
    for (uint32_t index = 0; index < 3; ++index) {
        assert(callers[index].ipc_packet_mapping == 0);
        assert(callers[index].ipc_original_va == 0);
        assert(callers[index].ipc_inflight_token == 0);
        assert(callers[index].ready_link.next == 0);
        assert(callers[index].ready_link.previous == 0);
        assert(callers[index].state == RUNTIME_TASK_STATE_EMPTY);
    }

    assert(runtime_domain_register(&domain, 4, AVP_RUNTIME_DOMAIN_BOOT, 4));
    assert(runtime_service_owner_initialize(
        &owner,
        &domain,
        0x70020000,
        provider_packet
    ));
}

int main(void) {
    test_provider_first();
    test_caller_first_capacity_pressure();
    test_registry_reset_cleans_inflight_ownership();
    return 0;
}
