#include "avp/devices/usb/usb_device.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>


enum {
    USB_EVENT_TRANSFER = 1,
    USB_EVENT_RESET = 2,
    USB_RESPONSE_TRANSFER = 1,
    USB_RESPONSE_RESET = 4,
    SOURCE_IBSS_EXECUTION_MARKER = 0x53494253,
};

static bool validate_image(uint32_t download_size) {
    return download_size != 0;
}

static uint32_t read_u32_le(const uint8_t *bytes) {
    return (uint32_t)bytes[0] |
        (uint32_t)bytes[1] << 8 |
        (uint32_t)bytes[2] << 16 |
        (uint32_t)bytes[3] << 24;
}

static uint32_t control_event(
    uint8_t *event,
    uint8_t request_type,
    uint8_t request,
    uint16_t value,
    uint16_t index,
    uint16_t length
) {
    event[0] = 8;
    event[1] = 0;
    event[2] = 0;
    event[3] = 0;
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
    return 14;
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
    AVPPlatformIdentity identity = {
        .ecid = UINT64_C(0x0a1b2c3d4e5f6071),
        .chip_id = 0xfe01,
        .security_domain = 1,
        .chip_fuse_mode = 3,
        .security_epoch = 1,
        .board_id = 0x90,
        .boot_flags = 0x3c,
        .variant = "vresearch101",
    };
    uint8_t storage[64] = {};
    USBDevice device;
    usb_device_initialize(
        &device,
        &identity,
        storage,
        sizeof(storage),
        validate_image,
        USB_MODE_RECOVERY
    );
    device.download_size = 0x6a00;

    uint8_t event[128] = {};
    uint8_t response[256] = {};
    uint32_t size = control_event(event, 0x80, 6, 0x0100, 0, 18);
    USBEventResult result = transact(&device, event, size, response);
    assert(result.response_size == 20);
    assert(response[0] == USB_RESPONSE_TRANSFER);
    assert(response[12] == 0x81 && response[13] == 0x12);

    size = control_event(event, 0x80, 6, 0x0200, 0, 43);
    result = transact(&device, event, size, response);
    assert(result.response_size == 45);
    assert(response[2] == 9 && response[3] == 2);
    assert(response[4] == 43 && response[5] == 0);
    assert(response[20] == 7 && response[21] == 5);
    assert(response[22] == 4 && response[23] == 2);

    size = control_event(event, 0x80, 6, 0x0304, 0x0409, 255);
    result = transact(&device, event, size, response);
    assert(result.response_size > 30);
    assert(response[0] == USB_RESPONSE_TRANSFER);
    bool execution_marker_found = false;
    for (uint32_t byte = 2; byte + 6 < result.response_size; byte += 2) {
        if (response[byte] == 'I' && response[byte + 2] == 'B' &&
            response[byte + 4] == 'S' && response[byte + 6] == 'S') {
            execution_marker_found = true;
        }
    }
    assert(execution_marker_found);

    const uint8_t bulk_event[] = {
        4, 0, 0, 0, 4, USB_EVENT_TRANSFER, 0xde, 0xad, 0xbe, 0xef,
    };
    result = usb_device_handle_event(
        &device,
        bulk_event,
        sizeof(bulk_event),
        response,
        sizeof(response)
    );
    assert(result.response_size == 2);
    assert(response[0] == USB_RESPONSE_TRANSFER && response[1] == 4);

    const uint8_t zlp_event[] = {0, 0, 0, 0, 4, USB_EVENT_TRANSFER};
    result = usb_device_handle_event(
        &device,
        zlp_event,
        sizeof(zlp_event),
        response,
        sizeof(response)
    );
    assert(result.response_size == 2);

    size = control_event(event, 0xc0, 0x5a, 0, 0, 20);
    result = transact(&device, event, size, response);
    assert(result.response_size == 22);
    assert(read_u32_le(response + 2) == SOURCE_IBSS_EXECUTION_MARKER);
    assert(read_u32_le(response + 6) == 0x6a00);
    assert(read_u32_le(response + 10) == 4);
    assert(read_u32_le(response + 14) == 2);
    assert(read_u32_le(response + 18) == 1);

    size = control_event(event, 0x41, 0, 0, 0, 0);
    result = transact(&device, event, size, response);
    assert(result.response_size == 2);
    assert(device.recovery_received_bytes == 0);
    assert(device.recovery_chunk_count == 0);
    assert(device.recovery_zlp_count == 0);

    size = control_event(event, 0x01, 11, 1, 1, 0);
    result = transact(&device, event, size, response);
    assert(result.response_size == 2);
    assert(response[0] == USB_RESPONSE_TRANSFER);

    const uint8_t reset_event[] = {0, 0, 0, 0, 0, USB_EVENT_RESET};
    result = usb_device_handle_event(
        &device,
        reset_event,
        sizeof(reset_event),
        response,
        sizeof(response)
    );
    assert(result.response_size == 2);
    assert(result.action == USB_FIRMWARE_ACTION_NONE);
    assert(response[0] == USB_RESPONSE_RESET);
    assert(device.download_size == 0x6a00);
}
