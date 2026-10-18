#include "avp/devices/usb/usb_endpoint_queue_adapter.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>


static USBEndpointTuple tuple_for(
    void *object,
    size_t size,
    USBEndpointTupleType type
) {
    return (USBEndpointTuple){
        .current = object,
        .lower_bound = (uintptr_t)object,
        .upper_bound = (uintptr_t)object + size,
        .type = type,
    };
}

static void setup_completion(void *context) {
    (void)context;
}

static void test_tuple_and_operations_contracts(void) {
    USBEndpointController controller = {0};
    uint8_t operations[USB_TRANSFER_OPERATIONS_SIZE];
    USBEndpointTuple tuple =
        usb_transfer_outlined_publish_state_and_operations_span(
            &controller,
            7,
            operations
        );
    assert(controller.header[3] == 7);
    assert(tuple.current == operations);
    assert(tuple.upper_bound - tuple.lower_bound == sizeof(operations));
    assert(tuple.type == USB_ENDPOINT_TUPLE_OPERATIONS);

    uint8_t bytes[USB_QUEUED_PAYLOAD_NODE_SIZE];
    assert(usb_transfer_validate_byte_span(tuple_for(
        bytes,
        sizeof(bytes),
        USB_ENDPOINT_TUPLE_BYTES
    )));
    assert(usb_queued_payload_node_tuple_validate(tuple_for(
        bytes,
        sizeof(bytes),
        USB_ENDPOINT_TUPLE_PAYLOAD_NODE
    )));
    assert(!usb_queued_payload_node_tuple_validate(tuple_for(
        bytes,
        sizeof(bytes) - 1,
        USB_ENDPOINT_TUPLE_PAYLOAD_NODE
    )));
    assert(usb_device_validate_controller_tuple(tuple_for(
        &controller,
        sizeof(controller),
        USB_ENDPOINT_TUPLE_CONTROLLER
    )));
}

static void test_endpoint_record_selection(void) {
    USBEndpointController controller = {0};
    uint32_t stride = 0;
    assert(usb_endpoint_outlined_select_record_index_and_stride_w0_w8(
        0x82,
        &stride
    ) == 4);
    assert(stride == 0x70);
    assert(usb_endpoint_outlined_select_record_index_and_stride_w8_w9(
        0x02,
        &stride
    ) == 5);
    assert(stride == 0x70);
    assert(usb_endpoint_outlined_resolve_record_x8_x9(
        &controller,
        5,
        stride
    ) == &controller.endpoints[5]);
    assert(usb_endpoint_outlined_resolve_record_x8_x9(
        &controller,
        12,
        stride
    ) == 0);

    USBEndpointTuple bounds =
        usb_endpoint_outlined_load_array_bounds_x19(&controller);
    assert(bounds.current == controller.endpoints);
    assert(bounds.lower_bound == (uintptr_t)controller.endpoints);
    assert(bounds.upper_bound - bounds.lower_bound == 12 * 0x70);
    USBEndpointTuple clone =
        usb_endpoint_outlined_load_array_bounds_x8(&controller);
    assert(memcmp(&bounds, &clone, sizeof(bounds)) == 0);
    clone = usb_endpoint_outlined_load_controller_view(&controller);
    assert(memcmp(&bounds, &clone, sizeof(bounds)) == 0);
    assert(usb_endpoint_outlined_compare_candidate_to_array_lower_x19(
        &controller,
        &controller.endpoints[0]
    ));
}

static void test_setup_and_transport_arguments(void) {
    void (*const completion)(void *) = setup_completion;
    void *const completion_context = (void *)(uintptr_t)0x5678;
    const uint8_t event[] = {
        8, 0, 0, 0, 0, 1,
        0x80, 6, 0, 1, 0, 0, 18, 0,
    };
    USBEP0SetupRequestArguments setup;
    assert(usb_ep0_outlined_create_setup_request(
        event,
        sizeof(event),
        completion,
        completion_context,
        &setup
    ));
    assert(setup.endpoint == 0);
    assert(setup.setup.bmRequestType == 0x80);
    assert(setup.setup.bRequest == 6);
    assert(setup.setup.wLength == 18);
    assert(setup.setup_span.current == event + 6);
    assert(setup.completion == completion);
    assert(setup.completion_context == completion_context);
    assert(!usb_ep0_outlined_create_setup_request(
        event,
        sizeof(event) - 1,
        completion,
        completion_context,
        &setup
    ));

    USBEndpointController controller = {0};
    USBTransportEventArguments arguments =
        usb_device_outlined_prepare_transport_event_arguments(
            &controller,
            0x82,
            3,
            512,
            0x1234
        );
    assert(arguments.transfer_state.current ==
        &controller.global_transfer_span);
    assert(arguments.transfer_state.upper_bound -
        arguments.transfer_state.lower_bound == 0x40);
    assert(arguments.endpoint == 0x82);
    assert(arguments.event == 3);
    assert(arguments.payload_size == 512);
    assert(arguments.completion == 0x1234);
}

static void test_span_move_phase_and_guard(void) {
    USBEndpointController controller = {.stack_guard = 0x55667788};
    controller.global_transfer_span = tuple_for(
        &controller,
        sizeof(controller),
        USB_ENDPOINT_TUPLE_CONTROLLER
    );
    USBEndpointTuple loaded =
        usb_device_outlined_global_transfer_span(&controller);
    assert(memcmp(
        &loaded,
        &controller.global_transfer_span,
        sizeof(loaded)
    ) == 0);
    assert(usb_outlined_stack_guard_load_to_x8(&controller) == 0x55667788);
    assert(usb_outlined_stack_guard_compare_saved_x8(
        &controller,
        0x55667788
    ));

    USBEndpointTransferRecord source = {0};
    USBEndpointQueueState destination = {0};
    source.next_span = tuple_for(
        &controller,
        sizeof(controller),
        USB_ENDPOINT_TUPLE_CONTROLLER
    );
    usb_transfer_outlined_move_and_clear_next_span(&source, &destination);
    assert(destination.next_span.current == &controller);
    assert(source.next_span.current == 0);
    assert(source.next_span.upper_bound == 0);

    USBEndpointRecord *out =
        usb_ep0_outlined_set_in_phase_one_advance_to_out_x9(
            &controller.endpoints[0]
        );
    assert(controller.endpoints[0].phase == 1);
    assert(out == &controller.endpoints[1]);
    out = usb_ep0_outlined_set_in_phase_advance_to_out_x8(
        &controller.endpoints[0],
        4
    );
    assert(controller.endpoints[0].phase == 4);
    assert(out == &controller.endpoints[1]);
}

int main(void) {
    test_tuple_and_operations_contracts();
    test_endpoint_record_selection();
    test_setup_and_transport_arguments();
    test_span_move_phase_and_guard();
    puts("usb_endpoint_queue_adapter_tests: ok");
    return 0;
}
