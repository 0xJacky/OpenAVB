#include "avp/devices/usb/usb_transfer_queue_owner.h"

#include "common/tusb_common.h"
#include "device/usbd_pvt.h"


static void clear_bytes(void *object, size_t size) {
    uint8_t *bytes = object;
    for (size_t index = 0; index < size; ++index) {
        bytes[index] = 0;
    }
}

static USBTransferRequestSpan empty_request(void) {
    return (USBTransferRequestSpan){0};
}

static bool endpoint_slot(uint8_t endpoint_address, size_t *slot) {
    const uint8_t endpoint_number = tu_edpt_number(endpoint_address);
    if (endpoint_number >= USB_TRANSFER_QUEUE_ENDPOINT_COUNT / 2) {
        return false;
    }
    *slot = (size_t)endpoint_number * 2u +
        (tu_edpt_dir(endpoint_address) == TUSB_DIR_IN ? 1u : 0u);
    return true;
}

static bool payload_contains(
    USBTransferBufferSpan payload,
    uint32_t requested_length
) {
    if (requested_length == 0 && payload.current == 0 &&
        payload.lower_bound == 0 && payload.upper_bound == 0) {
        return true;
    }
    const uintptr_t current = (uintptr_t)payload.current;
    return payload.type == USB_DESCRIPTOR_SPAN_TYPE_BYTE &&
        payload.lower_bound <= current && current <= payload.upper_bound &&
        requested_length <= payload.upper_bound - current;
}

static bool finish_request(
    USBTransferQueueOwner *owner,
    USBTransferRequestSpan request,
    int32_t status,
    uint32_t actual_length
) {
    request.current->status = status;
    request.current->actual_length = actual_length;
    request.current->transfer_state = 0;
    const bool dispatched =
        j_usb_device_dispatch_transfer_completion_optional(owner, request);
    const bool released = usb_transfer_request_complete_and_release(
        &owner->pool,
        request,
        &owner->completion_state
    );
    ++owner->completed_count;
    return dispatched && released;
}

static USBTransferRequestSpan pop_pending(
    USBTransferQueueOwner *owner,
    size_t slot
) {
    USBTransferRequestSpan request = owner->pending_head[slot];
    if (!usb_transfer_request_validate_tuple(request)) {
        return empty_request();
    }
    owner->pending_head[slot] = request.current->next;
    request.current->next = empty_request();
    if (!usb_transfer_request_validate_tuple(owner->pending_head[slot])) {
        owner->pending_head[slot] = empty_request();
        owner->pending_tail[slot] = empty_request();
    }
    return request;
}

static bool start_request(
    USBTransferQueueOwner *owner,
    size_t slot,
    USBTransferRequestSpan request
) {
    USBTransferRequest *const current = request.current;
    current->status = usb_transfer_status_unsupported();
    current->actual_length = 0;
    current->transfer_state = 1;
    owner->active[slot] = request;
    if (usbd_edpt_xfer(
        0,
        current->endpoint_address,
        current->payload.current,
        (uint16_t)current->requested_length,
        false
    )) {
        return true;
    }
    owner->active[slot] = empty_request();
    (void)finish_request(
        owner,
        request,
        usb_transfer_status_unsupported(),
        0
    );
    return false;
}

static bool start_pending(USBTransferQueueOwner *owner, size_t slot) {
    bool all_started = true;
    while (!usb_transfer_request_validate_tuple(owner->active[slot])) {
        USBTransferRequestSpan request = pop_pending(owner, slot);
        if (!usb_transfer_request_validate_tuple(request)) {
            return all_started;
        }
        if (start_request(owner, slot, request)) {
            return true;
        }
        all_started = false;
    }
    return all_started;
}

void usb_transfer_queue_owner_init(
    USBTransferQueueOwner *owner,
    USBTransferDispatchCompletion dispatch_completion,
    void *dispatch_context
) {
    clear_bytes(owner, sizeof(*owner));
    owner->pool = (USBTransferRequestPool){
        .records = owner->requests,
        .in_use = owner->requests_in_use,
        .capacity = USB_TRANSFER_QUEUE_REQUEST_CAPACITY,
    };
    owner->completion_state.ep0_in_phase = -1;
    owner->dispatch_completion = dispatch_completion;
    owner->dispatch_context = dispatch_context;
    owner->next_worker_event = 1;
}

void usb_endpoint_transfer_worker_start_hook_noop(void) {
}

uint32_t usb_submit_status_two_response_masked(
    USBTransferQueueOwner *owner
) {
    owner->status_response = 2;
    return owner->status_response;
}

bool usb_endpoint_transfer_worker(
    USBTransferQueueOwner *owner,
    uint8_t endpoint_address,
    int32_t status,
    uint32_t actual_length
) {
    size_t slot;
    if (!endpoint_slot(endpoint_address, &slot)) {
        return false;
    }
    USBTransferRequestSpan request = owner->active[slot];
    if (!usb_transfer_request_validate_tuple(request) ||
        request.current->endpoint_address != endpoint_address) {
        return false;
    }
    owner->active[slot] = empty_request();
    ++owner->worker_wake_count;
    const bool completed = finish_request(
        owner,
        request,
        status,
        actual_length
    );
    return start_pending(owner, slot) && completed;
}

bool usb_ep0_setup_transfer_complete(
    USBTransferQueueOwner *owner,
    int32_t status,
    uint32_t actual_length
) {
    return usb_endpoint_transfer_worker(owner, 0x00, status, actual_length);
}

bool usb_transfer_enqueue(
    USBTransferQueueOwner *owner,
    USBTransferRequestSpan request
) {
    if (!usb_transfer_request_validate_tuple(request)) {
        return false;
    }
    size_t slot;
    if (!endpoint_slot(request.current->endpoint_address, &slot)) {
        return false;
    }
    request.current->status = usb_transfer_status_unsupported();
    request.current->actual_length = 0;
    request.current->transfer_state = 0;
    request.current->next = empty_request();
    if (usb_transfer_request_validate_tuple(owner->pending_tail[slot])) {
        owner->pending_tail[slot].current->next = request;
    } else {
        owner->pending_head[slot] = request;
    }
    owner->pending_tail[slot] = request;
    return true;
}

bool usb_transfer_submit(
    USBTransferQueueOwner *owner,
    USBTransferRequestSpan request
) {
    if (!usb_transfer_request_validate_tuple(request)) {
        return false;
    }
    size_t slot;
    if (!endpoint_slot(request.current->endpoint_address, &slot) ||
        request.current->requested_length > UINT16_MAX ||
        !payload_contains(
            request.current->payload,
            request.current->requested_length
        )) {
        return false;
    }
    if (usb_transfer_request_validate_tuple(owner->active[slot])) {
        return usb_transfer_enqueue(owner, request);
    }
    return start_request(owner, slot, request);
}

bool usb_ep0_control_transfer_complete(
    USBTransferQueueOwner *owner,
    uint8_t endpoint_address,
    int32_t status,
    uint32_t actual_length
) {
    if (endpoint_address != 0x00 && endpoint_address != 0x80) {
        return false;
    }
    return usb_endpoint_transfer_worker(
        owner,
        endpoint_address,
        status,
        actual_length
    );
}

USBTransferRequestSpan usb_submit_ep0_setup_request(
    USBTransferQueueOwner *owner,
    const uint8_t setup[USB_TRANSFER_QUEUE_SETUP_SIZE],
    USBTransferCompletion completion,
    uintptr_t completion_type
) {
    for (size_t index = 0; index < USB_TRANSFER_QUEUE_SETUP_SIZE; ++index) {
        owner->setup_packet[index] = setup[index];
    }
    USBTransferRequestSpan request = usb_transfer_request_create(
        owner,
        0x00,
        (USBTransferBufferSpan){
            .current = owner->setup_packet,
            .lower_bound = (uintptr_t)owner->setup_packet,
            .upper_bound = (uintptr_t)owner->setup_packet +
                sizeof(owner->setup_packet),
            .type = USB_DESCRIPTOR_SPAN_TYPE_BYTE,
        },
        sizeof(owner->setup_packet),
        completion,
        completion_type
    );
    if (!usb_transfer_request_validate_tuple(request) ||
        !usb_transfer_submit(owner, request)) {
        return empty_request();
    }
    return request;
}

size_t usb_endpoint_cancel_pending_transfers(
    USBTransferQueueOwner *owner,
    uint8_t endpoint_address
) {
    size_t slot;
    if (!endpoint_slot(endpoint_address, &slot)) {
        return 0;
    }
    size_t cancelled = 0;
    USBTransferRequestSpan active = owner->active[slot];
    owner->active[slot] = empty_request();
    if (usb_transfer_request_validate_tuple(active)) {
        ++cancelled;
        (void)finish_request(owner, active, 1, 0);
    }
    USBTransferRequestSpan request;
    while (usb_transfer_request_validate_tuple(
        request = pop_pending(owner, slot)
    )) {
        ++cancelled;
        (void)finish_request(owner, request, 1, 0);
    }
    owner->cancelled_count += (uint32_t)cancelled;
    return cancelled;
}

int32_t usb_transfer_status_unsupported(void) {
    return -1;
}

bool usb_endpoint_payload_queue_append(
    USBTransferQueueOwner *owner,
    USBTransferRequestSpan request
) {
    return usb_transfer_enqueue(owner, request);
}

uint16_t usb_endpoint_worker_event_create(
    USBTransferQueueOwner *owner,
    uint16_t worker_owner,
    uint16_t worker_signal
) {
    owner->worker_owner = worker_owner;
    owner->worker_signal = worker_signal;
    owner->worker_event = owner->next_worker_event++;
    if (owner->next_worker_event == 0) {
        owner->next_worker_event = 1;
    }
    return owner->worker_event;
}

USBTransferRequestSpan usb_ep0_in_transfer_request_create(
    USBTransferQueueOwner *owner,
    USBTransferBufferSpan payload,
    uint32_t requested_length,
    USBTransferCompletion completion,
    uintptr_t completion_type
) {
    return usb_transfer_request_create(
        owner,
        0x80,
        payload,
        requested_length,
        completion,
        completion_type
    );
}

USBTransferRequestSpan usb_transfer_request_create(
    USBTransferQueueOwner *owner,
    uint8_t endpoint_address,
    USBTransferBufferSpan payload,
    uint32_t requested_length,
    USBTransferCompletion completion,
    uintptr_t completion_type
) {
    size_t slot;
    if (!endpoint_slot(endpoint_address, &slot) ||
        requested_length > UINT16_MAX ||
        !payload_contains(payload, requested_length)) {
        return empty_request();
    }
    USBTransferRequestSpan request = usb_transfer_request_allocate_one(
        &owner->pool
    );
    if (!usb_transfer_request_validate_tuple(request)) {
        return empty_request();
    }
    request.current->endpoint_address = endpoint_address;
    request.current->payload = payload;
    request.current->status = usb_transfer_status_unsupported();
    request.current->actual_length = 0;
    return usb_transfer_outlined_initialize_request_tail(
        request,
        requested_length,
        completion,
        completion_type
    );
}

bool j_usb_device_dispatch_transfer_completion_optional(
    USBTransferQueueOwner *owner,
    USBTransferRequestSpan request
) {
    if (owner->dispatch_completion == 0) {
        return true;
    }
    return owner->dispatch_completion(owner->dispatch_context, request);
}
