#pragma once

#include "avp/devices/simplevirtio/simple_virtio.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


#define SIMPLE_VIRTIO_STORAGE_ADAPTER_PUBLIC __attribute__((used, retain))

enum {
    SIMPLE_VIRTIO_BLOCK_DEVICE_ID = 2,
    SIMPLE_VIRTIO_STORAGE_SVC = 0xc2,
    SIMPLE_VIRTIO_STORAGE_INITIALIZE_METHOD = 0x18,
    SIMPLE_VIRTIO_STORAGE_BUILDER_SIZE = 0x28,
    SIMPLE_VIRTIO_STORAGE_DOMAIN_SIZE = 5,
};

typedef enum {
    SIMPLE_VIRTIO_STORAGE_ADAPTER_SUCCESS = 0,
    SIMPLE_VIRTIO_STORAGE_ADAPTER_INVALID_ARGUMENT = 22,
    SIMPLE_VIRTIO_STORAGE_ADAPTER_DEVICE_ERROR = 19,
    SIMPLE_VIRTIO_STORAGE_ADAPTER_BOUNDS_ERROR = 34,
    SIMPLE_VIRTIO_STORAGE_ADAPTER_REMOTE_ERROR = 5,
    SIMPLE_VIRTIO_STORAGE_ADAPTER_RESPONSE_ERROR = 71,
} SimpleVirtioStorageAdapterStatus;

typedef struct {
    SimpleVirtioBlock *pointer;
    uintptr_t lower_bound;
    uintptr_t upper_bound;
    uint32_t type;
} SimpleVirtioBlockDeviceTuple;

typedef struct {
    void *pointer;
    uintptr_t lower_bound;
    uintptr_t upper_bound;
} SimpleVirtioBlockProviderSpan;

typedef struct {
    void *pointer;
    uintptr_t lower_bound;
    uintptr_t upper_bound;
} SimpleVirtioBufferSpan;

typedef struct {
    uintptr_t registers[10];
} SimpleVirtioBlockFrame70;

typedef struct {
    SimpleVirtioBlockDeviceTuple device;
    SimpleVirtioBufferSpan buffer;
} SimpleVirtioBlockDispatchCapture;

typedef struct {
    uint8_t bytes[SIMPLE_VIRTIO_STORAGE_BUILDER_SIZE];
} SimpleVirtioStorageRPCBuilder;

_Static_assert(
    sizeof(SimpleVirtioStorageRPCBuilder) == SIMPLE_VIRTIO_STORAGE_BUILDER_SIZE,
    "SimpleVirtioStorageRPCBuilder size"
);

typedef struct {
    SimpleVirtioStorageRPCBuilder *pointer;
    uintptr_t lower_bound;
    uintptr_t upper_bound;
} SimpleVirtioStorageBuilderSpan;

typedef struct {
    const void *type;
    uint32_t alignment;
    uint32_t size;
    uintptr_t pointer_offset;
} SimpleVirtioStorageU32Field;

typedef struct {
    uint32_t result;
    char domain[SIMPLE_VIRTIO_STORAGE_DOMAIN_SIZE];
    uint8_t slot;
    uint8_t reserved[2];
} SimpleVirtioStorageInitializePacket;

_Static_assert(
    sizeof(SimpleVirtioStorageInitializePacket) == 12,
    "SimpleVirtioStorageInitializePacket size"
);

typedef int (*SimpleVirtioStorageRemoteCall)(
    void *context,
    uint16_t svc,
    uint16_t method,
    const SimpleVirtioStorageInitializePacket *request,
    SimpleVirtioStorageInitializePacket *response
);

SIMPLE_VIRTIO_STORAGE_ADAPTER_PUBLIC bool simple_virtio_block_device_initialize(
    SimpleVirtioBlock *block,
    SimpleVirtioBlockSlot slot
);

SIMPLE_VIRTIO_STORAGE_ADAPTER_PUBLIC bool
simple_virtio_block_validate_device_tuple(
    const SimpleVirtioBlockDeviceTuple *tuple
);

SIMPLE_VIRTIO_STORAGE_ADAPTER_PUBLIC bool
simple_virtio_block_validate_device_tuple_thunk(
    const SimpleVirtioBlockDeviceTuple *tuple
);

SIMPLE_VIRTIO_STORAGE_ADAPTER_PUBLIC bool
simple_virtio_block_validate_device_tuple_thunk_clone(
    const SimpleVirtioBlockDeviceTuple *tuple
);

SIMPLE_VIRTIO_STORAGE_ADAPTER_PUBLIC SimpleVirtioBlockDeviceTuple
simple_virtio_block_outlined_device_span_x26(
    SimpleVirtioBlock *device,
    uintptr_t upper_bound,
    uint32_t type
);

SIMPLE_VIRTIO_STORAGE_ADAPTER_PUBLIC SimpleVirtioBlockProviderSpan
simple_virtio_block_outlined_provider_span_x19_x28(
    void *provider,
    uintptr_t upper_bound
);

SIMPLE_VIRTIO_STORAGE_ADAPTER_PUBLIC bool
simple_virtio_block_outlined_update_sector_state(
    const SimpleVirtioBlock *block,
    uint32_t block_index,
    SimpleVirtioBlockHeader *header
);

SIMPLE_VIRTIO_STORAGE_ADAPTER_PUBLIC void
simple_virtio_block_outlined_frame70_save_x19_x28(
    SimpleVirtioBlockFrame70 *frame,
    const uintptr_t registers[10]
);

SIMPLE_VIRTIO_STORAGE_ADAPTER_PUBLIC void
simple_virtio_block_outlined_frame70_restore_x19_x28(
    const SimpleVirtioBlockFrame70 *frame,
    uintptr_t registers[10]
);

SIMPLE_VIRTIO_STORAGE_ADAPTER_PUBLIC bool
simple_virtio_block_outlined_capture_buffer_span_and_device(
    const SimpleVirtioBlockDeviceTuple *device,
    const SimpleVirtioBufferSpan *buffer,
    SimpleVirtioBlockDispatchCapture *capture
);

SIMPLE_VIRTIO_STORAGE_ADAPTER_PUBLIC int
simple_virtio_storage_initialize_remote(
    SimpleVirtioStorageRemoteCall call,
    void *context,
    const char domain[SIMPLE_VIRTIO_STORAGE_DOMAIN_SIZE],
    SimpleVirtioBlockSlot slot
);

SIMPLE_VIRTIO_STORAGE_ADAPTER_PUBLIC SimpleVirtioStorageBuilderSpan
simple_virtio_storage_outlined_builder_span_x28(
    SimpleVirtioStorageRPCBuilder *builder
);

SIMPLE_VIRTIO_STORAGE_ADAPTER_PUBLIC SimpleVirtioStorageU32Field
simple_virtio_storage_outlined_prepare_u32_field(
    const void *field_type
);
