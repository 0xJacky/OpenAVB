#include "avp/devices/usb/usb_ep0_state_adapter.h"

#include "device/dcd.h"
#include "device/usbd.h"


static const size_t shared_buffer_lengths[USB_DEVICE_SHARED_BUFFER_COUNT] = {
    USB_DEVICE_DESCRIPTOR_BUFFER_SIZE,
    USB_DEVICE_DFU_PAYLOAD_BUFFER_SIZE,
    0,
    0,
    0,
};

USBDeviceByteSpan usb_device_make_byte_span(
    uint8_t *current,
    uint8_t *upper_bound
) {
    return (USBDeviceByteSpan){
        .current = current,
        .lower_bound = (uintptr_t)current,
        .upper_bound = (uintptr_t)upper_bound,
        .type = 1,
    };
}

bool usb_device_validate_byte_span(USBDeviceByteSpan span) {
    const uintptr_t current = (uintptr_t)span.current;
    return span.type == 1 &&
        span.lower_bound <= current &&
        current <= span.upper_bound;
}

int usb_device_shared_buffers_initialize(
    USBEP0State *state,
    uint8_t *backing,
    size_t backing_capacity
) {
    if (state->initialized != 0) {
        return 0;
    }
    if (backing == 0 || backing_capacity < USB_DEVICE_SHARED_BACKING_SIZE) {
        return -1;
    }

    state->shared_backing = backing;
    state->shared_backing_capacity = backing_capacity;
    size_t offset = 0;
    for (size_t index = 0; index < USB_DEVICE_SHARED_BUFFER_COUNT; ++index) {
        uint8_t *const current = backing + offset;
        USBDeviceByteSpan span = usb_device_make_byte_span(
            current,
            current + shared_buffer_lengths[index]
        );
        if (!usb_device_validate_byte_span(span)) {
            return -1;
        }
        state->shared_buffers[index] = span;
        offset += shared_buffer_lengths[index];
    }
    state->initialized = 1;
    return 0;
}

USBDeviceByteSpan usb_device_shared_buffer_span(
    const USBEP0State *state,
    uint32_t buffer_index
) {
    if (buffer_index >= USB_DEVICE_SHARED_BUFFER_COUNT) {
        return (USBDeviceByteSpan){0};
    }
    const USBDeviceByteSpan published = state->shared_buffers[buffer_index];
    return usb_device_make_byte_span(
        published.current,
        published.current + shared_buffer_lengths[buffer_index]
    );
}

void usb_ep0_set_configuration_completion_noop(USBEP0State *state) {
    (void)state;
}

uint8_t usb_ep0_test_mode_selector(const USBEP0State *state) {
    return state->test_mode_selector;
}

void usb_ep0_set_test_mode_complete(USBEP0State *state) {
    state->apply_test_mode(usb_ep0_test_mode_selector(state));
}

int usb_outlined_return_success(void) {
    return 0;
}

int usb_ep0_return_success_thunk(void) {
    return usb_outlined_return_success();
}

bool usb_ep0_control_request_preflight_allow(
    const tusb_control_request_t *request
) {
    (void)request;
    return true;
}

void usb_ep0_reset_control_state(USBEP0State *state) {
    state->transfer_token = UINTPTR_MAX - UINT32_MAX;
    state->transfer_flags = 0;
    state->transfer_value = 0;
    state->transfer_index = 0;
    state->status_stage_active = 0;
    state->active_request = (tusb_control_request_t){0};
}

void usb_ep0_reset_and_stall(USBEP0State *state) {
    usb_ep0_reset_control_state(state);
    dcd_edpt_stall(0, TU_EP0_OUT);
    dcd_edpt_stall(0, TU_EP0_IN);
}

bool usb_ep0_submit_status_stage(USBEP0State *state) {
    if (!tud_control_status(0, &state->active_request)) {
        usb_ep0_reset_and_stall(state);
        return false;
    }
    state->status_stage_active = 1;
    return true;
}
