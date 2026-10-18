#include "avp/devices/usb/usb_dfu_protocol_adapter.h"

#include "class/dfu/dfu.h"

#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>


static uint8_t finish_statuses[16];
static uint8_t finish_count;

void tud_dfu_finish_flashing(uint8_t status) {
    assert(finish_count < sizeof(finish_statuses));
    finish_statuses[finish_count++] = status;
}

static void reset_finish_transcript(void) {
    finish_count = 0;
}

static void test_successful_download_and_manifest(void) {
    uint8_t destination[USB_DFU_PROTOCOL_TRANSFER_SIZE + 32] = {0};
    uint8_t first[USB_DFU_PROTOCOL_TRANSFER_SIZE];
    uint8_t terminal[32];
    for (uint32_t index = 0; index < sizeof(first); ++index) {
        first[index] = (uint8_t)index;
    }
    for (uint32_t index = 0; index < sizeof(terminal); ++index) {
        terminal[index] = (uint8_t)(0xa0 + index);
    }

    USBDFUProtocolAdapter adapter;
    usb_dfu_protocol_initialize(&adapter, 0x204);
    usb_dfu_download_prepare_destination(
        &adapter,
        destination,
        sizeof(destination)
    );
    reset_finish_transcript();

    const USBDFUProtocolEvent events[] = {
        {
            .kind = USB_DFU_PROTOCOL_EVENT_DOWNLOAD,
            .block_number = 0,
            .payload = first,
            .payload_length = sizeof(first),
        },
        {
            .kind = USB_DFU_PROTOCOL_EVENT_DOWNLOAD,
            .block_number = 1,
            .payload = terminal,
            .payload_length = sizeof(terminal),
        },
        {.kind = USB_DFU_PROTOCOL_EVENT_MANIFEST},
    };
    for (uint32_t index = 0; index < 3; ++index) {
        assert(dfu_protocol_event_handle(&adapter, &events[index]) == 0);
    }

    assert(finish_count == 3);
    assert(finish_statuses[0] == DFU_STATUS_OK);
    assert(finish_statuses[1] == DFU_STATUS_OK);
    assert(finish_statuses[2] == DFU_STATUS_OK);
    assert(adapter.transport_length == sizeof(destination));
    assert(adapter.accepted_image_length == sizeof(destination) - 16);
    assert(adapter.state == DFU_MANIFEST_WAIT_RESET);
    assert(usb_dfu_download_is_complete(&adapter));
    assert(usb_dfu_download_event_handle(&adapter) == 0x204);
    assert(dfu_download_wait(&adapter) == sizeof(destination) - 16);
    for (uint32_t index = 0; index < sizeof(first); ++index) {
        assert(destination[index] == first[index]);
    }
    for (uint32_t index = 0; index < sizeof(terminal); ++index) {
        assert(destination[sizeof(first) + index] == terminal[index]);
    }
}

static void test_footer_can_extend_the_last_data_block(void) {
    uint8_t destination[48] = {0};
    uint8_t payload[32] = {0x41};
    uint8_t footer[USB_DFU_PROTOCOL_FOOTER_SIZE] = {0x55};
    USBDFUProtocolAdapter adapter;
    usb_dfu_protocol_initialize(&adapter, 0x51);
    usb_dfu_download_prepare_destination(
        &adapter,
        destination,
        sizeof(destination)
    );
    reset_finish_transcript();

    assert(usb_dfu_download_data_stage_complete(
        &adapter,
        0,
        0,
        payload,
        sizeof(payload)
    ));
    assert(adapter.expected_block == 1);
    assert(adapter.last_block == 0);
    assert(usb_dfu_download_data_stage_complete(
        &adapter,
        0,
        0,
        footer,
        sizeof(footer)
    ));
    assert(adapter.expected_block == 1);
    assert(adapter.last_block == 0);
    assert(usb_dfu_manifestation_complete(&adapter, 0));
    assert(adapter.transport_length == sizeof(destination));
    assert(adapter.accepted_image_length == sizeof(payload));
    assert(finish_count == 3);
}

static void test_block_and_capacity_failures_are_observable(void) {
    uint8_t destination[32] = {0};
    uint8_t payload[17] = {0};
    USBDFUProtocolAdapter adapter;
    usb_dfu_protocol_initialize(&adapter, 7);
    usb_dfu_download_prepare_destination(
        &adapter,
        destination,
        sizeof(destination)
    );
    reset_finish_transcript();

    USBDFUProtocolEvent event = {
        .kind = USB_DFU_PROTOCOL_EVENT_DOWNLOAD,
        .block_number = 1,
        .payload = payload,
        .payload_length = sizeof(payload),
    };
    assert(dfu_protocol_event_handle(&adapter, &event) == -1);
    assert(adapter.state == DFU_ERROR);
    assert(adapter.status == DFU_STATUS_ERR_ADDRESS);
    assert(adapter.complete == 1);
    assert(dfu_download_wait(&adapter) == UINT32_MAX);
    assert(finish_count == 1);
    assert(finish_statuses[0] == DFU_STATUS_ERR_ADDRESS);

    usb_dfu_protocol_initialize(&adapter, 8);
    usb_dfu_download_prepare_destination(
        &adapter,
        destination,
        sizeof(destination)
    );
    reset_finish_transcript();
    event.block_number = 0;
    event.payload_length = USB_DFU_PROTOCOL_TRANSFER_SIZE + 1;
    assert(dfu_protocol_event_handle(&adapter, &event) == -1);
    assert(finish_statuses[0] == DFU_STATUS_ERR_ADDRESS);
}

static void test_manifest_and_abort_failures(void) {
    uint8_t destination[32] = {0};
    USBDFUProtocolAdapter adapter;
    usb_dfu_protocol_initialize(&adapter, 9);
    usb_dfu_download_prepare_destination(
        &adapter,
        destination,
        sizeof(destination)
    );
    reset_finish_transcript();

    const USBDFUProtocolEvent manifest = {
        .kind = USB_DFU_PROTOCOL_EVENT_MANIFEST,
    };
    assert(dfu_protocol_event_handle(&adapter, &manifest) == -1);
    assert(adapter.status == DFU_STATUS_ERR_VERIFY);
    assert(finish_statuses[0] == DFU_STATUS_ERR_VERIFY);

    usb_dfu_protocol_initialize(&adapter, 10);
    reset_finish_transcript();
    const USBDFUProtocolEvent abort_event = {
        .kind = USB_DFU_PROTOCOL_EVENT_ABORT,
    };
    assert(dfu_protocol_event_handle(&adapter, &abort_event) == -1);
    assert(adapter.status == DFU_STATUS_ERR_NOTDONE);
    assert(usb_dfu_download_event_handle(&adapter) == 10);
    assert(finish_statuses[0] == DFU_STATUS_ERR_NOTDONE);
}

static void test_lifecycle_and_recovered_constants(void) {
    uint8_t destination[16];
    USBDFUProtocolAdapter adapter;
    usb_dfu_protocol_initialize(&adapter, 0x33);
    assert(adapter.initialized == 1);
    assert(adapter.state == DFU_IDLE);
    assert(adapter.avp_status_code == 0x3200);
    assert(dfu_protocol_state_address_to_x8(&adapter) == &adapter);
    assert(dfu_protocol_source_token() == USB_DFU_PROTOCOL_SOURCE_TOKEN);
    assert(usb_dfu_download_buffer_tuple_validate(
        destination,
        sizeof(destination)
    ));
    assert(!usb_dfu_download_buffer_tuple_validate(0, sizeof(destination)));
    assert(!usb_dfu_download_buffer_tuple_validate(destination, 15));

    dfu_protocol_outlined_set_status_code_and_state2(
        &adapter,
        0x320f,
        DFU_STATUS_ERR_STALLEDPKT
    );
    assert(adapter.avp_status_code == 0x320f);
    assert(adapter.status == DFU_STATUS_ERR_STALLEDPKT);
    assert(adapter.state == DFU_IDLE);

    usb_dfu_protocol_deinitialize(&adapter);
    assert(adapter.initialized == 0);
    assert(adapter.destination == 0);
    assert(adapter.event_handle == 0);
}

int main(void) {
    test_successful_download_and_manifest();
    test_footer_can_extend_the_last_data_block();
    test_block_and_capacity_failures_are_observable();
    test_manifest_and_abort_failures();
    test_lifecycle_and_recovered_constants();
    puts("usb_dfu_protocol_adapter_tests: ok");
    return 0;
}
