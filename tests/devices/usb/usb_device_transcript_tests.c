#include "avp/devices/usb/usb_device.h"
#include "avp/devices/simplevirtio/simple_virtio_dcd.h"

#include "class/dfu/dfu.h"
#include "class/dfu/dfu_device.h"
#include "common/tusb_common.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>


enum {
    USB_EVENT_TRANSFER = 1,
    USB_EVENT_RESET = 2,
    USB_RESPONSE_TRANSFER = 1,
    USB_RESPONSE_STALL = 2,
    USB_RESPONSE_RESET = 4,
};

static bool validation_enabled;

static uint32_t fake_millis(void *context) {
    uint32_t *millis = context;
    return (*millis)++;
}

static bool validate_image(uint32_t download_size) {
    return validation_enabled && download_size == 4096;
}

static uint32_t control_event(
    uint8_t *event,
    uint8_t request_type,
    uint8_t request,
    uint16_t value,
    uint16_t index,
    const uint8_t *payload,
    uint16_t length
) {
    const bool device_to_host = (request_type & 0x80) != 0;
    const uint32_t payload_size = device_to_host ? 8 : 8 + length;
    event[0] = (uint8_t)payload_size;
    event[1] = (uint8_t)(payload_size >> 8);
    event[2] = (uint8_t)(payload_size >> 16);
    event[3] = (uint8_t)(payload_size >> 24);
    event[4] = 0;
    event[5] = USB_EVENT_TRANSFER;
    event[6] = request_type;
    event[7] = request;
    event[8] = (uint8_t)value;
    event[9] = (uint8_t)(value >> 8);
    event[10] = (uint8_t)index;
    event[11] = (uint8_t)(index >> 8);
    event[12] = (uint8_t)length;
    event[13] = (uint8_t)(length >> 8);
    for (uint16_t byte = 0; !device_to_host && byte < length; ++byte) {
        event[14 + byte] = payload[byte];
    }
    return 6 + payload_size;
}

static USBEventResult transact(
    USBDevice *device,
    uint8_t *event,
    uint32_t event_size,
    uint8_t *response
) {
    return usb_device_handle_event(
        device,
        event,
        event_size,
        response,
        256
    );
}

int main(void) {
    uint32_t injected_millis = 100;
    simple_virtio_dcd_bind_time_source(fake_millis, &injected_millis);
    tusb_time_delay_ms_api(5);
    assert(injected_millis == 106);
    injected_millis = UINT32_MAX - 2u;
    tusb_time_delay_ms_api(5);
    assert(injected_millis == 3);
    simple_virtio_dcd_bind_time_source(0, 0);
    const uint32_t monotonic_start = tusb_time_millis_api();
    const uint32_t monotonic_end = tusb_time_millis_api();
    assert((uint32_t)(monotonic_end - monotonic_start) < UINT32_C(1000));

    assert(tud_dfu_get_timeout_cb(0, DFU_DNBUSY) == 50);
    assert(tud_dfu_get_timeout_cb(0, DFU_MANIFEST) == 0);
    AVPPlatformIdentity identity = {
        .ecid = UINT64_C(0x1a2b3c4d5e6f7081),
        .chip_id = 0xfe01,
        .security_domain = 1,
        .chip_fuse_mode = 3,
        .security_epoch = 1,
        .board_id = 0x90,
        .boot_flags = 0x3c,
        .variant = "vresearch101",
    };
    uint8_t storage[4096] = {};
    USBDevice device;
    usb_device_initialize(
        &device,
        &identity,
        storage,
        sizeof(storage),
        validate_image,
        USB_MODE_DFU
    );

    uint8_t event[4096] = {};
    uint8_t response[256] = {};
    uint32_t size = control_event(event, 0x80, 6, 0x0100, 0, 0, 18);
    USBEventResult result = transact(&device, event, size, response);
    assert(result.response_size == 20);
    assert(response[0] == USB_RESPONSE_TRANSFER);
    assert(response[1] == 0);
    assert(response[2] == 0x12 && response[3] == 0x01);
    assert(response[12] == 0x27 && response[13] == 0x12);

    size = control_event(event, 0x80, 6, 0x0200, 0, 0, 25);
    result = transact(&device, event, size, response);
    assert(result.response_size == 27);
    static const uint8_t expected_configuration[] = {
        0x09, 0x02, 0x19, 0x00, 0x01, 0x01, 0x05, 0x80, 0xfa,
        0x09, 0x04, 0x00, 0x00, 0x00, 0xfe, 0x01, 0x00, 0x00,
        0x07, 0x21, 0x01, 0x0a, 0x00, 0x00, 0x08,
    };
    for (uint32_t byte = 0; byte < sizeof(expected_configuration); ++byte) {
        assert(response[2 + byte] == expected_configuration[byte]);
    }

    uint8_t block[2048] = {0x41, 0x56, 0x50, 0x42};
    const uint8_t reset_event[] = {0, 0, 0, 0, 0, USB_EVENT_RESET};

    size = control_event(event, 0x21, 1, 0, 0, block, sizeof(block));
    result = transact(&device, event, size, response);
    assert(result.response_size == 2);
    size = control_event(event, 0x21, 1, 1, 0, block, sizeof(block));
    result = transact(&device, event, size, response);
    assert(result.response_size == 2);
    size = control_event(event, 0x21, 1, 2, 0, 0, 0);
    result = transact(&device, event, size, response);
    assert(result.response_size == 2);
    size = control_event(event, 0xa1, 3, 0, 0, 0, 6);
    result = transact(&device, event, size, response);
    assert(result.response_size == 8);
    assert(device.image_validated == 0);
    result = usb_device_handle_event(
        &device,
        reset_event,
        sizeof(reset_event),
        response,
        sizeof(response)
    );
    assert(result.action == USB_FIRMWARE_ACTION_NONE);
    assert(device.download_size == 0);
    assert(device.next_block == 0);

    validation_enabled = true;
    size = control_event(event, 0x21, 1, 0, 0, block, sizeof(block));
    result = transact(&device, event, size, response);
    assert(result.response_size == 2);
    assert(response[0] == USB_RESPONSE_TRANSFER);

    size = control_event(event, 0x21, 1, 1, 0, block, sizeof(block));
    result = transact(&device, event, size, response);
    assert(result.response_size == 2);
    assert(response[0] == USB_RESPONSE_TRANSFER);

    size = control_event(event, 0xa1, 3, 0, 0, 0, 6);
    result = transact(&device, event, size, response);
    assert(result.response_size == 8);
    assert(response[6] == 5);
    assert(response[3] == 50);
    assert(response[4] == 0 && response[5] == 0);
    assert(device.download_size == sizeof(block) * 2);
    for (uint32_t byte = 0; byte < sizeof(block) * 2; ++byte) {
        assert(storage[byte] == block[byte % sizeof(block)]);
    }

    size = control_event(event, 0x21, 1, 2, 0, 0, 0);
    result = transact(&device, event, size, response);
    assert(result.response_size == 2);
    assert(response[0] == USB_RESPONSE_TRANSFER);

    size = control_event(event, 0xa1, 3, 0, 0, 0, 6);
    result = transact(&device, event, size, response);
    assert(result.response_size == 8);
    assert(response[6] == 6);
    assert(device.image_validated == 1);
    assert(device.manifestation_started == 1);

    size = control_event(event, 0xa1, 3, 0, 0, 0, 6);
    result = transact(&device, event, size, response);
    assert(result.response_size == 8);
    assert(response[6] == 7);

    result = usb_device_handle_event(
        &device,
        reset_event,
        sizeof(reset_event),
        response,
        sizeof(response)
    );
    assert(result.response_size == 2);
    assert(result.action == USB_FIRMWARE_ACTION_HANDOFF);
    assert(response[0] == USB_RESPONSE_RESET);

    event[0] = 7;
    event[1] = 0;
    event[2] = 0;
    event[3] = 0;
    event[4] = 0;
    event[5] = USB_EVENT_TRANSFER;
    result = transact(&device, event, 14, response);
    assert(result.response_size == 2);
    assert(response[0] == USB_RESPONSE_STALL);
}
