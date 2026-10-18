#include "avp/runtime/ipc/runtime_service.h"
#include "avp/runtime/ipc/runtime_svc.h"
#include "avp/devices/simplevirtio/simple_virtio.h"
#include "avp/devices/simplevirtio/simple_virtio_service.h"


enum {
    SIMPLE_VIRTIO_STORAGE_ERROR_INVALID_ARGUMENT = 22,
    SIMPLE_VIRTIO_SERVICE_PACKET_IPA = 0x70050000,
};

typedef struct {
    uint64_t sector_count;
    uint32_t block_size;
    bool valid;
} SimpleVirtioProxyGeometry;

static SimpleVirtioProxyGeometry proxy_geometry[2];

static uintptr_t simple_virtio_block_base(SimpleVirtioBlockSlot slot) {
    return slot == SIMPLE_VIRTIO_BLOCK_SLOT_ROOT
        ? SIMPLE_VIRTIO_ROOT_BLOCK_BASE
        : SIMPLE_VIRTIO_UTIL_BLOCK_BASE;
}

static bool proxy_transfer(
    const SimpleVirtioBlock *block,
    uint64_t block_index,
    uint32_t block_count,
    const void *buffer,
    uint64_t method
) {
    SourceSimpleVirtioStorageTransferPacket packet = {
        .block_index = block_index,
        .buffer = (uintptr_t)buffer,
        .block_count = block_count,
        .status = SIMPLE_VIRTIO_STORAGE_ERROR_INVALID_ARGUMENT,
        .slot = block->slot,
    };
    (void)runtime_service_call(
        AVP_RUNTIME_SERVICE_STORAGE,
        &packet,
        sizeof(packet),
        method,
        0
    );
    return packet.status == 0;
}

static bool simple_virtio_proxy_read(
    const SimpleVirtioBlock *block,
    uint64_t block_index,
    uint32_t block_count,
    void *buffer
) {
    return proxy_transfer(block, block_index, block_count, buffer, AVP_SOURCE_STORAGE_METHOD_READ);
}

static bool simple_virtio_proxy_write(
    const SimpleVirtioBlock *block,
    uint64_t block_index,
    uint32_t block_count,
    const void *buffer
) {
    return proxy_transfer(block, block_index, block_count, buffer, AVP_SOURCE_STORAGE_METHOD_WRITE);
}

static const SimpleVirtioBlockOps proxy_block_ops = {
    .read = simple_virtio_proxy_read,
    .write = simple_virtio_proxy_write,
};

bool simple_virtio_block_domain_initialize(
    SimpleVirtioBlock *block,
    SimpleVirtioBlockSlot slot
) {
    if (block == 0 || slot > SIMPLE_VIRTIO_BLOCK_SLOT_UTIL) {
        return false;
    }
    if (runtime_domain_query_locality(
            AVP_RUNTIME_DOMAIN_SIMPLE_VIRTIO
        )) {
        return simple_virtio_block_initialize(
            block,
            simple_virtio_block_base(slot)
        );
    }

    RuntimeStorageInitializeRPC packet;
    runtime_rpc_build_storage_initialize(
        &packet,
        slot == SIMPLE_VIRTIO_BLOCK_SLOT_ROOT ? "root" : "util",
        slot
    );
    (void)runtime_service_call(
            AVP_RUNTIME_SERVICE_STORAGE,
            &packet,
            sizeof(packet),
            SIMPLE_VIRTIO_STORAGE_METHOD_INITIALIZE,
            3
        );
    if (packet.result != 0 || !proxy_geometry[slot].valid) {
        return false;
    }
    *block = (SimpleVirtioBlock){
        .sector_count = proxy_geometry[slot].sector_count,
        .block_size = proxy_geometry[slot].block_size,
        .ops = &proxy_block_ops,
        .slot = slot,
    };
    return true;
}

static void simple_virtio_storage_service_method(
    void *handler_context,
    uint16_t method,
    uint16_t encoding_depth,
    void *raw_packet,
    uint32_t packet_size
) {
    SimpleVirtioStorageService *service = handler_context;
    if (method == SIMPLE_VIRTIO_STORAGE_METHOD_INITIALIZE) {
        RuntimeStorageInitializeRPC *packet = raw_packet;
        if (encoding_depth != 3 || packet_size != sizeof(*packet) ||
            !runtime_rpc_validate_storage_initialize(packet) ||
            packet->slot > SIMPLE_VIRTIO_BLOCK_SLOT_UTIL) {
            __builtin_trap();
        }
        const SimpleVirtioBlockSlot slot = packet->slot;
        const bool domain_matches = slot == SIMPLE_VIRTIO_BLOCK_SLOT_ROOT
            ? packet->domain[0] == 'r' && packet->domain[1] == 'o' &&
                packet->domain[2] == 'o' && packet->domain[3] == 't'
            : packet->domain[0] == 'u' && packet->domain[1] == 't' &&
                packet->domain[2] == 'i' && packet->domain[3] == 'l';
        ++service->initialize_count;
        service->initialized[slot] = domain_matches &&
            simple_virtio_block_initialize(
                &service->blocks[slot],
                simple_virtio_block_base(slot)
            );
        packet->result = service->initialized[slot]
            ? 0
            : SIMPLE_VIRTIO_STORAGE_ERROR_INVALID_ARGUMENT;
        if (service->initialized[slot]) {
            proxy_geometry[slot] = (SimpleVirtioProxyGeometry){
                .sector_count = service->blocks[slot].sector_count,
                .block_size = service->blocks[slot].block_size,
                .valid = true,
            };
        }
        return;
    }

    SourceSimpleVirtioStorageTransferPacket *packet = raw_packet;
    if (encoding_depth != 0 || packet_size != sizeof(*packet) ||
        packet->slot > SIMPLE_VIRTIO_BLOCK_SLOT_UTIL ||
        !service->initialized[packet->slot]) {
        __builtin_trap();
    }
    const SimpleVirtioBlock *device = &service->blocks[packet->slot];
    bool completed = false;
    if (method == AVP_SOURCE_STORAGE_METHOD_READ) {
        ++service->read_count;
        completed = simple_virtio_block_read(
            device,
            packet->block_index,
            packet->block_count,
            (void *)(uintptr_t)packet->buffer
        );
    } else if (method == AVP_SOURCE_STORAGE_METHOD_WRITE) {
        ++service->write_count;
        completed = simple_virtio_block_write(
            device,
            packet->block_index,
            packet->block_count,
            (const void *)(uintptr_t)packet->buffer
        );
    } else {
        __builtin_trap();
    }
    packet->status = completed ? 0 : SIMPLE_VIRTIO_STORAGE_ERROR_INVALID_ARGUMENT;
}

bool simple_virtio_storage_service_register(
    RuntimeServiceOwner *owner,
    SimpleVirtioStorageService *service,
    uintptr_t stack_top
) {
    *service = (SimpleVirtioStorageService){0};
    for (uint32_t slot = 0; slot < 2; ++slot) {
        proxy_geometry[slot] = (SimpleVirtioProxyGeometry){0};
    }
    return runtime_domain_register(
            &service->domain,
            AVP_RUNTIME_DOMAIN_SIMPLE_VIRTIO,
            AVP_RUNTIME_DOMAIN_BOOT,
            3
        ) &&
        runtime_service_owner_initialize(
            owner,
            &service->domain,
            stack_top,
            (void *)SIMPLE_VIRTIO_SERVICE_PACKET_IPA
        ) &&
        runtime_service_register(
            owner,
            AVP_RUNTIME_SERVICE_STORAGE,
            simple_virtio_storage_service_method,
            service
        ) &&
        runtime_service_owner_start(owner);
}
