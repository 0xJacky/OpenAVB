#include "avp/platform/platform.h"
#include "avp/runtime/exception/pvpanic.h"
#include "avp/runtime/ipc/runtime_service.h"
#include "avp/runtime/ipc/runtime_svc.h"
#include "avp/runtime/task/runtime_task.h"


enum {
    IPC_QUEUE_PROBE_STAGE = 0x49504351,
    IPC_QUEUE_PROVIDER_STACK = 0x70020000,
    IPC_QUEUE_CALLER_STACK_BASE = 0x70030000,
    IPC_QUEUE_PROVIDER_PACKET = 0x70050000,
    IPC_QUEUE_CALLER_TASK_BASE = 0x70060000,
};

typedef struct {
    uint64_t token;
    uint64_t result;
} IPCQueuePacket;

typedef struct {
    IPCQueuePacket packet;
    uint8_t token;
} IPCQueueCaller;

static RuntimeServiceOwner owner;
static RuntimeDomain domain;
static IPCQueueCaller caller_contexts[4];
static uint32_t handler_trace;
static uint32_t completion_trace;

static void ipc_queue_method(
    void *handler_context,
    uint16_t method,
    uint16_t encoding_depth,
    void *raw_packet,
    uint32_t packet_size
) {
    (void)handler_context;
    IPCQueuePacket *packet = raw_packet;
    if (method != 1 || encoding_depth != 1 ||
        packet_size != sizeof(*packet) || packet->token < 1 ||
        packet->token > 4) {
        pvpanic_trigger();
    }
    packet->result = UINT64_C(0x9000) | packet->token;
    if (caller_contexts[packet->token - 1].packet.result != 0) {
        pvpanic_trigger();
    }
    handler_trace = handler_trace << 4 | (uint32_t)packet->token;
}

__attribute__((noreturn))
static void ipc_queue_caller(void *raw_context) {
    IPCQueueCaller *context = raw_context;
    const uint64_t result = runtime_service_call(
        AVP_RUNTIME_SERVICE_STORAGE,
        &context->packet,
        sizeof(context->packet),
        1,
        1
    );
    if (result != AVP_RUNTIME_SERVICE_STORAGE ||
        context->packet.result != (UINT64_C(0x9000) | context->token)) {
        pvpanic_trigger();
    }
    completion_trace = completion_trace << 4 | context->token;
    runtime_task_yield();
    pvpanic_trigger();
}

void firmware_main(void) {
    pvpanic_initialize();
    runtime_service_runtime_initialize();
    platform_stage(IPC_QUEUE_PROBE_STAGE);

    if (!runtime_domain_register(&domain, 4, AVP_RUNTIME_DOMAIN_BOOT, 4) ||
        !runtime_service_owner_initialize(
            &owner,
            &domain,
            IPC_QUEUE_PROVIDER_STACK,
            (void *)IPC_QUEUE_PROVIDER_PACKET
        ) || !runtime_service_register(
            &owner,
            AVP_RUNTIME_SERVICE_STORAGE,
            ipc_queue_method,
            0
        )) {
        pvpanic_trigger();
    }
    RuntimeTask *callers = (RuntimeTask *)IPC_QUEUE_CALLER_TASK_BASE;
    for (uint32_t index = 0; index < 4; ++index) {
        caller_contexts[index] = (IPCQueueCaller){
            .packet = {.token = index + 1},
            .token = index + 1,
        };
        if (!runtime_task_register(
                &callers[index],
                runtime_domain_lookup(AVP_RUNTIME_DOMAIN_BOOT),
                IPC_QUEUE_CALLER_STACK_BASE + index * 0x1000,
                ipc_queue_caller,
                &caller_contexts[index]
            ) || !runtime_task_activate(&callers[index])) {
            pvpanic_trigger();
        }
    }

    uint32_t invariants = 0;
    runtime_task_yield();
    if (runtime_service_pending_count(&owner) == 2 &&
        runtime_service_blocked_count(&owner) == 2) {
        invariants |= 1u << 0;
    }
    if (runtime_service_pending_head(&owner) == &callers[0] &&
        runtime_service_blocked_head(&owner) == &callers[2]) {
        invariants |= 1u << 1;
    }
    bool all_waiting = true;
    bool replies_hidden = true;
    for (uint32_t index = 0; index < 4; ++index) {
        all_waiting &= callers[index].state == RUNTIME_TASK_STATE_IPC_WAIT;
        replies_hidden &= caller_contexts[index].packet.result == 0;
    }
    if (all_waiting) {
        invariants |= 1u << 2;
    }
    if (replies_hidden) {
        invariants |= 1u << 3;
    }

    if (!runtime_service_owner_start(&owner)) {
        pvpanic_trigger();
    }
    runtime_task_yield();
    bool replies_complete = handler_trace == 0x1234;
    bool callers_ready = true;
    for (uint32_t index = 0; index < 4; ++index) {
        replies_complete &= caller_contexts[index].packet.result ==
            (UINT64_C(0x9000) | (index + 1));
        callers_ready &= callers[index].state == RUNTIME_TASK_STATE_READY;
    }
    if (replies_complete) {
        invariants |= 1u << 4;
    }
    if (runtime_service_pending_count(&owner) == 0 &&
        runtime_service_blocked_count(&owner) == 0 &&
        runtime_service_is_armed(&owner) &&
        !runtime_service_has_active_call(&owner)) {
        invariants |= 1u << 5;
    }
    if (callers_ready && runtime_task_ready_count() == 4) {
        invariants |= 1u << 6;
    }

    runtime_task_yield();
    if (completion_trace == 0x1234 &&
        runtime_task_current()->domain->domain_id == AVP_RUNTIME_DOMAIN_BOOT &&
        runtime_task_switch_count() == 12) {
        invariants |= 1u << 7;
    }
    platform_progress(0, handler_trace);
    platform_progress(1, completion_trace);
    platform_progress(2, invariants);
    platform_progress(3, runtime_task_switch_count());
    pvpanic_trigger();
}
