#include "avp/devices/usb/usb_dfu.h"
#include "avp/image/boot/source_image.h"
#include "avp/devices/usb/usb_identity.h"


enum {
    USB_EVENT_TRANSFER = 1,
    USB_EVENT_RESET = 2,
    USB_EVENT_PAUSE = 3,
    USB_RESPONSE_TRANSFER = 1,
    USB_RESPONSE_STALL = 2,
    USB_RESPONSE_RESET = 4,
    USB_RESPONSE_PAUSE = 5,
    USB_DESCRIPTOR_DEVICE = 1,
    USB_DESCRIPTOR_CONFIGURATION = 2,
    USB_DESCRIPTOR_STRING = 3,
    USB_REQUEST_GET_CONFIGURATION = 8,
    USB_REQUEST_SET_CONFIGURATION = 9,
    USB_REQUEST_SET_ADDRESS = 5,
    USB_REQUEST_GET_DESCRIPTOR = 6,
    USB_REQUEST_SET_INTERFACE = 11,
    DFU_REQUEST_DOWNLOAD = 1,
    DFU_REQUEST_GET_STATUS = 3,
    DFU_REQUEST_CLEAR_STATUS = 4,
    DFU_REQUEST_GET_STATE = 5,
    DFU_REQUEST_ABORT = 6,
    DFU_STATE_IDLE = 2,
    DFU_STATE_DOWNLOAD_IDLE = 5,
    DFU_STATE_MANIFEST_SYNC = 6,
    DFU_STATE_MANIFEST = 7,
    DFU_STATE_ERROR = 10,
    DFU_STATUS_OK = 0,
    DFU_STATUS_ERR_VERIFY = 7,
    SOURCE_IBSS_EXECUTION_MARKER = 0x53494253,
    SOURCE_TELEMETRY_REQUEST = 0x5a,
};

static const uint8_t device_descriptor[] = {
    0x12, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x40,
    0xac, 0x05, 0x27, 0x12, 0x00, 0x00, 0x02, 0x03,
    0x04, 0x01,
};

static const uint8_t configuration_descriptor[] = {
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

static uint32_t minimum(uint32_t left, uint32_t right) {
    return left < right ? left : right;
}

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

void usb_dfu_initialize(USBDFUState *state, USBRuntimeMode mode) {
    state->address = 0;
    state->configuration = 0;
    state->dfu_state = DFU_STATE_IDLE;
    state->manifestation_step = 0;
    state->mode = (uint8_t)mode;
    state->dfu_status = DFU_STATUS_OK;
    state->image_validated = 0;
    state->source_image_executed = mode == USB_MODE_RECOVERY ? 1 : 0;
    state->download_size = 0;
    state->next_block = 0;
    state->last_block = UINT16_MAX;
    state->recovery_received_bytes = 0;
    state->recovery_chunk_count = 0;
    state->recovery_zlp_count = 0;
}

static uint32_t emit_descriptor(
    const USBSetupPacket *setup,
    uint8_t *output,
    uint32_t capacity,
    const AVPPlatformIdentity *identity,
    const USBDFUState *state
) {
    const uint8_t type = (uint8_t)(setup->value >> 8);
    const uint8_t index = (uint8_t)setup->value;
    if (type == USB_DESCRIPTOR_DEVICE) {
        uint8_t descriptor[sizeof(device_descriptor)];
        copy_bytes(descriptor, device_descriptor, sizeof(descriptor));
        const uint16_t product_id = usb_identity_product_id(
            identity,
            state->mode == USB_MODE_RECOVERY
        );
        descriptor[10] = (uint8_t)product_id;
        descriptor[11] = (uint8_t)(product_id >> 8);
        const uint32_t size = minimum(
            minimum((uint32_t)sizeof(descriptor), setup->length),
            capacity
        );
        copy_bytes(output, descriptor, size);
        return size;
    }
    if (type == USB_DESCRIPTOR_CONFIGURATION) {
        const bool recovery = state->mode == USB_MODE_RECOVERY;
        const uint8_t *source = recovery
            ? recovery_configuration_descriptor
            : configuration_descriptor;
        const uint32_t source_size = recovery
            ? sizeof(recovery_configuration_descriptor)
            : sizeof(configuration_descriptor);
        const uint32_t size = minimum(minimum(source_size, setup->length), capacity);
        copy_bytes(output, source, size);
        return size;
    }
    if (type == USB_DESCRIPTOR_STRING) {
        return usb_identity_emit_string(
            index,
            output,
            capacity,
            setup->length,
            identity,
            state->mode == USB_MODE_RECOVERY,
            state->source_image_executed != 0
        );
    }
    return UINT32_MAX;
}

static uint32_t handle_standard_request(
    USBDFUState *state,
    const USBSetupPacket *setup,
    uint8_t *payload,
    uint32_t capacity,
    const AVPPlatformIdentity *identity
) {
    if (setup->request == USB_REQUEST_GET_DESCRIPTOR &&
        (setup->request_type & 0x80) != 0) {
        return emit_descriptor(setup, payload, capacity, identity, state);
    }
    if (setup->request == USB_REQUEST_SET_ADDRESS && setup->length == 0) {
        state->address = (uint8_t)setup->value;
        return 0;
    }
    if (setup->request == USB_REQUEST_SET_CONFIGURATION && setup->length == 0) {
        state->configuration = (uint8_t)setup->value;
        return 0;
    }
    if (setup->request == USB_REQUEST_GET_CONFIGURATION && setup->length == 1 && capacity != 0) {
        payload[0] = state->configuration;
        return 1;
    }
    if (state->mode == USB_MODE_RECOVERY &&
        setup->request == USB_REQUEST_SET_INTERFACE && setup->length == 0) {
        return 0;
    }
    return UINT32_MAX;
}

static uint32_t handle_dfu_request(
    USBDFUState *state,
    const USBSetupPacket *setup,
    const uint8_t *out_payload,
    uint32_t out_payload_size,
    uint8_t *payload,
    uint32_t capacity
) {
    if (setup->request == DFU_REQUEST_DOWNLOAD && (setup->request_type & 0x80) == 0) {
        if (setup->length == 0) {
            const bool valid_terminal_block =
                state->dfu_state == DFU_STATE_DOWNLOAD_IDLE &&
                setup->value == state->next_block &&
                setup->index == 0 && out_payload_size == 0;
            state->image_validated = valid_terminal_block &&
                source_image_validate(state->download_size);
            state->dfu_status = state->image_validated
                ? DFU_STATUS_OK
                : DFU_STATUS_ERR_VERIFY;
            state->dfu_state = state->image_validated
                ? DFU_STATE_MANIFEST_SYNC
                : DFU_STATE_ERROR;
            state->manifestation_step = 0;
        } else {
            const bool first_block = state->dfu_state == DFU_STATE_IDLE;
            if (first_block) {
                state->download_size = 0;
                state->next_block = 0;
                state->last_block = UINT16_MAX;
                state->image_validated = 0;
                state->dfu_status = DFU_STATUS_OK;
            }
            const bool next_block = setup->value == state->next_block;
            const bool split_footer = setup->length == 16 &&
                state->last_block == setup->value &&
                state->next_block == (uint16_t)(setup->value + 1);
            if (setup->index != 0 || setup->length > 0x800 ||
                setup->length != out_payload_size ||
                (!next_block && !split_footer) ||
                state->download_size > SOURCE_IMAGE_CAPACITY - setup->length) {
                state->dfu_status = DFU_STATUS_ERR_VERIFY;
                state->dfu_state = DFU_STATE_ERROR;
                return 0;
            }
            copy_bytes(
                (uint8_t *)SOURCE_IMAGE_UPLOAD_BASE + state->download_size,
                out_payload,
                out_payload_size
            );
            state->download_size += out_payload_size;
            if (next_block) {
                state->last_block = setup->value;
                ++state->next_block;
            }
            state->dfu_state = DFU_STATE_DOWNLOAD_IDLE;
        }
        return 0;
    }
    if (setup->request == DFU_REQUEST_GET_STATUS && setup->length == 6 && capacity >= 6) {
        payload[0] = state->dfu_status;
        payload[1] = 0;
        payload[2] = 0;
        payload[3] = 0;
        payload[4] = state->dfu_state;
        payload[5] = 0;
        if (state->dfu_state == DFU_STATE_MANIFEST_SYNC) {
            state->dfu_state = DFU_STATE_MANIFEST;
            state->manifestation_step = 1;
        }
        return 6;
    }
    if (setup->request == DFU_REQUEST_GET_STATE && setup->length == 1 && capacity != 0) {
        payload[0] = state->dfu_state;
        return 1;
    }
    if ((setup->request == DFU_REQUEST_CLEAR_STATUS || setup->request == DFU_REQUEST_ABORT) &&
        setup->length == 0) {
        state->dfu_state = DFU_STATE_IDLE;
        state->manifestation_step = 0;
        return 0;
    }
    return UINT32_MAX;
}

static uint32_t handle_vendor_request(
    USBDFUState *state,
    const USBSetupPacket *setup,
    uint8_t *payload,
    uint32_t capacity
) {
    if (state->mode != USB_MODE_RECOVERY) {
        return UINT32_MAX;
    }
    if (setup->request_type == 0x41 && setup->request == 0 &&
        setup->length == 0) {
        state->recovery_received_bytes = 0;
        state->recovery_chunk_count = 0;
        state->recovery_zlp_count = 0;
        return 0;
    }
    if (setup->request_type == 0xc0 &&
        setup->request == SOURCE_TELEMETRY_REQUEST &&
        setup->length == 20 && capacity >= 20) {
        write_u32_le(payload, SOURCE_IBSS_EXECUTION_MARKER);
        write_u32_le(payload + 4, state->download_size);
        write_u32_le(payload + 8, state->recovery_received_bytes);
        write_u32_le(payload + 12, state->recovery_chunk_count);
        write_u32_le(payload + 16, state->recovery_zlp_count);
        return 20;
    }
    return UINT32_MAX;
}

static USBEventResult short_response(uint8_t *response, uint8_t code, uint8_t endpoint) {
    response[0] = code;
    response[1] = endpoint;
    return (USBEventResult){2, USB_FIRMWARE_ACTION_NONE};
}

USBEventResult usb_dfu_handle_event(
    USBDFUState *state,
    const uint8_t *event,
    uint32_t event_size,
    uint8_t *response,
    uint32_t response_capacity,
    const AVPPlatformIdentity *identity
) {
    if (event_size < 6 || response_capacity < 2) {
        return (USBEventResult){0, USB_FIRMWARE_ACTION_NONE};
    }
    const uint32_t declared_size = read_u32_le(event);
    const uint8_t endpoint = event[4];
    const uint8_t type = event[5];
    if (declared_size != event_size - 6) {
        return short_response(response, USB_RESPONSE_STALL, endpoint);
    }
    if (type == USB_EVENT_RESET) {
        const bool validated_handoff = state->mode == USB_MODE_DFU &&
            state->dfu_state == DFU_STATE_MANIFEST &&
            state->manifestation_step == 1 &&
            state->image_validated != 0;
        if (!validated_handoff && state->mode == USB_MODE_DFU) {
            usb_dfu_initialize(state, USB_MODE_DFU);
        } else {
            state->address = 0;
            state->configuration = 0;
        }
        response[0] = USB_RESPONSE_RESET;
        response[1] = endpoint;
        return (USBEventResult){
            2,
            validated_handoff
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
        if (state->mode == USB_MODE_RECOVERY && endpoint == 4) {
            state->recovery_received_bytes += declared_size;
            ++state->recovery_chunk_count;
            if (declared_size == 0) {
                ++state->recovery_zlp_count;
            }
        }
        return short_response(response, USB_RESPONSE_TRANSFER, endpoint);
    }
    if (event_size < 14) {
        return short_response(response, USB_RESPONSE_STALL, 0);
    }

    const USBSetupPacket *setup = (const USBSetupPacket *)(event + 6);
    const bool device_to_host = (setup->request_type & 0x80) != 0;
    const uint32_t expected_declared_size = device_to_host
        ? sizeof(USBSetupPacket)
        : sizeof(USBSetupPacket) + setup->length;
    if (declared_size != expected_declared_size) {
        return short_response(response, USB_RESPONSE_STALL, 0);
    }
    const uint8_t *out_payload = event + 6 + sizeof(USBSetupPacket);
    const uint32_t out_payload_size = device_to_host ? 0 : setup->length;
    uint32_t payload_size;
    if ((setup->request_type & 0x60) == 0) {
        payload_size = handle_standard_request(
            state,
            setup,
            response + 2,
            response_capacity - 2,
            identity
        );
    } else if ((setup->request_type & 0x60) == 0x20) {
        payload_size = handle_dfu_request(
            state,
            setup,
            out_payload,
            out_payload_size,
            response + 2,
            response_capacity - 2
        );
    } else if ((setup->request_type & 0x60) == 0x40) {
        payload_size = handle_vendor_request(
            state,
            setup,
            response + 2,
            response_capacity - 2
        );
    } else {
        payload_size = UINT32_MAX;
    }
    if (payload_size == UINT32_MAX) {
        return short_response(response, USB_RESPONSE_STALL, 0);
    }
    response[0] = USB_RESPONSE_TRANSFER;
    response[1] = 0;
    return (USBEventResult){
        payload_size + 2,
        USB_FIRMWARE_ACTION_NONE,
    };
}
