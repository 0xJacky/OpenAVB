#pragma once

#include "avp/platform/platform.h"

#include <stdint.h>


typedef struct __attribute__((packed)) {
    uint8_t request_type;
    uint8_t request;
    uint16_t value;
    uint16_t index;
    uint16_t length;
} USBSetupPacket;

_Static_assert(sizeof(USBSetupPacket) == 8, "USBSetupPacket size");

typedef struct {
    uint8_t address;
    uint8_t configuration;
    uint8_t dfu_state;
    uint8_t manifestation_step;
    uint8_t mode;
    uint8_t dfu_status;
    uint8_t image_validated;
    uint8_t source_image_executed;
    uint32_t download_size;
    uint16_t next_block;
    uint16_t last_block;
    uint32_t recovery_received_bytes;
    uint32_t recovery_chunk_count;
    uint32_t recovery_zlp_count;
} USBDFUState;

typedef enum {
    USB_MODE_DFU = 0,
    USB_MODE_RECOVERY = 1,
} USBRuntimeMode;

typedef enum {
    USB_FIRMWARE_ACTION_NONE = 0,
    USB_FIRMWARE_ACTION_HANDOFF = 1,
    USB_FIRMWARE_ACTION_REPOST_EVENT = 2,
} USBFirmwareAction;

typedef struct {
    uint32_t response_size;
    USBFirmwareAction action;
} USBEventResult;

void usb_dfu_initialize(USBDFUState *state, USBRuntimeMode mode);

USBEventResult usb_dfu_handle_event(
    USBDFUState *state,
    const uint8_t *event,
    uint32_t event_size,
    uint8_t *response,
    uint32_t response_capacity,
    const AVPPlatformIdentity *identity
);
