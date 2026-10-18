#include "avp/image/boot/source_image.h"
#include "avp/devices/usb/usb_dfu.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>


bool source_image_validate(uint32_t download_size) {
    return download_size != 0;
}

int main(void) {
    AVPPlatformIdentity identity = {
        .ecid = 0x1a2b3c4d5e6f7081,
        .chip_id = 0xfe01,
        .security_domain = 1,
        .chip_fuse_mode = 3,
        .security_epoch = 1,
        .board_id = 0x90,
        .boot_flags = 0x3c,
        .variant = "vresearch101",
    };
    USBDFUState state;
    usb_dfu_initialize(&state, USB_MODE_DFU);

    const uint8_t pause_event[] = {0, 0, 0, 0, 4, 3};
    uint8_t response[16] = {};
    USBEventResult result = usb_dfu_handle_event(
        &state,
        pause_event,
        sizeof(pause_event),
        response,
        sizeof(response),
        &identity
    );
    assert(result.response_size == 2);
    assert(result.action == USB_FIRMWARE_ACTION_NONE);
    assert(response[0] == 5);
    assert(response[1] == 4);

    const uint8_t unknown_event[] = {0, 0, 0, 0, 2, 0xff};
    result = usb_dfu_handle_event(
        &state,
        unknown_event,
        sizeof(unknown_event),
        response,
        sizeof(response),
        &identity
    );
    assert(result.response_size == 2);
    assert(result.action == USB_FIRMWARE_ACTION_NONE);
    assert(response[0] == 2);
    assert(response[1] == 2);
}
