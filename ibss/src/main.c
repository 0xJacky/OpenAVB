#include "avp/platform.h"
#include "avp/pvpanic.h"
#include "avp/simple_virtio.h"
#include "avp/source_image.h"
#include "avp/usb_device.h"


enum {
    USB_STATE_IPA = 0x70005000,
    SOURCE_IBSS_STAGE = 0x53494253,
};

void source_ibss_main(void) {
    pvpanic_initialize();
    USBDevice *state = (USBDevice *)USB_STATE_IPA;
    const uint32_t source_download_size = state->download_size;
    const AVPPlatformIdentity identity = state->identity;
    usb_device_initialize(
        state,
        &identity,
        (uint8_t *)SOURCE_IMAGE_UPLOAD_BASE,
        SOURCE_IMAGE_CAPACITY,
        source_image_validate,
        USB_MODE_RECOVERY
    );
    state->download_size = source_download_size;

    platform_stage(SOURCE_IBSS_STAGE);

    SimpleVirtioUSB usb;
    simple_virtio_usb_attach(&usb);
    simple_virtio_usb_post_event(&usb);

    for (;;) {
        const uint32_t event_size = simple_virtio_usb_wait_event(&usb);
        const USBEventResult result = usb_device_handle_event(
            state,
            usb.queue0_buffer,
            event_size,
            usb.queue1_buffer,
            SIMPLE_VIRTIO_QUEUE_CAPACITY
        );
        simple_virtio_usb_respond(&usb, result.response_size);
        if (result.action == USB_FIRMWARE_ACTION_REPOST_EVENT) {
            simple_virtio_usb_post_event(&usb);
        }
    }
}
