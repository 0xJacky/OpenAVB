#pragma once

#include "avp/platform/platform.h"
#include "avp/devices/usb/usb_dfu.h"
#include "avp/devices/usb/usb_dfu_protocol_adapter.h"

#include <stdbool.h>
#include <stdint.h>


typedef bool (*USBDeviceValidateImage)(uint32_t download_size);

typedef struct {
    AVPPlatformIdentity identity;
    uint8_t *download_buffer;
    uint32_t download_capacity;
    USBDeviceValidateImage validate_image;
    USBDFUProtocolAdapter dfu;
    uint32_t download_size;
    uint16_t next_block;
    uint16_t last_block;
    uint8_t image_validated;
    uint8_t manifestation_started;
    uint8_t manifestation_status_count;
    uint8_t terminal_block_valid;
    uint8_t mode;
    uint8_t source_image_executed;
    uint8_t configured;
    uint32_t recovery_received_bytes;
    uint32_t recovery_chunk_count;
    uint32_t recovery_zlp_count;
} USBDevice;

void usb_device_initialize(
    USBDevice *device,
    const AVPPlatformIdentity *identity,
    uint8_t *download_buffer,
    uint32_t download_capacity,
    USBDeviceValidateImage validate_image,
    USBRuntimeMode mode
);

USBEventResult usb_device_handle_event(
    USBDevice *device,
    const uint8_t *event,
    uint32_t event_size,
    uint8_t *response,
    uint32_t response_capacity
);
