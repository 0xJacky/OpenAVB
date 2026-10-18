#include "avp/devices/simplevirtio/simple_virtio_storage_adapter.h"


static bool bounds_contain(
    uintptr_t lower_bound,
    uintptr_t upper_bound,
    uintptr_t pointer,
    size_t size
) {
    return lower_bound <= pointer && pointer <= upper_bound &&
        size <= upper_bound - pointer;
}

bool simple_virtio_block_device_initialize(
    SimpleVirtioBlock *block,
    SimpleVirtioBlockSlot slot
) {
    if (block == 0 || slot > SIMPLE_VIRTIO_BLOCK_SLOT_UTIL) {
        return false;
    }
    const uintptr_t base = SIMPLE_VIRTIO_ROOT_BLOCK_BASE +
        ((uintptr_t)slot << 16);
    if (!simple_virtio_block_initialize(block, base)) {
        return false;
    }
    block->slot = (uint8_t)slot;
    return true;
}

bool simple_virtio_block_validate_device_tuple(
    const SimpleVirtioBlockDeviceTuple *tuple
) {
    if (tuple == 0 || tuple->pointer == 0 ||
        tuple->type != SIMPLE_VIRTIO_BLOCK_DEVICE_ID) {
        return false;
    }
    return bounds_contain(
        tuple->lower_bound,
        tuple->upper_bound,
        (uintptr_t)tuple->pointer,
        sizeof(*tuple->pointer)
    );
}

bool simple_virtio_block_validate_device_tuple_thunk(
    const SimpleVirtioBlockDeviceTuple *tuple
) {
    return simple_virtio_block_validate_device_tuple(tuple);
}

bool simple_virtio_block_validate_device_tuple_thunk_clone(
    const SimpleVirtioBlockDeviceTuple *tuple
) {
    return simple_virtio_block_validate_device_tuple(tuple);
}

SimpleVirtioBlockDeviceTuple simple_virtio_block_outlined_device_span_x26(
    SimpleVirtioBlock *device,
    uintptr_t upper_bound,
    uint32_t type
) {
    return (SimpleVirtioBlockDeviceTuple){
        .pointer = device,
        .lower_bound = (uintptr_t)device,
        .upper_bound = upper_bound,
        .type = type,
    };
}

SimpleVirtioBlockProviderSpan simple_virtio_block_outlined_provider_span_x19_x28(
    void *provider,
    uintptr_t upper_bound
) {
    return (SimpleVirtioBlockProviderSpan){
        .pointer = provider,
        .lower_bound = (uintptr_t)provider,
        .upper_bound = upper_bound,
    };
}

bool simple_virtio_block_outlined_update_sector_state(
    const SimpleVirtioBlock *block,
    uint32_t block_index,
    SimpleVirtioBlockHeader *header
) {
    if (block == 0 || header == 0 || block->block_size == 0 ||
        (block->block_size & 0x1ffu) != 0) {
        return false;
    }
    header->sector = (uint64_t)block_index * (block->block_size >> 9);
    return true;
}

void simple_virtio_block_outlined_frame70_save_x19_x28(
    SimpleVirtioBlockFrame70 *frame,
    const uintptr_t registers[10]
) {
    for (size_t index = 0; index < 10; ++index) {
        frame->registers[index] = registers[index];
    }
}

void simple_virtio_block_outlined_frame70_restore_x19_x28(
    const SimpleVirtioBlockFrame70 *frame,
    uintptr_t registers[10]
) {
    for (size_t index = 0; index < 10; ++index) {
        registers[index] = frame->registers[index];
    }
}

bool simple_virtio_block_outlined_capture_buffer_span_and_device(
    const SimpleVirtioBlockDeviceTuple *device,
    const SimpleVirtioBufferSpan *buffer,
    SimpleVirtioBlockDispatchCapture *capture
) {
    if (!simple_virtio_block_validate_device_tuple_thunk_clone(device) ||
        buffer == 0 || capture == 0 || buffer->pointer == 0 ||
        !bounds_contain(
            buffer->lower_bound,
            buffer->upper_bound,
            (uintptr_t)buffer->pointer,
            1
        )) {
        return false;
    }
    capture->device = *device;
    capture->buffer = *buffer;
    return true;
}

int simple_virtio_storage_initialize_remote(
    SimpleVirtioStorageRemoteCall call,
    void *context,
    const char domain[SIMPLE_VIRTIO_STORAGE_DOMAIN_SIZE],
    SimpleVirtioBlockSlot slot
) {
    if (call == 0 || domain == 0 || slot > SIMPLE_VIRTIO_BLOCK_SLOT_UTIL) {
        return SIMPLE_VIRTIO_STORAGE_ADAPTER_INVALID_ARGUMENT;
    }
    SimpleVirtioStorageInitializePacket request = {
        .result = 0,
        .slot = (uint8_t)slot,
    };
    for (size_t index = 0; index < SIMPLE_VIRTIO_STORAGE_DOMAIN_SIZE; ++index) {
        request.domain[index] = domain[index];
    }
    SimpleVirtioStorageInitializePacket response = {0};
    const int status = call(
        context,
        SIMPLE_VIRTIO_STORAGE_SVC,
        SIMPLE_VIRTIO_STORAGE_INITIALIZE_METHOD,
        &request,
        &response
    );
    if (status != 0) {
        return status;
    }
    if (response.result != 0) {
        return SIMPLE_VIRTIO_STORAGE_ADAPTER_RESPONSE_ERROR;
    }
    return SIMPLE_VIRTIO_STORAGE_ADAPTER_SUCCESS;
}

SimpleVirtioStorageBuilderSpan simple_virtio_storage_outlined_builder_span_x28(
    SimpleVirtioStorageRPCBuilder *builder
) {
    return (SimpleVirtioStorageBuilderSpan){
        .pointer = builder,
        .lower_bound = (uintptr_t)builder,
        .upper_bound = (uintptr_t)builder + sizeof(*builder),
    };
}

SimpleVirtioStorageU32Field simple_virtio_storage_outlined_prepare_u32_field(
    const void *field_type
) {
    return (SimpleVirtioStorageU32Field){
        .type = field_type,
        .alignment = 4,
        .size = 4,
        .pointer_offset = 0,
    };
}
