#include "avp/runtime/ipc/runtime_capability.h"
#include "avp/runtime/ipc/runtime_svc.h"

#include <assert.h>


int main(void) {
    runtime_capability_initialize();
    runtime_domain_reset(AVP_RUNTIME_DOMAIN_BOOT);
    assert(runtime_current_domain() == AVP_RUNTIME_DOMAIN_BOOT);
    assert(runtime_domain_resolve_locality(AVP_RUNTIME_DOMAIN_BOOT));
    assert(runtime_domain_resolve_locality(
        AVP_RUNTIME_DOMAIN_SIMPLE_VIRTIO
    ));
    RuntimeDomain domain;
    assert(runtime_domain_register(
        &domain,
        AVP_RUNTIME_DOMAIN_SIMPLE_VIRTIO,
        AVP_RUNTIME_DOMAIN_BOOT,
        3
    ));
    assert(runtime_domain_registry_contains(
        AVP_RUNTIME_DOMAIN_SIMPLE_VIRTIO
    ));
    assert(!runtime_domain_resolve_locality(
        AVP_RUNTIME_DOMAIN_SIMPLE_VIRTIO
    ));
    runtime_domain_set_current(&domain);
    assert(runtime_domain_resolve_locality(
        AVP_RUNTIME_DOMAIN_SIMPLE_VIRTIO
    ));
    assert(!runtime_domain_register(
        &domain,
        AVP_RUNTIME_DOMAIN_SIMPLE_VIRTIO,
        AVP_RUNTIME_DOMAIN_BOOT,
        3
    ));
    RuntimeDomain invalid_domain;
    assert(!runtime_domain_register(
        &invalid_domain,
        16,
        AVP_RUNTIME_DOMAIN_BOOT,
        16
    ));
    return 0;
}
