#include "avp/devices/usb/usb_device.h"

#include "avp/devices/simplevirtio/simple_virtio_dcd.h"
#include "avp/devices/usb/usb_identity.h"
#include "avp/image/boot/patch_store.h"
#include "class/dfu/dfu.h"
#include "class/dfu/dfu_device.h"
#include "device/dcd.h"
#include "tusb.h"


enum {
    USB_EVENT_TRANSFER = 1,
    USB_EVENT_RESET = 2,
    USB_EVENT_PAUSE = 3,
    USB_RESPONSE_TRANSFER = 1,
    USB_RESPONSE_STALL = 2,
    USB_RESPONSE_RESET = 4,
    USB_RESPONSE_PAUSE = 5,
    DFU_REQUEST_DOWNLOAD = 1,
    DFU_REQUEST_GET_STATUS = 3,
    USB_REQUEST_GET_DESCRIPTOR = 6,
    USB_REQUEST_SET_CONFIGURATION = 9,
    USB_REQUEST_SET_INTERFACE = 11,
    SOURCE_IBSS_EXECUTION_MARKER = 0x53494253,
    SOURCE_TELEMETRY_REQUEST = 0x5a,
    PATCH_STORE_REQUEST_OPEN = 0x70,
    PATCH_STORE_REQUEST_APPEND = 0x71,
    PATCH_STORE_REQUEST_SEAL = 0x72,
    PATCH_STORE_REQUEST_REPORT = 0x73,
};

static const uint8_t device_descriptor[] = {
    0x12, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x40,
    0xac, 0x05, 0x27, 0x12, 0x00, 0x00, 0x02, 0x03,
    0x04, 0x01,
};

/* TinyUSB parses the DFU 1.1 version field while the adapter preserves the wire descriptor below. */
static const uint8_t configuration_descriptor[] = {
    0x09, 0x02, 0x1b, 0x00, 0x01, 0x01, 0x05, 0x80, 0xfa,
    0x09, 0x04, 0x00, 0x00, 0x00, 0xfe, 0x01, 0x02, 0x00,
    0x09, 0x21, 0x01, 0x0a, 0x00, 0x00, 0x08, 0x10, 0x01,
};

static const uint8_t wire_configuration_descriptor[] = {
    0x09, 0x02, 0x19, 0x00, 0x01, 0x01, 0x05, 0x80, 0xfa,
    0x09, 0x04, 0x00, 0x00, 0x00, 0xfe, 0x01, 0x00, 0x00,
    0x07, 0x21, 0x01, 0x0a, 0x00, 0x00, 0x08,
};

static const uint8_t recovery_configuration_descriptor[] = {
    0x09, 0x02, 0x2b, 0x00, 0x02, 0x01, 0x00, 0x80, 0x32,
    0x09, 0x04, 0x00, 0x00, 0x01, 0xff, 0x00, 0x00, 0x00,
    0x07, 0x05, 0x04, 0x02, 0x00, 0x02, 0x00,
    0x09, 0x04, 0x01, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00,
    0x09, 0x04, 0x01, 0x01, 0x00, 0xff, 0x00, 0x00, 0x00,
};

static USBDevice *active_device;
static uint8_t dynamic_device_descriptor[sizeof(device_descriptor)];
static uint16_t string_descriptor[128];

static void copy_bytes(uint8_t *destination, const uint8_t *source, uint32_t size) {
    for (uint32_t index = 0; index < size; ++index) {
        destination[index] = source[index];
    }
}

static uint32_t read_u32_le(const uint8_t *bytes) {
    return (uint32_t)bytes[0] |
        (uint32_t)bytes[1] << 8 |
        (uint32_t)bytes[2] << 16 |
        (uint32_t)bytes[3] << 24;
}

static void write_u32_le(uint8_t *bytes, uint32_t value) {
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
    bytes[2] = (uint8_t)(value >> 16);
    bytes[3] = (uint8_t)(value >> 24);
}

const uint8_t *tud_descriptor_device_cb(void) {
    copy_bytes(
        dynamic_device_descriptor,
        device_descriptor,
        sizeof(dynamic_device_descriptor)
    );
    const uint16_t product_id = usb_identity_product_id(
        &active_device->identity,
        active_device->mode == USB_MODE_RECOVERY
    );
    dynamic_device_descriptor[10] = (uint8_t)product_id;
    dynamic_device_descriptor[11] = (uint8_t)(product_id >> 8);
    return dynamic_device_descriptor;
}

const uint8_t *tud_descriptor_configuration_cb(uint8_t index) {
    (void)index;
    return configuration_descriptor;
}

const uint16_t *tud_descriptor_string_cb(uint8_t index, uint16_t language_id) {
    (void)language_id;
    const uint32_t size = usb_identity_emit_string(
        index,
        (uint8_t *)string_descriptor,
        sizeof(string_descriptor),
        UINT16_MAX,
        &active_device->identity,
        active_device->mode == USB_MODE_RECOVERY,
        active_device->source_image_executed != 0
    );
    return size == UINT32_MAX ? 0 : string_descriptor;
}

uint32_t tud_dfu_get_timeout_cb(uint8_t alt, uint8_t state) {
    (void)alt;
    return state == DFU_DNBUSY ? 50 : 0;
}

void tud_dfu_download_cb(
    uint8_t alt,
    uint16_t block_num,
    const uint8_t *data,
    uint16_t length
) {
    (void)usb_dfu_download_data_stage_complete(
        &active_device->dfu,
        alt,
        block_num,
        data,
        length
    );
    active_device->download_size = active_device->dfu.transport_length;
    active_device->next_block = active_device->dfu.expected_block;
    active_device->last_block = active_device->dfu.last_block;
}

void tud_dfu_manifest_cb(uint8_t alt) {
    const bool image_valid = alt == 0 &&
        active_device->terminal_block_valid != 0 &&
        active_device->validate_image(active_device->dfu.transport_length);
    active_device->image_validated = image_valid &&
        usb_dfu_manifestation_complete(&active_device->dfu, alt);
    if (!image_valid) {
        (void)usb_dfu_mark_download_failed_and_get_event_handle(
            &active_device->dfu,
            DFU_STATUS_ERR_VERIFY
        );
    }
    active_device->manifestation_started =
        active_device->dfu.manifestation_started;
    active_device->download_size = active_device->dfu.transport_length;
}

static void pump_tinyusb(void) {
    while (tud_task_event_ready()) {
        tud_task();
    }
}

static void settle_download_block(void) {
    static const uint8_t get_status[] = {
        0xa1, 0x03, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00,
    };
    uint8_t status[6];
    for (uint8_t poll = 0; poll < 2; ++poll) {
        SimpleVirtioDCDTransfer transfer = {
            .in_payload = status,
            .in_payload_capacity = sizeof(status),
        };
        simple_virtio_dcd_begin(&transfer);
        dcd_event_setup_received(0, get_status, false);
        pump_tinyusb();
        simple_virtio_dcd_end();
        if (transfer.stalled) {
            __builtin_trap();
        }
    }
}

static void reset_download_state(USBDevice *device) {
    usb_dfu_download_prepare_destination(
        &device->dfu,
        device->download_buffer,
        device->download_capacity
    );
    device->download_size = 0;
    device->next_block = 0;
    device->last_block = UINT16_MAX;
    device->image_validated = 0;
    device->manifestation_started = 0;
    device->manifestation_status_count = 0;
    device->terminal_block_valid = 0;
}

void usb_device_initialize(
    USBDevice *device,
    const AVPPlatformIdentity *identity,
    uint8_t *download_buffer,
    uint32_t download_capacity,
    USBDeviceValidateImage validate_image,
    USBRuntimeMode mode
) {
    if (tud_inited()) {
        (void)tusb_deinit(0);
    }
    device->identity = *identity;
    device->download_buffer = download_buffer;
    device->download_capacity = download_capacity;
    device->validate_image = validate_image;
    usb_dfu_protocol_initialize(&device->dfu, 1);
    reset_download_state(device);
    device->mode = (uint8_t)mode;
    device->source_image_executed = mode == USB_MODE_RECOVERY ? 1 : 0;
    device->configured = 0;
    device->recovery_received_bytes = 0;
    device->recovery_chunk_count = 0;
    device->recovery_zlp_count = 0;
    active_device = device;
    const tusb_rhport_init_t rhport = {
        .role = TUSB_ROLE_DEVICE,
        .speed = TUSB_SPEED_HIGH,
    };
    if (!tusb_init(0, &rhport)) {
        __builtin_trap();
    }
    dcd_event_bus_reset(0, TUSB_SPEED_HIGH, false);
    pump_tinyusb();
}

static USBEventResult short_response(uint8_t *response, uint8_t code, uint8_t endpoint) {
    response[0] = code;
    response[1] = endpoint;
    return (USBEventResult){2, USB_FIRMWARE_ACTION_NONE};
}

USBEventResult usb_device_handle_event(
    USBDevice *device,
    const uint8_t *event,
    uint32_t event_size,
    uint8_t *response,
    uint32_t response_capacity
) {
    if (event_size < 6 || response_capacity < 2) {
        return (USBEventResult){0, USB_FIRMWARE_ACTION_NONE};
    }
    active_device = device;
    const uint32_t declared_size = read_u32_le(event);
    const uint8_t endpoint = event[4];
    const uint8_t type = event[5];
    if (declared_size != event_size - 6) {
        return short_response(response, USB_RESPONSE_STALL, endpoint);
    }
    if (type == USB_EVENT_RESET) {
        const bool handoff = device->image_validated != 0 &&
            device->manifestation_started != 0;
        dcd_event_bus_reset(0, TUSB_SPEED_HIGH, false);
        pump_tinyusb();
        device->configured = 0;
        if (device->mode == USB_MODE_DFU && !handoff) {
            reset_download_state(device);
        }
        response[0] = USB_RESPONSE_RESET;
        response[1] = endpoint;
        return (USBEventResult){
            2,
            handoff
                ? USB_FIRMWARE_ACTION_HANDOFF
                : USB_FIRMWARE_ACTION_NONE,
        };
    }
    if (type == USB_EVENT_PAUSE) {
        return short_response(response, USB_RESPONSE_PAUSE, endpoint);
    }
    if (type != USB_EVENT_TRANSFER) {
        return short_response(response, USB_RESPONSE_STALL, endpoint);
    }
    if (endpoint != 0) {
        if (device->mode == USB_MODE_RECOVERY && endpoint == 4) {
            device->recovery_received_bytes += declared_size;
            ++device->recovery_chunk_count;
            if (declared_size == 0) {
                ++device->recovery_zlp_count;
            }
        }
        return short_response(response, USB_RESPONSE_TRANSFER, endpoint);
    }
    if (event_size < 14) {
        return short_response(response, USB_RESPONSE_STALL, endpoint);
    }

    const USBSetupPacket *setup = (const USBSetupPacket *)(event + 6);
    const bool device_to_host = (setup->request_type & 0x80) != 0;
    const uint32_t expected_size = device_to_host
        ? sizeof(USBSetupPacket)
        : sizeof(USBSetupPacket) + setup->length;
    if (declared_size != expected_size) {
        return short_response(response, USB_RESPONSE_STALL, endpoint);
    }
    if (setup->request_type == 0x21 &&
        setup->request == DFU_REQUEST_DOWNLOAD &&
        setup->length == 0) {
        device->terminal_block_valid = setup->index == 0 &&
            setup->value == device->next_block;
        device->manifestation_status_count = 0;
    }
    /*
     * The patch table, uploaded rather than compiled in.
     *
     * These are answered in either mode: the tables for iBSS and for iBEC have
     * to be in place before the image runs, which is while the ROM is still the
     * DFU device, and the ones for the monitor and the kernel are uploaded the
     * same way in the same session.
     *
     *   0x70  out  wValue=stage                      open a stage
     *   0x71  out  wValue=stage wIndex=chunk + data  append, in order
     *   0x72  out  wValue=stage wIndex=count         seal, count must agree
     *   0x73  in                                     what the ROM now holds
     */
    if (setup->request_type == 0x41 &&
        setup->request == PATCH_STORE_REQUEST_OPEN && setup->length == 0) {
        return short_response(
            response,
            patch_store_open(setup->value) ? USB_RESPONSE_TRANSFER : USB_RESPONSE_STALL,
            0
        );
    }
    if (setup->request_type == 0x41 &&
        setup->request == PATCH_STORE_REQUEST_APPEND && setup->length != 0) {
        return short_response(
            response,
            patch_store_append(setup->value, setup->index, event + 14, setup->length)
                ? USB_RESPONSE_TRANSFER
                : USB_RESPONSE_STALL,
            0
        );
    }
    if (setup->request_type == 0x41 &&
        setup->request == PATCH_STORE_REQUEST_SEAL && setup->length == 0) {
        return short_response(
            response,
            patch_store_seal(setup->value, setup->index) ? USB_RESPONSE_TRANSFER : USB_RESPONSE_STALL,
            0
        );
    }
    if (setup->request_type == 0xc0 &&
        setup->request == PATCH_STORE_REQUEST_REPORT &&
        setup->length >= PATCH_STAGE_COUNT * 12 &&
        response_capacity >= PATCH_STAGE_COUNT * 12 + 2) {
        const uint32_t written =
            patch_store_report(response + 2, response_capacity - 2);

        response[0] = USB_RESPONSE_TRANSFER;
        response[1] = 0;
        return (USBEventResult){(uint32_t)(written + 2), USB_FIRMWARE_ACTION_NONE};
    }
    if (device->mode == USB_MODE_RECOVERY &&
        setup->request_type == 0x41 && setup->request == 0 &&
        setup->length == 0) {
        device->recovery_received_bytes = 0;
        device->recovery_chunk_count = 0;
        device->recovery_zlp_count = 0;
        return short_response(response, USB_RESPONSE_TRANSFER, 0);
    }
    if (device->mode == USB_MODE_RECOVERY &&
        setup->request_type == 0xc0 &&
        setup->request == SOURCE_TELEMETRY_REQUEST &&
        setup->length == 20 && response_capacity >= 22) {
        response[0] = USB_RESPONSE_TRANSFER;
        response[1] = 0;
        write_u32_le(response + 2, SOURCE_IBSS_EXECUTION_MARKER);
        write_u32_le(response + 6, device->download_size);
        write_u32_le(response + 10, device->recovery_received_bytes);
        write_u32_le(response + 14, device->recovery_chunk_count);
        write_u32_le(response + 18, device->recovery_zlp_count);
        return (USBEventResult){22, USB_FIRMWARE_ACTION_NONE};
    }
    if (device->mode == USB_MODE_RECOVERY &&
        (setup->request_type & 0x60) == 0 &&
        setup->request == USB_REQUEST_SET_INTERFACE && setup->length == 0) {
        return short_response(response, USB_RESPONSE_TRANSFER, 0);
    }

    SimpleVirtioDCDTransfer transfer = {
        .out_payload = event + 14,
        .out_payload_size = device_to_host ? 0 : setup->length,
        .in_payload = response + 2,
        .in_payload_capacity = response_capacity - 2,
    };
    simple_virtio_dcd_begin(&transfer);
    /* AVPBooter accepts DFU class requests before Host configuration. */
    if ((setup->request_type & 0x60) == 0x20 && !device->configured) {
        static const uint8_t set_configuration[] = {
            0x00, USB_REQUEST_SET_CONFIGURATION, 0x01, 0x00,
            0x00, 0x00, 0x00, 0x00,
        };
        dcd_event_setup_received(0, set_configuration, false);
        pump_tinyusb();
        device->configured = 1;
    }
    dcd_event_setup_received(0, event + 6, false);
    pump_tinyusb();
    simple_virtio_dcd_end();
    if (!transfer.stalled) {
        if (setup->request_type == 0x21 &&
            setup->request == DFU_REQUEST_DOWNLOAD && setup->length != 0) {
            settle_download_block();
        }
        if (setup->request_type == 0x00 &&
            setup->request == USB_REQUEST_SET_CONFIGURATION &&
            setup->value == 1) {
            device->configured = 1;
        }
        if (setup->request_type == 0xa1 &&
            setup->request == DFU_REQUEST_GET_STATUS && setup->length == 6 &&
            response[6] == DFU_DNLOAD_IDLE) {
            /* The bridge settles TinyUSB internally; preserve AVPBooter's wire timeout. */
            response[3] = 50;
        }
        if (setup->request_type == 0xa1 &&
            setup->request == DFU_REQUEST_GET_STATUS && setup->length == 6 &&
            device->image_validated != 0) {
            /* The USB/IP bridge observes AVPBooter's 6 -> 7 manifestation sequence. */
            response[6] = device->manifestation_status_count == 0 ? 6 : 7;
            device->manifestation_status_count = 1;
        }
        if (setup->request_type == 0x80 &&
            setup->request == USB_REQUEST_GET_DESCRIPTOR &&
            (setup->value >> 8) == 2) {
            const uint8_t *wire_descriptor = device->mode == USB_MODE_RECOVERY
                ? recovery_configuration_descriptor
                : wire_configuration_descriptor;
            const uint32_t wire_descriptor_size = device->mode == USB_MODE_RECOVERY
                ? sizeof(recovery_configuration_descriptor)
                : sizeof(wire_configuration_descriptor);
            transfer.in_payload_size = setup->length < wire_descriptor_size
                ? setup->length
                : wire_descriptor_size;
            copy_bytes(
                response + 2,
                wire_descriptor,
                transfer.in_payload_size
            );
        }
    }
    response[0] = transfer.stalled
        ? USB_RESPONSE_STALL
        : USB_RESPONSE_TRANSFER;
    response[1] = endpoint;
    return (USBEventResult){
        transfer.stalled ? 2 : transfer.in_payload_size + 2,
        USB_FIRMWARE_ACTION_NONE,
    };
}
