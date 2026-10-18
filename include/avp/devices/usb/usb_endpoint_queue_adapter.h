#pragma once

#include "tusb.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


#define USB_ENDPOINT_QUEUE_PUBLIC __attribute__((used, retain))

enum {
    USB_ENDPOINT_RECORD_COUNT = 12,
    USB_ENDPOINT_RECORD_STRIDE = 0x70,
    USB_QUEUED_PAYLOAD_NODE_SIZE = 0x48,
    USB_TRANSFER_OPERATIONS_SIZE = 0x120,
};

typedef enum {
    USB_ENDPOINT_TUPLE_BYTES = 1,
    USB_ENDPOINT_TUPLE_PAYLOAD_NODE = 2,
    USB_ENDPOINT_TUPLE_CONTROLLER = 3,
    USB_ENDPOINT_TUPLE_OPERATIONS = 4,
} USBEndpointTupleType;

typedef struct {
    void *current;
    uintptr_t lower_bound;
    uintptr_t upper_bound;
    uintptr_t type;
} USBEndpointTuple;

typedef struct {
    uint32_t reserved[3];
    uint32_t phase;
    uint8_t storage[USB_ENDPOINT_RECORD_STRIDE - 0x10];
} USBEndpointRecord;

typedef struct {
    uint8_t header[0x30];
    USBEndpointTuple global_transfer_span;
    USBEndpointRecord endpoints[USB_ENDPOINT_RECORD_COUNT];
    uint64_t stack_guard;
} USBEndpointController;

typedef struct {
    uint8_t prefix[0x48];
    USBEndpointTuple next_span;
} USBEndpointTransferRecord;

typedef struct {
    uint8_t prefix[0x10];
    USBEndpointTuple next_span;
} USBEndpointQueueState;

typedef void (*USBEP0SetupCompletion)(void *context);

typedef struct {
    USBEndpointTuple transfer_state;
    uintptr_t endpoint;
    uintptr_t event;
    uintptr_t payload_size;
    uintptr_t completion;
} USBTransportEventArguments;

typedef struct {
    uint8_t endpoint;
    tusb_control_request_t setup;
    USBEndpointTuple setup_span;
    USBEP0SetupCompletion completion;
    void *completion_context;
} USBEP0SetupRequestArguments;

_Static_assert(sizeof(USBEndpointTuple) == 0x20, "endpoint tuple size");
_Static_assert(sizeof(USBEndpointRecord) == USB_ENDPOINT_RECORD_STRIDE,
    "endpoint record stride");
_Static_assert(offsetof(USBEndpointRecord, phase) == 0x0c,
    "endpoint phase offset");
_Static_assert(offsetof(USBEndpointController, endpoints) == 0x50,
    "endpoint array offset");
_Static_assert(offsetof(USBEndpointController, stack_guard) == 0x590,
    "endpoint array upper bound");
_Static_assert(offsetof(USBEndpointTransferRecord, next_span) == 0x48,
    "next transfer span offset");
_Static_assert(offsetof(USBEndpointQueueState, next_span) == 0x10,
    "destination queue span offset");

USB_ENDPOINT_QUEUE_PUBLIC USBEndpointTuple
usb_transfer_outlined_publish_state_and_operations_span(
    USBEndpointController *controller,
    uint8_t state,
    void *operations
);
USB_ENDPOINT_QUEUE_PUBLIC bool usb_transfer_validate_byte_span(
    USBEndpointTuple tuple
);
USB_ENDPOINT_QUEUE_PUBLIC bool usb_queued_payload_node_tuple_validate(
    USBEndpointTuple tuple
);
USB_ENDPOINT_QUEUE_PUBLIC bool usb_device_validate_controller_tuple(
    USBEndpointTuple tuple
);
USB_ENDPOINT_QUEUE_PUBLIC USBEndpointTuple
usb_device_outlined_global_transfer_span(
    const USBEndpointController *controller
);
USB_ENDPOINT_QUEUE_PUBLIC uint32_t
usb_endpoint_outlined_select_record_index_and_stride_w0_w8(
    uint8_t endpoint_address,
    uint32_t *stride
);
USB_ENDPOINT_QUEUE_PUBLIC bool usb_ep0_outlined_create_setup_request(
    const uint8_t *event,
    size_t event_size,
    USBEP0SetupCompletion completion,
    void *completion_context,
    USBEP0SetupRequestArguments *arguments
);
USB_ENDPOINT_QUEUE_PUBLIC USBEndpointRecord *
usb_endpoint_outlined_resolve_record_x8_x9(
    USBEndpointController *controller,
    uint32_t record_index,
    uint32_t stride
);
USB_ENDPOINT_QUEUE_PUBLIC USBEndpointTuple
usb_endpoint_outlined_load_array_bounds_x19(
    USBEndpointController *controller
);
USB_ENDPOINT_QUEUE_PUBLIC uint32_t
usb_endpoint_outlined_select_record_index_and_stride_w8_w9(
    uint8_t endpoint_address,
    uint32_t *stride
);
USB_ENDPOINT_QUEUE_PUBLIC USBEndpointTuple
usb_endpoint_outlined_load_controller_view(USBEndpointController *controller);
USB_ENDPOINT_QUEUE_PUBLIC bool usb_outlined_stack_guard_compare_saved_x8(
    const USBEndpointController *controller,
    uint64_t saved_guard
);
USB_ENDPOINT_QUEUE_PUBLIC USBTransportEventArguments
usb_device_outlined_prepare_transport_event_arguments(
    USBEndpointController *controller,
    uintptr_t endpoint,
    uintptr_t event,
    uintptr_t payload_size,
    uintptr_t completion
);
USB_ENDPOINT_QUEUE_PUBLIC void
usb_transfer_outlined_move_and_clear_next_span(
    USBEndpointTransferRecord *source,
    USBEndpointQueueState *destination
);
USB_ENDPOINT_QUEUE_PUBLIC USBEndpointTuple
usb_endpoint_outlined_load_array_bounds_x8(
    USBEndpointController *controller
);
USB_ENDPOINT_QUEUE_PUBLIC USBEndpointRecord *
usb_ep0_outlined_set_in_phase_one_advance_to_out_x9(
    USBEndpointRecord *ep0_in
);
USB_ENDPOINT_QUEUE_PUBLIC uint64_t usb_outlined_stack_guard_load_to_x8(
    const USBEndpointController *controller
);
USB_ENDPOINT_QUEUE_PUBLIC bool
usb_endpoint_outlined_compare_candidate_to_array_lower_x19(
    const USBEndpointController *controller,
    const USBEndpointRecord *candidate
);
USB_ENDPOINT_QUEUE_PUBLIC USBEndpointRecord *
usb_ep0_outlined_set_in_phase_advance_to_out_x8(
    USBEndpointRecord *ep0_in,
    uint32_t phase
);
