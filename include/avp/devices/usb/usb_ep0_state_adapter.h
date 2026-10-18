#pragma once

#include "tusb.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


#define USB_EP0_STATE_PUBLIC __attribute__((used, retain))

enum {
    USB_DEVICE_SHARED_BACKING_SIZE = 0xc00,
    USB_DEVICE_SHARED_BUFFER_COUNT = 5,
    USB_DEVICE_DESCRIPTOR_BUFFER_SIZE = 0x100,
    USB_DEVICE_DFU_PAYLOAD_BUFFER_SIZE = 0x800,
};

typedef struct {
    uint8_t *current;
    uintptr_t lower_bound;
    uintptr_t upper_bound;
    uintptr_t type;
} USBDeviceByteSpan;

typedef void (*USBEP0TestModeApply)(uint8_t selector);

typedef struct {
    uint8_t *shared_backing;
    size_t shared_backing_capacity;
    USBDeviceByteSpan shared_buffers[USB_DEVICE_SHARED_BUFFER_COUNT];
    tusb_control_request_t active_request;
    USBEP0TestModeApply apply_test_mode;
    uintptr_t transfer_token;
    uint32_t transfer_flags;
    uint16_t transfer_value;
    uint16_t transfer_index;
    uint8_t test_mode_selector;
    uint8_t status_stage_active;
    uint8_t initialized;
} USBEP0State;

_Static_assert(sizeof(USBDeviceByteSpan) == 0x20, "USB byte span size");

USB_EP0_STATE_PUBLIC int usb_device_shared_buffers_initialize(
    USBEP0State *state,
    uint8_t *backing,
    size_t backing_capacity
);

USB_EP0_STATE_PUBLIC USBDeviceByteSpan usb_device_shared_buffer_span(
    const USBEP0State *state,
    uint32_t buffer_index
);

USB_EP0_STATE_PUBLIC void usb_ep0_set_configuration_completion_noop(
    USBEP0State *state
);

USB_EP0_STATE_PUBLIC void usb_ep0_set_test_mode_complete(
    USBEP0State *state
);

USB_EP0_STATE_PUBLIC int usb_ep0_return_success_thunk(void);
USB_EP0_STATE_PUBLIC bool usb_ep0_control_request_preflight_allow(
    const tusb_control_request_t *request
);
USB_EP0_STATE_PUBLIC bool usb_device_validate_byte_span(
    USBDeviceByteSpan span
);
USB_EP0_STATE_PUBLIC int usb_outlined_return_success(void);
USB_EP0_STATE_PUBLIC USBDeviceByteSpan usb_device_make_byte_span(
    uint8_t *current,
    uint8_t *upper_bound
);
USB_EP0_STATE_PUBLIC uint8_t usb_ep0_test_mode_selector(
    const USBEP0State *state
);
USB_EP0_STATE_PUBLIC void usb_ep0_reset_control_state(USBEP0State *state);
USB_EP0_STATE_PUBLIC void usb_ep0_reset_and_stall(USBEP0State *state);
USB_EP0_STATE_PUBLIC bool usb_ep0_submit_status_stage(USBEP0State *state);
