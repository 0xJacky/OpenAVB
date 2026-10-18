#include "avp/platform/platform.h"
#include "avp/runtime/exception/pvpanic.h"
#include "avp/runtime/ipc/runtime_service.h"
#include "avp/runtime/ipc/runtime_svc.h"
#include "avp/runtime/task/runtime_task.h"
#include "avp/devices/simplevirtio/simple_virtio.h"
#include "avp/devices/simplevirtio/simple_virtio_service.h"


enum {
    BLOCK_PROBE_BUFFER_IPA = 0x70007000,
    BLOCK_PROBE_STAGE = 0x424c434b,
    BLOCK_SERVICE_STACK_TOP = 0x70020000,
};

static RuntimeServiceOwner storage_owner;
static SimpleVirtioStorageService storage_service;

void firmware_main(void) {
    pvpanic_initialize();
    runtime_service_runtime_initialize();
    if (!simple_virtio_storage_service_register(
            &storage_owner,
            &storage_service,
            BLOCK_SERVICE_STACK_TOP
        ) || runtime_domain_query_locality(
            AVP_RUNTIME_DOMAIN_SIMPLE_VIRTIO
        )) {
        pvpanic_trigger();
    }
    SimpleVirtioBlock block;
    if (!simple_virtio_block_domain_initialize(
            &block,
            SIMPLE_VIRTIO_BLOCK_SLOT_ROOT
        ) ||
        block.sector_count != 1 || block.block_size != 512) {
        pvpanic_trigger();
    }
    uint8_t *buffer = (uint8_t *)BLOCK_PROBE_BUFFER_IPA;
    for (uint32_t index = 0; index < block.block_size; ++index) {
        buffer[index] = (uint8_t)(index ^ 0xa5);
    }
    if (!simple_virtio_block_write(&block, 0, 1, buffer)) {
        pvpanic_trigger();
    }
    for (uint32_t index = 0; index < block.block_size; ++index) {
        buffer[index] = 0;
    }
    if (!simple_virtio_block_read(&block, 0, 1, buffer)) {
        pvpanic_trigger();
    }
    for (uint32_t index = 0; index < block.block_size; ++index) {
        if (buffer[index] != (uint8_t)(index ^ 0xa5)) {
            pvpanic_trigger();
        }
    }
    platform_progress(0, !runtime_domain_query_locality(
        AVP_RUNTIME_DOMAIN_SIMPLE_VIRTIO
    ));
    platform_progress(1, runtime_task_switch_count());
    platform_progress(
        2,
        storage_service.initialize_count |
            storage_service.read_count << 8 |
            storage_service.write_count << 16
    );
    platform_progress(
        3,
        runtime_current_domain() == AVP_RUNTIME_DOMAIN_BOOT
    );
    platform_stage(BLOCK_PROBE_STAGE);
    pvpanic_trigger();
}
