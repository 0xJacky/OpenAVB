#include "avp/devices/usb/usb_transfer_queue_owner.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>


static struct {
    bool result;
    unsigned calls;
    uint8_t endpoint;
    uint8_t *buffer;
    uint16_t length;
} tinyusb_probe;

static struct {
    unsigned calls;
    int32_t status[USB_TRANSFER_QUEUE_REQUEST_CAPACITY];
    uint32_t actual[USB_TRANSFER_QUEUE_REQUEST_CAPACITY];
} completion_probe;

static struct {
    unsigned calls;
    int32_t status;
    uint32_t actual;
} dispatch_probe;

bool usbd_edpt_xfer(
    uint8_t rhport,
    uint8_t endpoint_address,
    uint8_t *buffer,
    uint16_t total_bytes,
    bool is_isr
) {
    assert(rhport == 0);
    assert(!is_isr);
    ++tinyusb_probe.calls;
    tinyusb_probe.endpoint = endpoint_address;
    tinyusb_probe.buffer = buffer;
    tinyusb_probe.length = total_bytes;
    return tinyusb_probe.result;
}

static USBTransferBufferSpan byte_span(uint8_t *bytes, size_t size) {
    return (USBTransferBufferSpan){
        .current = bytes,
        .lower_bound = (uintptr_t)bytes,
        .upper_bound = (uintptr_t)bytes + size,
        .type = USB_DESCRIPTOR_SPAN_TYPE_BYTE,
    };
}

static void request_complete(
    USBTransferRequest *request,
    uintptr_t lower_bound,
    uintptr_t upper_bound,
    uintptr_t type
) {
    assert(lower_bound == (uintptr_t)request);
    assert(upper_bound == lower_bound + sizeof(*request));
    assert(type == USB_TRANSFER_REQUEST_TYPE);
    const unsigned index = completion_probe.calls++;
    completion_probe.status[index] = request->status;
    completion_probe.actual[index] = request->actual_length;
}

static bool dispatch_complete(
    void *context,
    USBTransferRequestSpan request
) {
    assert(context == &dispatch_probe);
    ++dispatch_probe.calls;
    dispatch_probe.status = request.current->status;
    dispatch_probe.actual = request.current->actual_length;
    return true;
}

static void reset_probes(void) {
    memset(&tinyusb_probe, 0, sizeof(tinyusb_probe));
    memset(&completion_probe, 0, sizeof(completion_probe));
    memset(&dispatch_probe, 0, sizeof(dispatch_probe));
    tinyusb_probe.result = true;
}

static void test_create_submit_fifo_and_worker(void) {
    USBTransferQueueOwner owner;
    uint8_t first_payload[32];
    uint8_t second_payload[16];
    reset_probes();
    usb_transfer_queue_owner_init(
        &owner,
        dispatch_complete,
        &dispatch_probe
    );

    USBTransferRequestSpan first = usb_transfer_request_create(
        &owner,
        0x81,
        byte_span(first_payload, sizeof(first_payload)),
        24,
        request_complete,
        0x146c50
    );
    USBTransferRequestSpan second = usb_transfer_request_create(
        &owner,
        0x81,
        byte_span(second_payload, sizeof(second_payload)),
        sizeof(second_payload),
        request_complete,
        0x146c50
    );
    assert(usb_transfer_submit(&owner, first));
    assert(tinyusb_probe.calls == 1);
    assert(tinyusb_probe.endpoint == 0x81);
    assert(tinyusb_probe.buffer == first_payload);
    assert(tinyusb_probe.length == 24);
    assert(first.current->transfer_state == 1);
    assert(usb_transfer_submit(&owner, second));
    assert(tinyusb_probe.calls == 1);
    assert(second.current->transfer_state == 0);

    assert(usb_endpoint_transfer_worker(&owner, 0x81, 0, 19));
    assert(completion_probe.calls == 1);
    assert(completion_probe.status[0] == 0);
    assert(completion_probe.actual[0] == 19);
    assert(dispatch_probe.calls == 1);
    assert(tinyusb_probe.calls == 2);
    assert(tinyusb_probe.buffer == second_payload);

    assert(usb_endpoint_transfer_worker(&owner, 0x81, -2, 7));
    assert(completion_probe.calls == 2);
    assert(completion_probe.status[1] == -2);
    assert(completion_probe.actual[1] == 7);
    assert(owner.completed_count == 2);
    assert(owner.worker_wake_count == 2);
    assert(!usb_endpoint_transfer_worker(&owner, 0x81, 0, 0));
}

static void test_submit_failure_is_observable_and_released(void) {
    USBTransferQueueOwner owner;
    uint8_t payload[8];
    reset_probes();
    usb_transfer_queue_owner_init(&owner, 0, 0);
    tinyusb_probe.result = false;
    USBTransferRequestSpan request = usb_transfer_request_create(
        &owner,
        0x02,
        byte_span(payload, sizeof(payload)),
        sizeof(payload),
        request_complete,
        0
    );
    assert(!usb_transfer_submit(&owner, request));
    assert(tinyusb_probe.calls == 1);
    assert(completion_probe.calls == 1);
    assert(completion_probe.status[0] == usb_transfer_status_unsupported());
    assert(owner.requests_in_use[0] == 0);
    assert(owner.completed_count == 1);
}

static void test_cancel_active_and_pending_fifo(void) {
    USBTransferQueueOwner owner;
    uint8_t payloads[3][4];
    reset_probes();
    usb_transfer_queue_owner_init(&owner, 0, 0);
    for (size_t index = 0; index < 3; ++index) {
        USBTransferRequestSpan request = usb_transfer_request_create(
            &owner,
            0x83,
            byte_span(payloads[index], sizeof(payloads[index])),
            sizeof(payloads[index]),
            request_complete,
            index
        );
        assert(index == 0 ? usb_transfer_submit(&owner, request) :
            usb_endpoint_payload_queue_append(&owner, request));
    }
    assert(usb_endpoint_cancel_pending_transfers(&owner, 0x83) == 3);
    assert(completion_probe.calls == 3);
    assert(completion_probe.status[0] == 1);
    assert(completion_probe.status[1] == 1);
    assert(completion_probe.status[2] == 1);
    assert(owner.cancelled_count == 3);
    assert(usb_endpoint_cancel_pending_transfers(&owner, 0x83) == 0);
}

static void test_ep0_setup_control_and_identity(void) {
    USBTransferQueueOwner owner;
    const uint8_t setup[USB_TRANSFER_QUEUE_SETUP_SIZE] = {
        0x80, 0x06, 0x00, 0x01, 0x00, 0x00, 0x12, 0x00,
    };
    uint8_t response[18];
    reset_probes();
    usb_transfer_queue_owner_init(&owner, 0, 0);
    USBTransferRequestSpan setup_request = usb_submit_ep0_setup_request(
        &owner,
        setup,
        request_complete,
        0
    );
    assert(usb_transfer_request_validate_tuple(setup_request));
    assert(memcmp(owner.setup_packet, setup, sizeof(setup)) == 0);
    assert(tinyusb_probe.endpoint == 0x00);
    assert(tinyusb_probe.length == sizeof(setup));
    assert(usb_ep0_setup_transfer_complete(&owner, 0, sizeof(setup)));

    USBTransferRequestSpan response_request =
        usb_ep0_in_transfer_request_create(
            &owner,
            byte_span(response, sizeof(response)),
            sizeof(response),
            request_complete,
            0
        );
    assert(response_request.current->endpoint_address == 0x80);
    assert(usb_transfer_submit(&owner, response_request));
    assert(usb_ep0_control_transfer_complete(
        &owner,
        0x80,
        0,
        sizeof(response)
    ));
    assert(completion_probe.calls == 2);
    assert(!usb_ep0_control_transfer_complete(&owner, 0x81, 0, 0));
}

static void test_event_status_noop_and_validation(void) {
    USBTransferQueueOwner owner;
    uint8_t payload[1];
    reset_probes();
    usb_transfer_queue_owner_init(&owner, 0, 0);
    usb_endpoint_transfer_worker_start_hook_noop();
    assert(usb_submit_status_two_response_masked(&owner) == 2);
    assert(owner.status_response == 2);
    assert(usb_endpoint_worker_event_create(&owner, 0x1234, 0x5678) == 1);
    assert(usb_endpoint_worker_event_create(&owner, 1, 2) == 2);
    assert(owner.worker_owner == 1);
    assert(owner.worker_signal == 2);

    assert(usb_transfer_request_create(
        &owner,
        0x86,
        byte_span(payload, sizeof(payload)),
        1,
        0,
        0
    ).current == 0);
    assert(usb_transfer_request_create(
        &owner,
        0x81,
        byte_span(payload, sizeof(payload)),
        2,
        0,
        0
    ).current == 0);
    assert(j_usb_device_dispatch_transfer_completion_optional(
        &owner,
        (USBTransferRequestSpan){0}
    ));
}

int main(void) {
    test_create_submit_fifo_and_worker();
    test_submit_failure_is_observable_and_released();
    test_cancel_active_and_pending_fifo();
    test_ep0_setup_control_and_identity();
    test_event_status_noop_and_validation();
    puts("usb transfer queue owner tests passed");
    return 0;
}
