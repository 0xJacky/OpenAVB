#include "avp/platform/platform.h"
#include "avp/runtime/exception/pvpanic.h"
#include "avp/runtime/ipc/runtime_service.h"
#include "avp/runtime/ipc/runtime_svc.h"
#include "avp/runtime/task/runtime_task.h"


enum {
    IPC_TOKEN_INVALID_STAGE = 0x49504354,
    IPC_TOKEN_PROVIDER_STACK = 0x70020000,
    IPC_TOKEN_PROVIDER_PACKET = 0x70050000,
};

static RuntimeServiceOwner owner;
static RuntimeDomain domain;
static uint64_t packet;

static void corrupt_reply_token(
    void *handler_context,
    uint16_t method,
    uint16_t encoding_depth,
    void *raw_packet,
    uint32_t packet_size
) {
    (void)handler_context;
    (void)raw_packet;
    if (method != 1 || encoding_depth != 1 || packet_size != sizeof(packet)) {
        pvpanic_trigger();
    }
    RuntimeTask *caller = runtime_task_lookup_ordinal(
        domain.service_active_caller_ordinal
    );
    if (caller == 0) {
        pvpanic_trigger();
    }
    platform_progress(0, 1);
    caller->ipc_inflight_token ^= 1;
}

void firmware_main(void) {
    pvpanic_initialize();
    runtime_service_runtime_initialize();
    platform_stage(IPC_TOKEN_INVALID_STAGE);
    if (!runtime_domain_register(&domain, 4, AVP_RUNTIME_DOMAIN_BOOT, 4) ||
        !runtime_service_owner_initialize(
            &owner,
            &domain,
            IPC_TOKEN_PROVIDER_STACK,
            (void *)IPC_TOKEN_PROVIDER_PACKET
        ) || !runtime_service_register(
            &owner,
            AVP_RUNTIME_SERVICE_STORAGE,
            corrupt_reply_token,
            0
        ) || !runtime_service_owner_start(&owner)) {
        pvpanic_trigger();
    }
    (void)runtime_service_call(
        AVP_RUNTIME_SERVICE_STORAGE,
        &packet,
        sizeof(packet),
        1,
        1
    );
    platform_progress(1, 1);
    pvpanic_trigger();
}
