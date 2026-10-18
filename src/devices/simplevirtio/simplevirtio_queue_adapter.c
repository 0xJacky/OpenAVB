#include "avp/devices/simplevirtio/simplevirtio_queue_adapter.h"

#include <stddef.h>


enum {
    SIMPLEVIRTIO_MMIO_VERSION = 0x000,
    SIMPLEVIRTIO_MMIO_DEVICE_ID = 0x004,
    SIMPLEVIRTIO_MMIO_QUEUE_AVAILABLE = 0x010,
    SIMPLEVIRTIO_MMIO_DEVICE_FEATURES = 0x400,
    SIMPLEVIRTIO_MMIO_STATUS = 0x404,
    SIMPLEVIRTIO_MMIO_DESCRIPTOR = 0x408,
    SIMPLEVIRTIO_MMIO_QUEUE_SELECT = 0x410,
};

static bool device_span_valid(SimpleVirtioDeviceSpan span) {
    const uintptr_t device_address = (uintptr_t)span.device;
    return span.device != 0 &&
        span.lower_bound <= device_address &&
        device_address <= UINTPTR_MAX - sizeof(*span.device) &&
        device_address + sizeof(*span.device) <= span.upper_bound;
}

static bool mmio_offset_valid(uint32_t offset) {
    return (offset & 3) == 0 &&
        offset <= SIMPLEVIRTIO_ADAPTER_MMIO_LIMIT - sizeof(uint32_t);
}

SimpleVirtioAdapterStatus simplevirtio_mmio_read32_bounds_failure(
    SimpleVirtioQueueDevice *device
) {
    if (device != 0) {
        device->last_error = SIMPLEVIRTIO_ADAPTER_ERROR_BOUNDS;
    }
    return SIMPLEVIRTIO_ADAPTER_ERROR_BOUNDS;
}

bool simplevirtio_mmio_read32(
    SimpleVirtioDeviceSpan span,
    uint32_t offset,
    uint32_t *value
) {
    if (!device_span_valid(span) || !mmio_offset_valid(offset) ||
        value == 0 || span.device->io.read32 == 0) {
        (void)simplevirtio_mmio_read32_bounds_failure(span.device);
        return false;
    }
    *value = span.device->io.read32(
        span.device->io.context,
        span.device->base,
        offset
    );
    return true;
}

SimpleVirtioAdapterStatus simplevirtio_mmio_write32_bounds_failure(
    SimpleVirtioQueueDevice *device
) {
    if (device != 0) {
        device->last_error = SIMPLEVIRTIO_ADAPTER_ERROR_BOUNDS;
    }
    return SIMPLEVIRTIO_ADAPTER_ERROR_BOUNDS;
}

bool simplevirtio_mmio_write32(
    SimpleVirtioDeviceSpan span,
    uint32_t offset,
    uint32_t value
) {
    if (!device_span_valid(span) || !mmio_offset_valid(offset) ||
        span.device->io.write32 == 0) {
        (void)simplevirtio_mmio_write32_bounds_failure(span.device);
        return false;
    }
    span.device->io.write32(
        span.device->io.context,
        span.device->base,
        offset,
        value
    );
    return true;
}

SimpleVirtioDeviceSpan simplevirtio_outlined_save_device_bounds_x19_x20(
    SimpleVirtioQueueDevice *device,
    uintptr_t lower_bound,
    uintptr_t upper_bound
) {
    return (SimpleVirtioDeviceSpan){device, lower_bound, upper_bound};
}

SimpleVirtioDeviceSpan simplevirtio_outlined_device_span_x21(
    SimpleVirtioDeviceSpan span
) {
    return span;
}

SimpleVirtioDeviceSpan simplevirtio_outlined_device_span_x22(
    SimpleVirtioDeviceSpan span
) {
    return span;
}

bool simplevirtio_queue_allocation_validate(
    const SimpleVirtioQueueDevice *device,
    const SimpleVirtioQueueAllocation *allocation
) {
    if (device == 0 || allocation == 0 || allocation->active == 0 ||
        allocation->descriptor_count > SIMPLEVIRTIO_ADAPTER_MAX_SEGMENTS) {
        return false;
    }
    for (uint8_t index = 0; index < device->queue_count; ++index) {
        if (allocation == &device->queues[index]) {
            return true;
        }
    }
    return false;
}

SimpleVirtioAdapterStatus simplevirtio_device_initialize(
    SimpleVirtioQueueDevice *device,
    const SimpleVirtioAdapterIO *io,
    uintptr_t base,
    uint32_t expected_device_id,
    uint8_t queue_count,
    bool driver_ready,
    const uint64_t descriptor_table_addresses[
        SIMPLEVIRTIO_ADAPTER_MAX_QUEUES
    ]
) {
    if (device == 0 || io == 0 || io->read32 == 0 ||
        io->write32 == 0 || io->write64 == 0 || io->translate == 0 ||
        queue_count > SIMPLEVIRTIO_ADAPTER_MAX_QUEUES ||
        (queue_count != 0 && descriptor_table_addresses == 0)) {
        return SIMPLEVIRTIO_ADAPTER_ERROR_ARGUMENT;
    }
    *device = (SimpleVirtioQueueDevice){
        .io = *io,
        .base = base,
        .queue_count = queue_count,
    };
    const uintptr_t device_address = (uintptr_t)device;
    SimpleVirtioDeviceSpan span =
        simplevirtio_outlined_save_device_bounds_x19_x20(
            device,
            device_address,
            device_address + sizeof(*device)
        );
    uint32_t value = 0;
    if (!simplevirtio_mmio_read32(span, SIMPLEVIRTIO_MMIO_VERSION, &value) ||
        value != 1) {
        device->last_error = SIMPLEVIRTIO_ADAPTER_ERROR_VERSION;
        return device->last_error;
    }
    if (!simplevirtio_mmio_write32(
            span,
            SIMPLEVIRTIO_MMIO_DEVICE_FEATURES,
            1
        ) ||
        !simplevirtio_mmio_read32(
            span,
            SIMPLEVIRTIO_MMIO_DEVICE_FEATURES,
            &value
        ) || value != 1 ||
        !simplevirtio_mmio_read32(span, SIMPLEVIRTIO_MMIO_DEVICE_ID, &value) ||
        value != expected_device_id) {
        device->last_error = SIMPLEVIRTIO_ADAPTER_ERROR_DEVICE;
        return device->last_error;
    }
    for (uint8_t index = 0; index < queue_count; ++index) {
        device->queues[index].descriptor_table_address =
            descriptor_table_addresses[index];
        device->queues[index].active = 1;
    }
    device->status = driver_ready ? 3 : 1;
    if (queue_count != 0 &&
        !simplevirtio_mmio_write32(
            span,
            SIMPLEVIRTIO_MMIO_STATUS,
            device->status
        )) {
        return device->last_error;
    }
    return SIMPLEVIRTIO_ADAPTER_OK;
}

SimpleVirtioAdapterStatus simplevirtio_device_deinitialize(
    SimpleVirtioDeviceSpan span,
    uint32_t poll_limit
) {
    if (!device_span_valid(span)) {
        return simplevirtio_mmio_write32_bounds_failure(span.device);
    }
    for (uint8_t index = 0; index < span.device->queue_count; ++index) {
        span.device->queues[index] = (SimpleVirtioQueueAllocation){0};
    }
    if (!simplevirtio_mmio_write32(span, SIMPLEVIRTIO_MMIO_STATUS, 0)) {
        return span.device->last_error;
    }
    uint32_t status = 0;
    for (uint32_t poll = 0; poll < poll_limit; ++poll) {
        if (!simplevirtio_mmio_read32(
                span,
                SIMPLEVIRTIO_MMIO_STATUS,
                &status
            )) {
            return span.device->last_error;
        }
        if (status == 0) {
            span.device->queue_count = 0;
            span.device->selected_queue = 0;
            span.device->status = 0;
            return SIMPLEVIRTIO_ADAPTER_OK;
        }
    }
    span.device->last_error = SIMPLEVIRTIO_ADAPTER_ERROR_BUSY;
    return span.device->last_error;
}

SimpleVirtioAdapterStatus simplevirtio_queue_select(
    SimpleVirtioDeviceSpan span,
    uint8_t queue_index
) {
    if (!device_span_valid(span)) {
        return simplevirtio_mmio_write32_bounds_failure(span.device);
    }
    if (queue_index >= span.device->queue_count ||
        !simplevirtio_queue_allocation_validate(
            span.device,
            &span.device->queues[queue_index]
        )) {
        span.device->last_error = SIMPLEVIRTIO_ADAPTER_ERROR_QUEUE;
        return span.device->last_error;
    }
    span.device->selected_queue = queue_index;
    if (!simplevirtio_mmio_write32(
            span,
            SIMPLEVIRTIO_MMIO_QUEUE_SELECT,
            queue_index
        )) {
        return span.device->last_error;
    }
    return SIMPLEVIRTIO_ADAPTER_OK;
}

SimpleVirtioAdapterStatus simplevirtio_queue_available(
    SimpleVirtioDeviceSpan span,
    uint8_t queue_index,
    uint32_t *available
) {
    SimpleVirtioAdapterStatus status = simplevirtio_queue_select(
        span,
        queue_index
    );
    if (status != SIMPLEVIRTIO_ADAPTER_OK) {
        return status;
    }
    if (!simplevirtio_mmio_read32(
            span,
            SIMPLEVIRTIO_MMIO_QUEUE_AVAILABLE,
            available
        )) {
        return span.device->last_error;
    }
    return SIMPLEVIRTIO_ADAPTER_OK;
}

SimpleVirtioAdapterStatus simplevirtio_append_descriptor_segment(
    SimpleVirtioDeviceSpan span,
    uint64_t address,
    uint32_t length,
    bool device_writes,
    bool continues
) {
    if (!device_span_valid(span)) {
        return simplevirtio_mmio_write32_bounds_failure(span.device);
    }
    SimpleVirtioQueueAllocation *allocation =
        &span.device->queues[span.device->selected_queue];
    if (!simplevirtio_queue_allocation_validate(span.device, allocation) ||
        allocation->descriptor_count >= SIMPLEVIRTIO_ADAPTER_MAX_SEGMENTS ||
        length == 0) {
        span.device->last_error = SIMPLEVIRTIO_ADAPTER_ERROR_DESCRIPTOR;
        return span.device->last_error;
    }
    allocation->descriptors[allocation->descriptor_count++] =
        (SimpleVirtioSegment){
            .ipa = address,
            .size = length,
            .tag = SIMPLE_VIRTIO_TAG,
            .flags = (uint16_t)(
                (device_writes ? SIMPLE_VIRTIO_WRITABLE : 0) |
                (continues ? SIMPLE_VIRTIO_NEXT : 0)
            ),
        };
    return SIMPLEVIRTIO_ADAPTER_OK;
}

SimpleVirtioAdapterStatus simplevirtio_commit_descriptor(
    SimpleVirtioDeviceSpan span,
    bool wait_for_completion,
    uint32_t poll_limit
) {
    if (!device_span_valid(span)) {
        return simplevirtio_mmio_write32_bounds_failure(span.device);
    }
    SimpleVirtioQueueAllocation *allocation =
        &span.device->queues[span.device->selected_queue];
    if (!simplevirtio_queue_allocation_validate(span.device, allocation) ||
        allocation->descriptor_count == 0) {
        span.device->last_error = SIMPLEVIRTIO_ADAPTER_ERROR_DESCRIPTOR;
        return span.device->last_error;
    }
    for (uint8_t index = 0; index < allocation->descriptor_count; ++index) {
        const bool continues =
            (allocation->descriptors[index].flags & SIMPLE_VIRTIO_NEXT) != 0;
        if (continues != (index + 1 < allocation->descriptor_count)) {
            span.device->last_error = SIMPLEVIRTIO_ADAPTER_ERROR_DESCRIPTOR;
            return span.device->last_error;
        }
        allocation->descriptors[index].ipa = span.device->io.translate(
            span.device->io.context,
            allocation->descriptors[index].ipa
        );
    }
    const uint64_t descriptor_address = span.device->io.translate(
        span.device->io.context,
        allocation->descriptor_table_address
    );
    span.device->io.write64(
        span.device->io.context,
        span.device->base,
        SIMPLEVIRTIO_MMIO_DESCRIPTOR,
        descriptor_address
    );
    if (wait_for_completion) {
        uint32_t available = 0;
        for (uint32_t poll = 0; poll < poll_limit; ++poll) {
            if (!simplevirtio_mmio_read32(
                    span,
                    SIMPLEVIRTIO_MMIO_QUEUE_AVAILABLE,
                    &available
                )) {
                return span.device->last_error;
            }
            if (available != 0) {
                allocation->descriptor_count = 0;
                return SIMPLEVIRTIO_ADAPTER_OK;
            }
        }
        span.device->last_error = SIMPLEVIRTIO_ADAPTER_ERROR_BUSY;
        return span.device->last_error;
    }
    allocation->descriptor_count = 0;
    return SIMPLEVIRTIO_ADAPTER_OK;
}

SimpleVirtioDiagnostic simplevirtio_outlined_log_source_line(uint32_t line) {
    return (SimpleVirtioDiagnostic){simplevirtio_source_token(), line};
}

uint64_t simplevirtio_source_token(void) {
    return SIMPLEVIRTIO_SOURCE_TOKEN;
}
