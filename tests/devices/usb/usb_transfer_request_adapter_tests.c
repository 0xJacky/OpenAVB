#include "avp/devices/usb/usb_transfer_request_adapter.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>


static struct {
    bool result;
    unsigned calls;
    uint8_t rhport;
    uint8_t endpoint_address;
    uint8_t *buffer;
    uint16_t length;
    bool is_isr;
} transfer_probe;

static struct {
    unsigned calls;
    USBTransferRequestSpan request;
} completion_probe;

bool usbd_edpt_xfer(
    uint8_t rhport,
    uint8_t endpoint_address,
    uint8_t *buffer,
    uint16_t total_bytes,
    bool is_isr
) {
    ++transfer_probe.calls;
    transfer_probe.rhport = rhport;
    transfer_probe.endpoint_address = endpoint_address;
    transfer_probe.buffer = buffer;
    transfer_probe.length = total_bytes;
    transfer_probe.is_isr = is_isr;
    return transfer_probe.result;
}

static void completion_callback(
    USBTransferRequest *current,
    uintptr_t lower_bound,
    uintptr_t upper_bound,
    uintptr_t type
) {
    ++completion_probe.calls;
    completion_probe.request = (USBTransferRequestSpan){
        .current = current,
        .lower_bound = lower_bound,
        .upper_bound = upper_bound,
        .type = type,
    };
}

static USBTransferBufferSpan byte_span(uint8_t *bytes, size_t size) {
    return (USBTransferBufferSpan){
        .current = bytes,
        .lower_bound = (uintptr_t)bytes,
        .upper_bound = (uintptr_t)bytes + size,
        .type = USB_DESCRIPTOR_SPAN_TYPE_BYTE,
    };
}

static void test_allocation_layout_and_tail(void) {
    USBTransferRequest records[2];
    uint8_t in_use[2] = {0};
    USBTransferRequestPool pool = {records, in_use, 2};
    USBTransferRequestSpan first = usb_transfer_request_allocate_one(&pool);
    assert(usb_transfer_request_validate_tuple(first));
    assert(first.current == &records[0]);
    assert(first.upper_bound - first.lower_bound == USB_TRANSFER_REQUEST_SIZE);
    first.current->next.current = &records[1];
    first = usb_transfer_outlined_initialize_request_tail(
        first,
        0x1234,
        completion_callback,
        0x146c50
    );
    assert(first.current->requested_length == 0x1234);
    assert(first.current->transfer_state == 0);
    assert(first.current->completion == completion_callback);
    assert(first.current->completion_type == 0x146c50);
    assert(first.current->next.current == 0);
    assert(usb_transfer_outlined_restore_request_span_x22_x19(first).current ==
        first.current);

    USBTransferRequestSpan second = usb_transfer_request_allocate_one(&pool);
    assert(second.current == &records[1]);
    assert(usb_transfer_request_allocate_one(&pool).current == 0);
    second.type = 0;
    assert(!usb_transfer_request_validate_tuple(second));
}

static void test_tinyusb_submission_and_failure_release(void) {
    USBTransferRequest records[1];
    uint8_t in_use[1] = {0};
    USBTransferRequestPool pool = {records, in_use, 1};
    uint8_t payload[32];
    USBTransferRequestSpan submitted;

    memset(&transfer_probe, 0, sizeof(transfer_probe));
    transfer_probe.result = true;
    assert(usb_transfer_request_create_and_submit(
        &pool,
        0x81,
        byte_span(payload, sizeof(payload)),
        24,
        completion_callback,
        0x146c50,
        &submitted
    ));
    assert(submitted.current == &records[0]);
    assert(submitted.current->endpoint_address == 0x81);
    assert(submitted.current->payload.current == payload);
    assert(transfer_probe.calls == 1);
    assert(transfer_probe.rhport == 0);
    assert(transfer_probe.endpoint_address == 0x81);
    assert(transfer_probe.buffer == payload);
    assert(transfer_probe.length == 24);
    assert(!transfer_probe.is_isr);

    USBTransferCompletionState controller = {0};
    controller.ep0_in_phase = -1;
    assert(usb_transfer_request_complete_and_release(
        &pool,
        submitted,
        &controller
    ));
    assert(completion_probe.calls == 1);
    assert(completion_probe.request.current == &records[0]);
    assert(in_use[0] == 0);

    memset(&transfer_probe, 0, sizeof(transfer_probe));
    transfer_probe.result = false;
    assert(!usb_transfer_request_create_and_submit(
        &pool,
        0x02,
        byte_span(payload, sizeof(payload)),
        sizeof(payload),
        0,
        0,
        &submitted
    ));
    assert(transfer_probe.calls == 1);
    assert(in_use[0] == 0);
    assert(submitted.current == 0);

    memset(&transfer_probe, 0, sizeof(transfer_probe));
    transfer_probe.result = true;
    assert(!usb_transfer_request_create_and_submit(
        &pool,
        0x81,
        byte_span(payload, sizeof(payload)),
        UINT16_MAX + 1u,
        0,
        0,
        &submitted
    ));
    assert(transfer_probe.calls == 0);
}

static void test_ep0_completion_gate(void) {
    USBTransferRequest records[1];
    uint8_t in_use[1] = {0};
    USBTransferRequestPool pool = {records, in_use, 1};
    USBTransferRequestSpan request = usb_transfer_request_allocate_one(&pool);
    request.current->endpoint_address = 0x80;
    request = usb_transfer_outlined_initialize_request_tail(
        request,
        0,
        0,
        0
    );
    USBTransferCompletionState controller = {0};
    controller.ep0_in_phase = 0;
    assert(usb_transfer_request_complete_and_release(
        &pool,
        request,
        &controller
    ));
    assert(controller.ep0_in_completion == 1);

    request = usb_transfer_request_allocate_one(&pool);
    request.current->endpoint_address = 0x80;
    controller.ep0_in_completion = 0;
    controller.ep0_in_phase = -1;
    assert(usb_transfer_request_complete_and_release(
        &pool,
        request,
        &controller
    ));
    assert(controller.ep0_in_completion == 0);
}

static void test_ep0_spans_and_callbacks(void) {
    USBTransferEP0State state = {0};
    uint8_t payload[16];
    state.payload = (USBDescriptorSpan){
        .current = payload + 3,
        .lower_bound = payload,
        .upper_bound = payload + sizeof(payload),
        .type = USB_DESCRIPTOR_SPAN_TYPE_BYTE,
    };
    USBTransferSpanBounds bounds =
        usb_ep0_outlined_load_payload_bounds(&state);
    assert(bounds.current == (uintptr_t)(payload + 3));
    assert(bounds.lower_bound == (uintptr_t)payload);
    assert(bounds.upper_bound == (uintptr_t)(payload + sizeof(payload)));
    assert(bounds.current_at_or_above_lower);
    USBDescriptorSpan loaded = usb_ep0_outlined_load_payload_span(&state);
    assert(memcmp(&loaded, &state.payload, sizeof(loaded)) == 0);

    USBDescriptorSpan setup =
        usb_ep0_outlined_setup_request_span_x25(&state);
    assert(setup.current == (uint8_t *)&state.setup);
    assert(setup.upper_bound - setup.lower_bound ==
        sizeof(tusb_control_request_t));

    state.endpoint_callbacks[0] = (USBTransferCallbackSpan){
        .current = payload,
        .lower_bound = (uintptr_t)payload,
        .upper_bound = (uintptr_t)(payload + sizeof(payload)),
        .type = 9,
    };
    USBTransferCallbackSpan *record =
        usb_ep0_outlined_select_endpoint_callback_record(&state, 0);
    assert(record == &state.endpoint_callbacks[0]);
    USBTransferCallbackSpan callback =
        usb_ep0_outlined_load_endpoint_callback_span_x9(record);
    assert(memcmp(&callback, record, sizeof(callback)) == 0);
    assert(usb_ep0_outlined_select_endpoint_callback_record(&state, 2) == 0);

    USBTransferRequestSpan empty = usb_ep0_outlined_clear_transfer_span();
    assert(empty.current == 0 && empty.lower_bound == 0 &&
        empty.upper_bound == 0 && empty.type == 0);
}

static void test_configuration_descriptor_contracts(void) {
    tusb_desc_configuration_t descriptor = {
        .bLength = sizeof(descriptor),
        .bDescriptorType = TUSB_DESC_CONFIGURATION,
        .wTotalLength = 0x180,
        .bNumInterfaces = 1,
        .bConfigurationValue = 1,
        .bmAttributes = 0x80,
        .bMaxPower = 50,
    };
    USBDescriptorSpan span = {
        .current = (uint8_t *)&descriptor,
        .lower_bound = (uint8_t *)&descriptor,
        .upper_bound = (uint8_t *)(&descriptor + 1),
        .type = USB_DESCRIPTOR_SPAN_TYPE_CONFIGURATION,
    };
    assert(usb_ep0_validate_descriptor_span_thunk(span));
    USBTransferClampedLength clamped =
        usb_ep0_outlined_clamp_configuration_descriptor_length(
            &descriptor,
            0x200
        );
    assert(clamped.length == 0x180);
    assert(clamped.exceeds_control_limit);
    clamped = usb_ep0_outlined_clamp_configuration_descriptor_length(
        &descriptor,
        0x40
    );
    assert(clamped.length == 0x40);
    assert(!clamped.exceeds_control_limit);

    descriptor.bDescriptorType = TUSB_DESC_DEVICE;
    assert(!usb_ep0_validate_descriptor_span_thunk(span));
}

int main(void) {
    test_allocation_layout_and_tail();
    test_tinyusb_submission_and_failure_release();
    test_ep0_completion_gate();
    test_ep0_spans_and_callbacks();
    test_configuration_descriptor_contracts();
    puts("usb_transfer_request_adapter_tests: ok");
    return 0;
}
