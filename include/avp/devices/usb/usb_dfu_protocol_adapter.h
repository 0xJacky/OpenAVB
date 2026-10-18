#pragma once

#include <stdbool.h>
#include <stdint.h>


enum {
    USB_DFU_PROTOCOL_TRANSFER_SIZE = 0x800,
    USB_DFU_PROTOCOL_FOOTER_SIZE = 16,
};

#define USB_DFU_PROTOCOL_SOURCE_TOKEN UINT64_C(0x093b6b18e1328251)

typedef enum {
    USB_DFU_PROTOCOL_EVENT_DOWNLOAD = 1,
    USB_DFU_PROTOCOL_EVENT_MANIFEST = 2,
    USB_DFU_PROTOCOL_EVENT_ABORT = 3,
} USBDFUProtocolEventKind;

typedef struct {
    USBDFUProtocolEventKind kind;
    uint8_t alternate_setting;
    uint16_t block_number;
    const uint8_t *payload;
    uint16_t payload_length;
} USBDFUProtocolEvent;

typedef struct {
    uint8_t *destination;
    uint32_t destination_capacity;
    uint32_t transport_length;
    uint32_t accepted_image_length;
    uint16_t expected_block;
    uint16_t last_block;
    uint16_t event_handle;
    uint16_t avp_status_code;
    uint8_t state;
    uint8_t status;
    uint8_t initialized;
    uint8_t complete;
    uint8_t manifestation_started;
} USBDFUProtocolAdapter;

#define USB_DFU_PROTOCOL_PUBLIC __attribute__((used, retain))

USB_DFU_PROTOCOL_PUBLIC void usb_dfu_protocol_initialize(
    USBDFUProtocolAdapter *adapter,
    uint16_t event_handle
);
USB_DFU_PROTOCOL_PUBLIC void usb_dfu_protocol_deinitialize(
    USBDFUProtocolAdapter *adapter
);
USB_DFU_PROTOCOL_PUBLIC void usb_dfu_download_prepare_destination(
    USBDFUProtocolAdapter *adapter,
    uint8_t *destination,
    uint32_t destination_capacity
);
USB_DFU_PROTOCOL_PUBLIC bool usb_dfu_download_buffer_tuple_validate(
    const uint8_t *destination,
    uint32_t destination_capacity
);
USB_DFU_PROTOCOL_PUBLIC int dfu_protocol_event_handle(
    USBDFUProtocolAdapter *adapter,
    const USBDFUProtocolEvent *event
);
USB_DFU_PROTOCOL_PUBLIC bool usb_dfu_download_data_stage_complete(
    USBDFUProtocolAdapter *adapter,
    uint8_t alternate_setting,
    uint16_t block_number,
    const uint8_t *payload,
    uint16_t payload_length
);
USB_DFU_PROTOCOL_PUBLIC bool usb_dfu_manifestation_complete(
    USBDFUProtocolAdapter *adapter,
    uint8_t alternate_setting
);
USB_DFU_PROTOCOL_PUBLIC uint16_t
usb_dfu_mark_download_failed_and_get_event_handle(
    USBDFUProtocolAdapter *adapter,
    uint8_t status
);
USB_DFU_PROTOCOL_PUBLIC bool usb_dfu_download_is_complete(
    const USBDFUProtocolAdapter *adapter
);
USB_DFU_PROTOCOL_PUBLIC uint16_t usb_dfu_download_event_handle(
    const USBDFUProtocolAdapter *adapter
);
USB_DFU_PROTOCOL_PUBLIC uint32_t dfu_download_wait(
    const USBDFUProtocolAdapter *adapter
);
USB_DFU_PROTOCOL_PUBLIC USBDFUProtocolAdapter *
dfu_protocol_state_address_to_x8(
    USBDFUProtocolAdapter *adapter
);
USB_DFU_PROTOCOL_PUBLIC void
dfu_protocol_outlined_set_status_code_and_state2(
    USBDFUProtocolAdapter *adapter,
    uint16_t status_code,
    uint8_t tinyusb_status
);
USB_DFU_PROTOCOL_PUBLIC uint64_t dfu_protocol_source_token(void);
