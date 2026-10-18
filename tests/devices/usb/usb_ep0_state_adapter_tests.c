#include "avp/devices/usb/usb_ep0_state_adapter.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>


static unsigned status_calls;
static unsigned stall_out_calls;
static unsigned stall_in_calls;
static bool status_result = true;
static uint8_t applied_test_mode;

bool tud_control_status(
    uint8_t rhport,
    const tusb_control_request_t *request
) {
    assert(rhport == 0);
    assert(request != 0);
    ++status_calls;
    return status_result;
}

void dcd_edpt_stall(uint8_t rhport, uint8_t endpoint) {
    assert(rhport == 0);
    if (endpoint == TU_EP0_OUT) {
        ++stall_out_calls;
    } else {
        assert(endpoint == TU_EP0_IN);
        ++stall_in_calls;
    }
}

static void apply_test_mode(uint8_t selector) {
    applied_test_mode = selector;
}

static void reset_transcript(void) {
    status_calls = 0;
    stall_out_calls = 0;
    stall_in_calls = 0;
    status_result = true;
    applied_test_mode = 0;
}

static void test_shared_buffers(void) {
    USBEP0State state = {0};
    uint8_t backing[USB_DEVICE_SHARED_BACKING_SIZE];
    memset(backing, 0xa5, sizeof(backing));
    assert(usb_device_shared_buffers_initialize(
        &state,
        backing,
        sizeof(backing)
    ) == 0);
    assert(state.initialized == 1);

    const size_t expected_lengths[] = {0x100, 0x800, 0, 0, 0};
    const size_t expected_offsets[] = {0, 0x100, 0x900, 0x900, 0x900};
    for (uint32_t index = 0; index < USB_DEVICE_SHARED_BUFFER_COUNT; ++index) {
        USBDeviceByteSpan span = usb_device_shared_buffer_span(&state, index);
        assert(span.current == backing + expected_offsets[index]);
        assert(span.lower_bound == (uintptr_t)span.current);
        assert(span.upper_bound - span.lower_bound == expected_lengths[index]);
        assert(usb_device_validate_byte_span(span));
    }
    assert(usb_device_shared_buffer_span(&state, 5).current == 0);

    state.shared_buffers[0].current++;
    assert(usb_device_shared_buffers_initialize(
        &state,
        backing,
        sizeof(backing)
    ) == 0);
    assert(state.shared_buffers[0].current == backing + 1);
}

static void test_span_validation(void) {
    uint8_t bytes[8];
    USBDeviceByteSpan span = usb_device_make_byte_span(bytes, bytes + 8);
    assert(usb_device_validate_byte_span(span));
    span.current = bytes + 8;
    assert(usb_device_validate_byte_span(span));
    span.current = (uint8_t *)((uintptr_t)bytes + 9);
    assert(!usb_device_validate_byte_span(span));
    span = usb_device_make_byte_span(bytes, bytes + 8);
    span.type = 2;
    assert(!usb_device_validate_byte_span(span));
}

static void test_completion_and_constant_contracts(void) {
    USBEP0State state = {
        .apply_test_mode = apply_test_mode,
        .test_mode_selector = 4,
        .transfer_flags = 0xa5a5,
    };
    usb_ep0_set_configuration_completion_noop(&state);
    assert(state.transfer_flags == 0xa5a5);
    assert(usb_ep0_test_mode_selector(&state) == 4);
    usb_ep0_set_test_mode_complete(&state);
    assert(applied_test_mode == 4);
    assert(usb_ep0_return_success_thunk() == 0);
    assert(usb_outlined_return_success() == 0);
    assert(usb_ep0_control_request_preflight_allow(&state.active_request));
}

static void test_reset_and_status_transcript(void) {
    reset_transcript();
    USBEP0State state = {
        .transfer_token = 1,
        .transfer_flags = 2,
        .transfer_value = 3,
        .transfer_index = 4,
        .status_stage_active = 0,
        .active_request = {.bRequest = 5},
    };
    assert(usb_ep0_submit_status_stage(&state));
    assert(status_calls == 1);
    assert(state.status_stage_active == 1);
    assert(stall_out_calls == 0);
    assert(stall_in_calls == 0);

    status_result = false;
    assert(!usb_ep0_submit_status_stage(&state));
    assert(status_calls == 2);
    assert(state.status_stage_active == 0);
    assert(state.transfer_token == UINTPTR_MAX - UINT32_MAX);
    assert(state.transfer_flags == 0);
    assert(state.transfer_value == 0);
    assert(state.transfer_index == 0);
    assert(state.active_request.bRequest == 0);
    assert(stall_out_calls == 1);
    assert(stall_in_calls == 1);
}

static void test_short_backing_fails(void) {
    USBEP0State state = {0};
    uint8_t backing[USB_DEVICE_SHARED_BACKING_SIZE - 1];
    assert(usb_device_shared_buffers_initialize(
        &state,
        backing,
        sizeof(backing)
    ) == -1);
    assert(state.initialized == 0);
}

int main(void) {
    test_shared_buffers();
    test_span_validation();
    test_completion_and_constant_contracts();
    test_reset_and_status_transcript();
    test_short_backing_fails();
    puts("usb_ep0_state_adapter_tests: ok");
    return 0;
}
