#include "avp/devices/simplevirtio/simple_virtio_usb_adapter.h"


static bool tuple_contains(
    SimpleVirtioUSBTuple tuple,
    uintptr_t expected_type,
    size_t size
) {
    const uintptr_t current = (uintptr_t)tuple.current;
    return tuple.type == expected_type && tuple.current != 0 &&
        tuple.lower_bound <= current && current < tuple.upper_bound &&
        size <= tuple.upper_bound - current;
}

uintptr_t simple_virtio_bus_base(void) {
    return SIMPLE_VIRTIO_USB_BUS_BASE;
}

uint32_t simple_virtio_usb_slot(void) {
    return SIMPLE_VIRTIO_USB_SLOT;
}

uintptr_t simple_virtio_usb_mmio_base(void) {
    return simple_virtio_bus_base() +
        (uintptr_t)simple_virtio_usb_slot() * SIMPLE_VIRTIO_USB_SLOT_STRIDE;
}

bool simple_virtio_usb_endpoint_queue_initialize(
    SimpleVirtioUSBAsync *usb,
    uint32_t queue_index,
    uint8_t endpoint
) {
    if (usb == 0 ||
        queue_index >= SIMPLE_VIRTIO_USB_ASYNC_COMPLETION_SLOT_COUNT) {
        return false;
    }
    SimpleVirtioUSBAsyncCompletionSlot *slot =
        &usb->completion_slots[queue_index];
    *slot = (SimpleVirtioUSBAsyncCompletionSlot){
        .endpoint = endpoint,
    };
    slot->tail_cursor = (SimpleVirtioUSBAsyncOwnedReference){
        .pointer = &slot->head,
        .lower_bound = (uintptr_t)&slot->head,
        .upper_bound = (uintptr_t)&slot->head + sizeof(slot->head),
        .cookie = (uintptr_t)slot,
    };
    return true;
}

bool simple_virtio_usb_validate_controller_tuple(SimpleVirtioUSBTuple tuple) {
    return tuple_contains(
        tuple,
        SIMPLE_VIRTIO_USB_TUPLE_CONTROLLER,
        sizeof(SimpleVirtioUSBAsync)
    );
}

bool simple_virtio_usb_validate_device_tuple(SimpleVirtioUSBTuple tuple) {
    return tuple_contains(
        tuple,
        SIMPLE_VIRTIO_USB_TUPLE_DEVICE,
        sizeof(uintptr_t)
    );
}

bool simple_virtio_usb_validate_queue_tuple(SimpleVirtioUSBTuple tuple) {
    return tuple_contains(
        tuple,
        SIMPLE_VIRTIO_USB_TUPLE_QUEUE,
        sizeof(SimpleVirtioUSBAsyncCompletionSlot)
    );
}

bool simple_virtio_usb_validate_buffer_tuple(SimpleVirtioUSBTuple tuple) {
    return tuple_contains(tuple, SIMPLE_VIRTIO_USB_TUPLE_BUFFER, 1);
}

bool simple_virtio_usb_validate_completion_tuple(SimpleVirtioUSBTuple tuple) {
    return tuple_contains(
        tuple,
        SIMPLE_VIRTIO_USB_TUPLE_COMPLETION,
        sizeof(SimpleVirtioUSBAsyncCompletionRecord)
    );
}

SimpleVirtioUSBTuple simple_virtio_usb_outlined_load_device_bounds_x19(
    SimpleVirtioUSBTuple tuple
) {
    return tuple;
}

SimpleVirtioUSBTuple simple_virtio_usb_outlined_load_device_bounds_x25(
    SimpleVirtioUSBTuple tuple
) {
    return tuple;
}

SimpleVirtioUSBTuple simple_virtio_usb_outlined_load_device_bounds_x21(
    SimpleVirtioUSBTuple tuple
) {
    return tuple;
}

bool usb_event_outlined_prepare_payload_dispatch_arguments(
    const uint8_t *event,
    uint32_t event_size,
    SimpleVirtioUSBPayloadDispatchArguments *arguments
) {
    if (event == 0 || arguments == 0 || event_size < 6) {
        return false;
    }
    const uint32_t declared_length = (uint32_t)event[0] |
        (uint32_t)event[1] << 8 |
        (uint32_t)event[2] << 16 |
        (uint32_t)event[3] << 24;
    if (declared_length != event_size - 6) {
        return false;
    }
    *arguments = (SimpleVirtioUSBPayloadDispatchArguments){
        .declared_length = declared_length,
        .endpoint = event[4],
        .type = event[5],
        .payload = {
            .current = (void *)(event + 6),
            .lower_bound = (uintptr_t)(event + 6),
            .upper_bound = (uintptr_t)event + event_size,
            .type = SIMPLE_VIRTIO_USB_TUPLE_BUFFER,
        },
    };
    return true;
}
