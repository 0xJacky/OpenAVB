#include "avp/devices/usb/usb_dfu_protocol_adapter.h"

#include "class/dfu/dfu.h"
#include "class/dfu/dfu_device.h"

#include <limits.h>


enum { USB_DFU_AVP_STATUS_OK = 0x3200 };

static void copy_bytes(
    uint8_t *destination,
    const uint8_t *source,
    uint16_t length
) {
    for (uint16_t index = 0; index < length; ++index) {
        destination[index] = source[index];
    }
}

bool usb_dfu_download_buffer_tuple_validate(
    const uint8_t *destination,
    uint32_t destination_capacity
) {
    return destination != 0 &&
        destination_capacity >= USB_DFU_PROTOCOL_FOOTER_SIZE;
}

void usb_dfu_protocol_initialize(
    USBDFUProtocolAdapter *adapter,
    uint16_t event_handle
) {
    *adapter = (USBDFUProtocolAdapter){
        .accepted_image_length = UINT32_MAX,
        .last_block = UINT16_MAX,
        .event_handle = event_handle,
        .avp_status_code = USB_DFU_AVP_STATUS_OK,
        .state = DFU_IDLE,
        .status = DFU_STATUS_OK,
        .initialized = 1,
    };
}

void usb_dfu_protocol_deinitialize(USBDFUProtocolAdapter *adapter) {
    *adapter = (USBDFUProtocolAdapter){0};
}

void usb_dfu_download_prepare_destination(
    USBDFUProtocolAdapter *adapter,
    uint8_t *destination,
    uint32_t destination_capacity
) {
    adapter->destination = destination;
    adapter->destination_capacity = destination_capacity;
    adapter->transport_length = 0;
    adapter->accepted_image_length = UINT32_MAX;
    adapter->expected_block = 0;
    adapter->last_block = UINT16_MAX;
    adapter->complete = 0;
    adapter->manifestation_started = 0;
    adapter->avp_status_code = USB_DFU_AVP_STATUS_OK;
    adapter->status = DFU_STATUS_OK;
    adapter->state = DFU_IDLE;
}

USBDFUProtocolAdapter *dfu_protocol_state_address_to_x8(
    USBDFUProtocolAdapter *adapter
) {
    return adapter;
}

void dfu_protocol_outlined_set_status_code_and_state2(
    USBDFUProtocolAdapter *adapter,
    uint16_t status_code,
    uint8_t tinyusb_status
) {
    adapter->avp_status_code = status_code;
    adapter->status = tinyusb_status;
    adapter->state = DFU_IDLE;
}

uint16_t usb_dfu_mark_download_failed_and_get_event_handle(
    USBDFUProtocolAdapter *adapter,
    uint8_t status
) {
    adapter->accepted_image_length = UINT32_MAX;
    adapter->status = status;
    adapter->state = DFU_ERROR;
    adapter->complete = 1;
    tud_dfu_finish_flashing(status);
    return adapter->event_handle;
}

bool usb_dfu_download_data_stage_complete(
    USBDFUProtocolAdapter *adapter,
    uint8_t alternate_setting,
    uint16_t block_number,
    const uint8_t *payload,
    uint16_t payload_length
) {
    const bool next_block = block_number == adapter->expected_block;
    const bool split_footer = payload_length == USB_DFU_PROTOCOL_FOOTER_SIZE &&
        block_number == adapter->last_block &&
        adapter->expected_block == (uint16_t)(block_number + 1);
    const bool valid = adapter->initialized != 0 &&
        alternate_setting == 0 &&
        (next_block || split_footer) &&
        payload_length != 0 &&
        payload_length <= USB_DFU_PROTOCOL_TRANSFER_SIZE &&
        payload != 0 &&
        usb_dfu_download_buffer_tuple_validate(
            adapter->destination,
            adapter->destination_capacity
        ) &&
        adapter->transport_length <=
            adapter->destination_capacity - payload_length;
    if (!valid) {
        (void)usb_dfu_mark_download_failed_and_get_event_handle(
            adapter,
            DFU_STATUS_ERR_ADDRESS
        );
        return false;
    }

    copy_bytes(
        adapter->destination + adapter->transport_length,
        payload,
        payload_length
    );
    adapter->transport_length += payload_length;
    if (next_block) {
        adapter->last_block = block_number;
        ++adapter->expected_block;
    }
    adapter->avp_status_code = USB_DFU_AVP_STATUS_OK;
    adapter->status = DFU_STATUS_OK;
    adapter->state = DFU_DNLOAD_IDLE;
    tud_dfu_finish_flashing(DFU_STATUS_OK);
    return true;
}

bool usb_dfu_manifestation_complete(
    USBDFUProtocolAdapter *adapter,
    uint8_t alternate_setting
) {
    const bool valid = adapter->initialized != 0 &&
        alternate_setting == 0 &&
        adapter->state == DFU_DNLOAD_IDLE &&
        adapter->transport_length >= USB_DFU_PROTOCOL_FOOTER_SIZE;
    adapter->manifestation_started = 1;
    if (!valid) {
        (void)usb_dfu_mark_download_failed_and_get_event_handle(
            adapter,
            DFU_STATUS_ERR_VERIFY
        );
        return false;
    }

    adapter->accepted_image_length =
        adapter->transport_length - USB_DFU_PROTOCOL_FOOTER_SIZE;
    adapter->avp_status_code = USB_DFU_AVP_STATUS_OK;
    adapter->status = DFU_STATUS_OK;
    adapter->state = DFU_MANIFEST_WAIT_RESET;
    adapter->complete = 1;
    tud_dfu_finish_flashing(DFU_STATUS_OK);
    return true;
}

int dfu_protocol_event_handle(
    USBDFUProtocolAdapter *adapter,
    const USBDFUProtocolEvent *event
) {
    switch (event->kind) {
        case USB_DFU_PROTOCOL_EVENT_DOWNLOAD:
            return usb_dfu_download_data_stage_complete(
                adapter,
                event->alternate_setting,
                event->block_number,
                event->payload,
                event->payload_length
            ) ? 0 : -1;
        case USB_DFU_PROTOCOL_EVENT_MANIFEST:
            return usb_dfu_manifestation_complete(
                adapter,
                event->alternate_setting
            ) ? 0 : -1;
        case USB_DFU_PROTOCOL_EVENT_ABORT:
            (void)usb_dfu_mark_download_failed_and_get_event_handle(
                adapter,
                DFU_STATUS_ERR_NOTDONE
            );
            return -1;
    }
    (void)usb_dfu_mark_download_failed_and_get_event_handle(
        adapter,
        DFU_STATUS_ERR_STALLEDPKT
    );
    return -1;
}

bool usb_dfu_download_is_complete(const USBDFUProtocolAdapter *adapter) {
    return adapter->complete != 0;
}

uint16_t usb_dfu_download_event_handle(
    const USBDFUProtocolAdapter *adapter
) {
    return adapter->event_handle;
}

uint32_t dfu_download_wait(const USBDFUProtocolAdapter *adapter) {
    return usb_dfu_download_is_complete(adapter)
        ? adapter->accepted_image_length
        : UINT32_MAX;
}

uint64_t dfu_protocol_source_token(void) {
    return USB_DFU_PROTOCOL_SOURCE_TOKEN;
}
