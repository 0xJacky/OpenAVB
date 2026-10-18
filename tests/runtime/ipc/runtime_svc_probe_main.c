#include "avp/platform/platform.h"
#include "avp/runtime/exception/pvpanic.h"
#include "avp/runtime/ipc/runtime_service.h"
#include "avp/runtime/ipc/runtime_svc.h"
#include "avp/runtime/task/runtime_task.h"


enum {
    RUNTIME_SVC_PROBE_STAGE = 0x53564344,
    RUNTIME_SVC_PROBE_STACK_TOP = 0x70020000,
    RUNTIME_SVC_PROVIDER_PACKET = 0x70050000,
};

static const uint64_t locality_service = UINT64_C(0x6c61636f6c766373);
static RuntimeServiceOwner service_owner;
typedef struct {
    uint64_t result;
} LocalityRequest;
static LocalityRequest request;

static void query_locality_from_owner(
    void *handler_context,
    uint16_t method,
    uint16_t encoding_depth,
    void *packet,
    uint32_t packet_size
) {
    (void)handler_context;
    if (method != 1 || encoding_depth != 1 ||
        packet_size != sizeof(request)) {
        pvpanic_trigger();
    }
    ((LocalityRequest *)packet)->result = runtime_domain_query_locality(
        AVP_RUNTIME_DOMAIN_SIMPLE_VIRTIO
    );
}

void firmware_main(void) {
    pvpanic_initialize();
    runtime_service_runtime_initialize();
    platform_stage(RUNTIME_SVC_PROBE_STAGE);
    platform_progress(
        0,
        runtime_domain_query_locality(AVP_RUNTIME_DOMAIN_BOOT)
    );
    platform_progress(
        1,
        runtime_domain_query_locality(AVP_RUNTIME_DOMAIN_SIMPLE_VIRTIO)
    );
    RuntimeDomain *service_domain = 0;
    static RuntimeDomain domain;
    if (!runtime_domain_register(
            &domain,
            AVP_RUNTIME_DOMAIN_SIMPLE_VIRTIO,
            AVP_RUNTIME_DOMAIN_BOOT,
            3
        ) || (service_domain = runtime_domain_lookup(
            AVP_RUNTIME_DOMAIN_SIMPLE_VIRTIO
        )) == 0 || !runtime_service_owner_initialize(
            &service_owner,
            service_domain,
            RUNTIME_SVC_PROBE_STACK_TOP,
            (void *)RUNTIME_SVC_PROVIDER_PACKET
        ) || !runtime_service_register(
            &service_owner,
            locality_service,
            query_locality_from_owner,
            0
        ) || !runtime_service_owner_start(&service_owner)) {
        pvpanic_trigger();
    }
    platform_progress(
        2,
        runtime_domain_query_locality(AVP_RUNTIME_DOMAIN_SIMPLE_VIRTIO)
    );
    (void)runtime_service_call(
        locality_service,
        &request,
        sizeof(request),
        1,
        1
    );
    if (request.result != 1) {
        pvpanic_trigger();
    }
    platform_progress(
        3,
        (uint32_t)request.result
    );
    pvpanic_trigger();
}
