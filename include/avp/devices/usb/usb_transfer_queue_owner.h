#pragma once

#include "avp/devices/usb/usb_transfer_request_adapter.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


#define USB_TRANSFER_QUEUE_OWNER_PUBLIC __attribute__((used, retain))

enum {
    USB_TRANSFER_QUEUE_ENDPOINT_COUNT = 12,
    USB_TRANSFER_QUEUE_REQUEST_CAPACITY = 16,
    USB_TRANSFER_QUEUE_SETUP_SIZE = 8,
};

typedef struct USBTransferQueueOwner USBTransferQueueOwner;

typedef bool (*USBTransferDispatchCompletion)(
    void *context,
    USBTransferRequestSpan request
);

struct USBTransferQueueOwner {
    USBTransferRequest requests[USB_TRANSFER_QUEUE_REQUEST_CAPACITY];
    uint8_t requests_in_use[USB_TRANSFER_QUEUE_REQUEST_CAPACITY];
    USBTransferRequestPool pool;
    USBTransferCompletionState completion_state;
    USBTransferRequestSpan pending_head[USB_TRANSFER_QUEUE_ENDPOINT_COUNT];
    USBTransferRequestSpan pending_tail[USB_TRANSFER_QUEUE_ENDPOINT_COUNT];
    USBTransferRequestSpan active[USB_TRANSFER_QUEUE_ENDPOINT_COUNT];
    USBTransferDispatchCompletion dispatch_completion;
    void *dispatch_context;
    uint16_t worker_event;
    uint16_t worker_owner;
    uint16_t worker_signal;
    uint16_t next_worker_event;
    uint32_t worker_wake_count;
    uint32_t completed_count;
    uint32_t cancelled_count;
    uint32_t status_response;
    uint8_t setup_packet[USB_TRANSFER_QUEUE_SETUP_SIZE];
};

USB_TRANSFER_QUEUE_OWNER_PUBLIC void usb_transfer_queue_owner_init(
    USBTransferQueueOwner *owner,
    USBTransferDispatchCompletion dispatch_completion,
    void *dispatch_context
);

USB_TRANSFER_QUEUE_OWNER_PUBLIC void
usb_endpoint_transfer_worker_start_hook_noop(void);

USB_TRANSFER_QUEUE_OWNER_PUBLIC uint32_t
usb_submit_status_two_response_masked(USBTransferQueueOwner *owner);

USB_TRANSFER_QUEUE_OWNER_PUBLIC bool usb_endpoint_transfer_worker(
    USBTransferQueueOwner *owner,
    uint8_t endpoint_address,
    int32_t status,
    uint32_t actual_length
);

USB_TRANSFER_QUEUE_OWNER_PUBLIC bool usb_ep0_setup_transfer_complete(
    USBTransferQueueOwner *owner,
    int32_t status,
    uint32_t actual_length
);

USB_TRANSFER_QUEUE_OWNER_PUBLIC bool usb_transfer_submit(
    USBTransferQueueOwner *owner,
    USBTransferRequestSpan request
);

USB_TRANSFER_QUEUE_OWNER_PUBLIC bool usb_transfer_enqueue(
    USBTransferQueueOwner *owner,
    USBTransferRequestSpan request
);

USB_TRANSFER_QUEUE_OWNER_PUBLIC bool usb_ep0_control_transfer_complete(
    USBTransferQueueOwner *owner,
    uint8_t endpoint_address,
    int32_t status,
    uint32_t actual_length
);

USB_TRANSFER_QUEUE_OWNER_PUBLIC USBTransferRequestSpan
usb_submit_ep0_setup_request(
    USBTransferQueueOwner *owner,
    const uint8_t setup[USB_TRANSFER_QUEUE_SETUP_SIZE],
    USBTransferCompletion completion,
    uintptr_t completion_type
);

USB_TRANSFER_QUEUE_OWNER_PUBLIC size_t
usb_endpoint_cancel_pending_transfers(
    USBTransferQueueOwner *owner,
    uint8_t endpoint_address
);

USB_TRANSFER_QUEUE_OWNER_PUBLIC int32_t usb_transfer_status_unsupported(void);

USB_TRANSFER_QUEUE_OWNER_PUBLIC bool usb_endpoint_payload_queue_append(
    USBTransferQueueOwner *owner,
    USBTransferRequestSpan request
);

USB_TRANSFER_QUEUE_OWNER_PUBLIC uint16_t usb_endpoint_worker_event_create(
    USBTransferQueueOwner *owner,
    uint16_t worker_owner,
    uint16_t worker_signal
);

USB_TRANSFER_QUEUE_OWNER_PUBLIC USBTransferRequestSpan
usb_ep0_in_transfer_request_create(
    USBTransferQueueOwner *owner,
    USBTransferBufferSpan payload,
    uint32_t requested_length,
    USBTransferCompletion completion,
    uintptr_t completion_type
);

USB_TRANSFER_QUEUE_OWNER_PUBLIC USBTransferRequestSpan
usb_transfer_request_create(
    USBTransferQueueOwner *owner,
    uint8_t endpoint_address,
    USBTransferBufferSpan payload,
    uint32_t requested_length,
    USBTransferCompletion completion,
    uintptr_t completion_type
);

USB_TRANSFER_QUEUE_OWNER_PUBLIC bool
j_usb_device_dispatch_transfer_completion_optional(
    USBTransferQueueOwner *owner,
    USBTransferRequestSpan request
);
