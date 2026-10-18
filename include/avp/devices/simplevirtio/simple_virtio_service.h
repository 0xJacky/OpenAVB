#pragma once

#include "avp/runtime/ipc/runtime_service.h"
#include "avp/runtime/ipc/runtime_rpc.h"
#include "avp/devices/simplevirtio/simple_virtio.h"

#include <stdbool.h>
#include <stdint.h>


enum {
    SIMPLE_VIRTIO_STORAGE_METHOD_INITIALIZE = 0x18,
    AVP_SOURCE_STORAGE_METHOD_READ = 0x4001,
    AVP_SOURCE_STORAGE_METHOD_WRITE = 0x4002,
};

typedef struct {
    uint64_t block_index;
    uint64_t buffer;
    uint32_t block_count;
    int32_t status;
    uint8_t slot;
    uint8_t reserved[3];
} SourceSimpleVirtioStorageTransferPacket;

_Static_assert(
    sizeof(SourceSimpleVirtioStorageTransferPacket) == 32,
    "SourceSimpleVirtioStorageTransferPacket size"
);

typedef struct {
    RuntimeDomain domain;
    SimpleVirtioBlock blocks[2];
    uint32_t initialize_count;
    uint32_t read_count;
    uint32_t write_count;
    bool initialized[2];
} SimpleVirtioStorageService;

bool simple_virtio_storage_service_register(
    RuntimeServiceOwner *owner,
    SimpleVirtioStorageService *service,
    uintptr_t stack_top
);
