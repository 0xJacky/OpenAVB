#pragma once

#include "avp/devices/usb/usb_descriptor_adapter.h"
#include "common/tusb_types.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


#define USB_TRANSFER_REQUEST_PUBLIC __attribute__((used, retain))

enum {
    USB_TRANSFER_REQUEST_SIZE = 0x70,
    USB_TRANSFER_REQUEST_TYPE = 5,
    USB_TRANSFER_CALLBACK_COUNT = 2,
    USB_EP0_CONTROL_LENGTH_LIMIT = 0x100,
};

typedef struct USBTransferRequest USBTransferRequest;

typedef struct {
    uint8_t *current;
    uintptr_t lower_bound;
    uintptr_t upper_bound;
    uintptr_t type;
} USBTransferBufferSpan;

typedef struct {
    USBTransferRequest *current;
    uintptr_t lower_bound;
    uintptr_t upper_bound;
    uintptr_t type;
} USBTransferRequestSpan;

typedef void (*USBTransferCompletion)(
    USBTransferRequest *current,
    uintptr_t lower_bound,
    uintptr_t upper_bound,
    uintptr_t type
);

struct USBTransferRequest {
    uint8_t endpoint_address;
    uint8_t reserved_header[7];
    USBTransferBufferSpan payload;
    int32_t status;
    uint32_t requested_length;
    uint32_t actual_length;
    uint8_t transfer_state;
    uint8_t reserved_state[3];
    USBTransferCompletion completion;
    uintptr_t completion_type;
    USBTransferRequestSpan next;
    uint8_t reserved_tail[8];
};

typedef struct {
    USBTransferRequest *records;
    uint8_t *in_use;
    size_t capacity;
} USBTransferRequestPool;

typedef struct {
    uint8_t reserved_flags[5];
    uint8_t ep0_in_completion;
    uint8_t reserved_to_phase[0x4a];
    int8_t ep0_in_phase;
} USBTransferCompletionState;

typedef struct {
    void *current;
    uintptr_t lower_bound;
    uintptr_t upper_bound;
    uintptr_t type;
} USBTransferCallbackSpan;

typedef struct {
    uint8_t reserved_prefix[0x30];
    USBDescriptorSpan payload;
    tusb_control_request_t setup;
    uint8_t reserved_to_callbacks[0xe0];
    USBTransferCallbackSpan endpoint_callbacks[USB_TRANSFER_CALLBACK_COUNT];
} USBTransferEP0State;

typedef struct {
    uintptr_t current;
    uintptr_t lower_bound;
    uintptr_t upper_bound;
    bool current_at_or_above_lower;
} USBTransferSpanBounds;

typedef struct {
    uint16_t length;
    bool exceeds_control_limit;
} USBTransferClampedLength;

_Static_assert(sizeof(USBTransferBufferSpan) == 0x20,
    "transfer buffer span size");
_Static_assert(sizeof(USBTransferRequestSpan) == 0x20,
    "transfer request span size");
_Static_assert(sizeof(USBTransferRequest) == USB_TRANSFER_REQUEST_SIZE,
    "transfer request size");
_Static_assert(offsetof(USBTransferRequest, payload) == 0x08,
    "transfer payload offset");
_Static_assert(offsetof(USBTransferRequest, status) == 0x28,
    "transfer status offset");
_Static_assert(offsetof(USBTransferRequest, requested_length) == 0x2c,
    "transfer requested length offset");
_Static_assert(offsetof(USBTransferRequest, actual_length) == 0x30,
    "transfer actual length offset");
_Static_assert(offsetof(USBTransferRequest, transfer_state) == 0x34,
    "transfer state offset");
_Static_assert(offsetof(USBTransferRequest, completion) == 0x38,
    "transfer completion offset");
_Static_assert(offsetof(USBTransferRequest, completion_type) == 0x40,
    "transfer completion type offset");
_Static_assert(offsetof(USBTransferRequest, next) == 0x48,
    "transfer next span offset");
_Static_assert(offsetof(USBTransferCompletionState, ep0_in_completion) == 5,
    "EP0 IN completion flag offset");
_Static_assert(offsetof(USBTransferCompletionState, ep0_in_phase) == 0x50,
    "EP0 IN phase offset");
_Static_assert(offsetof(USBTransferEP0State, payload) == 0x30,
    "EP0 payload span offset");
_Static_assert(offsetof(USBTransferEP0State, setup) == 0x50,
    "EP0 setup request offset");
_Static_assert(offsetof(USBTransferEP0State, endpoint_callbacks) == 0x138,
    "EP0 endpoint callback table offset");

USB_TRANSFER_REQUEST_PUBLIC bool usb_transfer_request_complete_and_release(
    USBTransferRequestPool *pool,
    USBTransferRequestSpan request,
    USBTransferCompletionState *controller
);

USB_TRANSFER_REQUEST_PUBLIC bool usb_transfer_request_create_and_submit(
    USBTransferRequestPool *pool,
    uint8_t endpoint_address,
    USBTransferBufferSpan payload,
    uint32_t requested_length,
    USBTransferCompletion completion,
    uintptr_t completion_type,
    USBTransferRequestSpan *submitted_request
);

USB_TRANSFER_REQUEST_PUBLIC bool usb_ep0_validate_descriptor_span_thunk(
    USBDescriptorSpan descriptor
);

USB_TRANSFER_REQUEST_PUBLIC bool usb_transfer_request_validate_tuple(
    USBTransferRequestSpan request
);

USB_TRANSFER_REQUEST_PUBLIC USBTransferRequestSpan
usb_ep0_outlined_clear_transfer_span(void);

USB_TRANSFER_REQUEST_PUBLIC USBTransferSpanBounds
usb_ep0_outlined_load_payload_bounds(const USBTransferEP0State *state);

USB_TRANSFER_REQUEST_PUBLIC USBDescriptorSpan
usb_ep0_outlined_load_payload_span(const USBTransferEP0State *state);

USB_TRANSFER_REQUEST_PUBLIC USBDescriptorSpan
usb_ep0_outlined_setup_request_span_x25(USBTransferEP0State *state);

USB_TRANSFER_REQUEST_PUBLIC USBTransferCallbackSpan
usb_ep0_outlined_load_endpoint_callback_span_x9(
    const USBTransferCallbackSpan *record
);

USB_TRANSFER_REQUEST_PUBLIC USBTransferCallbackSpan *
usb_ep0_outlined_select_endpoint_callback_record(
    USBTransferEP0State *state,
    uint32_t endpoint_index
);

USB_TRANSFER_REQUEST_PUBLIC USBTransferRequestSpan
usb_transfer_outlined_initialize_request_tail(
    USBTransferRequestSpan request,
    uint32_t requested_length,
    USBTransferCompletion completion,
    uintptr_t completion_type
);

USB_TRANSFER_REQUEST_PUBLIC USBTransferRequestSpan
usb_transfer_outlined_restore_request_span_x22_x19(
    USBTransferRequestSpan request
);

USB_TRANSFER_REQUEST_PUBLIC USBTransferClampedLength
usb_ep0_outlined_clamp_configuration_descriptor_length(
    const tusb_desc_configuration_t *descriptor,
    uint16_t requested_length
);

USB_TRANSFER_REQUEST_PUBLIC USBTransferRequestSpan
usb_transfer_request_allocate_one(USBTransferRequestPool *pool);
