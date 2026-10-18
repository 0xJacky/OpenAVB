#include "avp/devices/simplevirtio/simple_virtio_usb_adapter.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>


static SimpleVirtioUSBTuple tuple_for(
    void *object,
    size_t size,
    SimpleVirtioUSBTupleType type
) {
    return (SimpleVirtioUSBTuple){
        .current = object,
        .lower_bound = (uintptr_t)object,
        .upper_bound = (uintptr_t)object + size,
        .type = type,
    };
}

static void test_mmio_address_contract(void) {
    assert(simple_virtio_bus_base() == 0x30000000);
    assert(simple_virtio_usb_slot() == 16);
    assert(simple_virtio_usb_mmio_base() == 0x30100000);
}

static void test_endpoint_queue_initialize(void) {
    SimpleVirtioUSBAsync usb;
    memset(&usb, 0xa5, sizeof(usb));
    assert(simple_virtio_usb_endpoint_queue_initialize(&usb, 2, 0x85));
    const SimpleVirtioUSBAsyncCompletionSlot *slot = &usb.completion_slots[2];
    assert(slot->endpoint == 0x85);
    assert(slot->pending_credit == 0);
    assert(slot->head.pointer == 0);
    assert(slot->tail_cursor.pointer == &usb.completion_slots[2].head);
    assert(slot->tail_cursor.lower_bound ==
        (uintptr_t)&usb.completion_slots[2].head);
    assert(slot->tail_cursor.upper_bound - slot->tail_cursor.lower_bound ==
        sizeof(slot->head));
    assert(!simple_virtio_usb_endpoint_queue_initialize(&usb, 4, 0));
}

static void test_tuple_validators_and_outliners(void) {
    SimpleVirtioUSBAsync controller = {0};
    uintptr_t device = 0x30100000;
    SimpleVirtioUSBAsyncCompletionSlot queue = {0};
    uint8_t buffer[8] = {0};
    SimpleVirtioUSBAsyncCompletionRecord completion = {0};

    SimpleVirtioUSBTuple tuple = tuple_for(
        &controller,
        sizeof(controller),
        SIMPLE_VIRTIO_USB_TUPLE_CONTROLLER
    );
    assert(simple_virtio_usb_validate_controller_tuple(tuple));
    tuple.upper_bound--;
    assert(!simple_virtio_usb_validate_controller_tuple(tuple));

    tuple = tuple_for(
        &device,
        sizeof(device),
        SIMPLE_VIRTIO_USB_TUPLE_DEVICE
    );
    assert(simple_virtio_usb_validate_device_tuple(tuple));
    SimpleVirtioUSBTuple loaded =
        simple_virtio_usb_outlined_load_device_bounds_x19(tuple);
    assert(memcmp(
        &tuple,
        &loaded,
        sizeof(tuple)
    ) == 0);
    loaded = simple_virtio_usb_outlined_load_device_bounds_x25(tuple);
    assert(memcmp(
        &tuple,
        &loaded,
        sizeof(tuple)
    ) == 0);
    loaded = simple_virtio_usb_outlined_load_device_bounds_x21(tuple);
    assert(memcmp(
        &tuple,
        &loaded,
        sizeof(tuple)
    ) == 0);

    assert(simple_virtio_usb_validate_queue_tuple(tuple_for(
        &queue,
        sizeof(queue),
        SIMPLE_VIRTIO_USB_TUPLE_QUEUE
    )));
    assert(simple_virtio_usb_validate_buffer_tuple(tuple_for(
        buffer,
        sizeof(buffer),
        SIMPLE_VIRTIO_USB_TUPLE_BUFFER
    )));
    assert(simple_virtio_usb_validate_completion_tuple(tuple_for(
        &completion,
        sizeof(completion),
        SIMPLE_VIRTIO_USB_TUPLE_COMPLETION
    )));
    tuple = tuple_for(buffer, sizeof(buffer), SIMPLE_VIRTIO_USB_TUPLE_QUEUE);
    assert(!simple_virtio_usb_validate_buffer_tuple(tuple));
}

static void test_event_payload_arguments(void) {
    const uint8_t event[] = {
        3, 0, 0, 0, 0x04, SIMPLE_VIRTIO_USB_EVENT_TRANSFER,
        0xaa, 0xbb, 0xcc,
    };
    SimpleVirtioUSBPayloadDispatchArguments arguments;
    assert(usb_event_outlined_prepare_payload_dispatch_arguments(
        event,
        sizeof(event),
        &arguments
    ));
    assert(arguments.declared_length == 3);
    assert(arguments.endpoint == 0x04);
    assert(arguments.type == SIMPLE_VIRTIO_USB_EVENT_TRANSFER);
    assert(arguments.payload.current == event + 6);
    assert(arguments.payload.upper_bound == (uintptr_t)event + sizeof(event));
    assert(*(uint8_t *)arguments.payload.current == 0xaa);

    uint8_t corrupt[sizeof(event)];
    memcpy(corrupt, event, sizeof(event));
    corrupt[0] = 4;
    assert(!usb_event_outlined_prepare_payload_dispatch_arguments(
        corrupt,
        sizeof(corrupt),
        &arguments
    ));
}

int main(void) {
    test_mmio_address_contract();
    test_endpoint_queue_initialize();
    test_tuple_validators_and_outliners();
    test_event_payload_arguments();
    puts("simple_virtio_usb_adapter_tests: ok");
    return 0;
}
