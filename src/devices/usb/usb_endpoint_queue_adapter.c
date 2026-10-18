#include "avp/devices/usb/usb_endpoint_queue_adapter.h"


static bool tuple_contains(USBEndpointTuple tuple, size_t required_size) {
    const uintptr_t current = (uintptr_t)tuple.current;
    return tuple.lower_bound <= current &&
        current <= tuple.upper_bound &&
        required_size <= tuple.upper_bound - current;
}

USBEndpointTuple usb_transfer_outlined_publish_state_and_operations_span(
    USBEndpointController *controller,
    uint8_t state,
    void *operations
) {
    controller->header[3] = state;
    return (USBEndpointTuple){
        .current = operations,
        .lower_bound = (uintptr_t)operations,
        .upper_bound = (uintptr_t)operations + USB_TRANSFER_OPERATIONS_SIZE,
        .type = USB_ENDPOINT_TUPLE_OPERATIONS,
    };
}

bool usb_transfer_validate_byte_span(USBEndpointTuple tuple) {
    return tuple.type == USB_ENDPOINT_TUPLE_BYTES && tuple_contains(tuple, 0);
}

bool usb_queued_payload_node_tuple_validate(USBEndpointTuple tuple) {
    return tuple.type == USB_ENDPOINT_TUPLE_PAYLOAD_NODE &&
        tuple_contains(tuple, USB_QUEUED_PAYLOAD_NODE_SIZE);
}

bool usb_device_validate_controller_tuple(USBEndpointTuple tuple) {
    return tuple.type == USB_ENDPOINT_TUPLE_CONTROLLER &&
        tuple_contains(tuple, sizeof(USBEndpointController));
}

USBEndpointTuple usb_device_outlined_global_transfer_span(
    const USBEndpointController *controller
) {
    return controller->global_transfer_span;
}

static uint32_t endpoint_record_index(uint8_t endpoint_address) {
    return (uint32_t)tu_edpt_number(endpoint_address) * 2u +
        (tu_edpt_dir(endpoint_address) == TUSB_DIR_OUT ? 1u : 0u);
}

uint32_t usb_endpoint_outlined_select_record_index_and_stride_w0_w8(
    uint8_t endpoint_address,
    uint32_t *stride
) {
    *stride = USB_ENDPOINT_RECORD_STRIDE;
    return endpoint_record_index(endpoint_address);
}

bool usb_ep0_outlined_create_setup_request(
    const uint8_t *event,
    size_t event_size,
    USBEP0SetupCompletion completion,
    void *completion_context,
    USBEP0SetupRequestArguments *arguments
) {
    if (event_size < 6 + sizeof(tusb_control_request_t)) {
        return false;
    }
    arguments->endpoint = 0;
    const uint8_t *const setup = event + 6;
    for (size_t index = 0; index < sizeof(arguments->setup); ++index) {
        ((uint8_t *)&arguments->setup)[index] = setup[index];
    }
    arguments->setup_span = (USBEndpointTuple){
        .current = (void *)setup,
        .lower_bound = (uintptr_t)setup,
        .upper_bound = (uintptr_t)setup + sizeof(tusb_control_request_t),
        .type = USB_ENDPOINT_TUPLE_BYTES,
    };
    arguments->completion = completion;
    arguments->completion_context = completion_context;
    return true;
}

USBEndpointRecord *usb_endpoint_outlined_resolve_record_x8_x9(
    USBEndpointController *controller,
    uint32_t record_index,
    uint32_t stride
) {
    if (stride != USB_ENDPOINT_RECORD_STRIDE ||
        record_index >= USB_ENDPOINT_RECORD_COUNT) {
        return 0;
    }
    return (USBEndpointRecord *)(
        (uint8_t *)controller->endpoints + (size_t)record_index * stride
    );
}

static USBEndpointTuple endpoint_array_bounds(USBEndpointController *controller) {
    return (USBEndpointTuple){
        .current = controller->endpoints,
        .lower_bound = (uintptr_t)controller->endpoints,
        .upper_bound = (uintptr_t)controller->endpoints +
            sizeof(controller->endpoints),
        .type = USB_ENDPOINT_TUPLE_CONTROLLER,
    };
}

USBEndpointTuple usb_endpoint_outlined_load_array_bounds_x19(
    USBEndpointController *controller
) {
    return endpoint_array_bounds(controller);
}

uint32_t usb_endpoint_outlined_select_record_index_and_stride_w8_w9(
    uint8_t endpoint_address,
    uint32_t *stride
) {
    return usb_endpoint_outlined_select_record_index_and_stride_w0_w8(
        endpoint_address,
        stride
    );
}

USBEndpointTuple usb_endpoint_outlined_load_controller_view(
    USBEndpointController *controller
) {
    return endpoint_array_bounds(controller);
}

bool usb_outlined_stack_guard_compare_saved_x8(
    const USBEndpointController *controller,
    uint64_t saved_guard
) {
    return controller->stack_guard == saved_guard;
}

USBTransportEventArguments
usb_device_outlined_prepare_transport_event_arguments(
    USBEndpointController *controller,
    uintptr_t endpoint,
    uintptr_t event,
    uintptr_t payload_size,
    uintptr_t completion
) {
    return (USBTransportEventArguments){
        .transfer_state = {
            .current = &controller->global_transfer_span,
            .lower_bound = (uintptr_t)&controller->global_transfer_span,
            .upper_bound = (uintptr_t)&controller->endpoints[0] + 0x20,
            .type = USB_ENDPOINT_TUPLE_CONTROLLER,
        },
        .endpoint = endpoint,
        .event = event,
        .payload_size = payload_size,
        .completion = completion,
    };
}

void usb_transfer_outlined_move_and_clear_next_span(
    USBEndpointTransferRecord *source,
    USBEndpointQueueState *destination
) {
    destination->next_span = source->next_span;
    source->next_span = (USBEndpointTuple){0};
}

USBEndpointTuple usb_endpoint_outlined_load_array_bounds_x8(
    USBEndpointController *controller
) {
    return endpoint_array_bounds(controller);
}

USBEndpointRecord *usb_ep0_outlined_set_in_phase_one_advance_to_out_x9(
    USBEndpointRecord *ep0_in
) {
    ep0_in->phase = 1;
    return ep0_in + 1;
}

uint64_t usb_outlined_stack_guard_load_to_x8(
    const USBEndpointController *controller
) {
    return controller->stack_guard;
}

bool usb_endpoint_outlined_compare_candidate_to_array_lower_x19(
    const USBEndpointController *controller,
    const USBEndpointRecord *candidate
) {
    return candidate >= &controller->endpoints[0];
}

USBEndpointRecord *usb_ep0_outlined_set_in_phase_advance_to_out_x8(
    USBEndpointRecord *ep0_in,
    uint32_t phase
) {
    ep0_in->phase = phase;
    return ep0_in + 1;
}
