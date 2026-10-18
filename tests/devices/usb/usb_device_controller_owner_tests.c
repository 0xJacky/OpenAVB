#include "avp/devices/usb/usb_device_controller_owner.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>


typedef struct {
    tusb_desc_endpoint_t opened[16];
    uint8_t closed[32];
    uint8_t stalled[16];
    uint8_t cleared[16];
    uint32_t open_count;
    uint32_t close_count;
    uint32_t close_all_count;
    uint32_t stall_count;
    uint32_t clear_count;
    uint32_t disconnect_count;
    uint32_t deinit_count;
    bool open_result;
    bool deinit_result;
} TinyUSBTrace;

static TinyUSBTrace tinyusb;

bool usbd_edpt_open(uint8_t rhport, const tusb_desc_endpoint_t *descriptor) {
    assert(rhport == 0);
    tinyusb.opened[tinyusb.open_count++] = *descriptor;
    return tinyusb.open_result;
}

void usbd_edpt_close(uint8_t rhport, uint8_t endpoint_address) {
    assert(rhport == 0);
    tinyusb.closed[tinyusb.close_count++] = endpoint_address;
}

void dcd_edpt_close_all(uint8_t rhport) {
    assert(rhport == 0);
    ++tinyusb.close_all_count;
}

void usbd_edpt_stall(uint8_t rhport, uint8_t endpoint_address) {
    assert(rhport == 0);
    tinyusb.stalled[tinyusb.stall_count++] = endpoint_address;
}

void usbd_edpt_clear_stall(uint8_t rhport, uint8_t endpoint_address) {
    assert(rhport == 0);
    tinyusb.cleared[tinyusb.clear_count++] = endpoint_address;
}

bool tud_disconnect(void) {
    ++tinyusb.disconnect_count;
    return true;
}

bool tusb_deinit(uint8_t rhport) {
    assert(rhport == 0);
    ++tinyusb.deinit_count;
    return tinyusb.deinit_result;
}

typedef struct {
    uint32_t event_count;
    uint32_t worker_count;
    uint32_t start_count;
    uint32_t finalize_count;
    uint8_t last_event_type;
    int start_status;
    int finalize_status;
} OwnerTrace;

static void event_callback(
    USBDeviceControllerOwner *owner,
    const USBDeviceControllerEvent *event,
    void *context
) {
    (void)owner;
    OwnerTrace *trace = context;
    ++trace->event_count;
    trace->last_event_type = event->type;
}

static void worker_callback(
    USBDeviceControllerOwner *owner,
    void *context
) {
    (void)owner;
    ++((OwnerTrace *)context)->worker_count;
}

static int function_start(
    USBDeviceControllerOwner *owner,
    void *context
) {
    (void)owner;
    OwnerTrace *trace = context;
    ++trace->start_count;
    return trace->start_status;
}

static int function_finalize(
    USBDeviceControllerOwner *owner,
    void *context
) {
    (void)owner;
    OwnerTrace *trace = context;
    ++trace->finalize_count;
    return trace->finalize_status;
}

static void reset_tinyusb(void) {
    memset(&tinyusb, 0, sizeof(tinyusb));
    tinyusb.open_result = true;
    tinyusb.deinit_result = true;
}

static void test_resource_and_callback_contract(void) {
    reset_tinyusb();
    USBDeviceControllerOwner owner = {0};
    OwnerTrace trace = {0};
    assert(usb_device_prepare_resources(&owner) == 0);
    assert(usb_control_transfer_buffer_capacity(&owner) == 0x1000);
    uint8_t *buffer = usb_device_controller_acquire_control_buffer(&owner);
    assert(buffer == owner.control_buffer);
    assert(usb_device_controller_acquire_control_buffer(&owner) == 0);
    owner.control_length = 32;
    usb_control_transfer_buffer_release(&owner);
    assert(owner.control_length == 0);
    assert(!owner.control_buffer_leased);

    assert(usb_device_install_callback_pair(
        &owner,
        (USBDeviceControllerCallbackPair){
            .event = event_callback,
            .worker = worker_callback,
            .context = &trace,
        }
    ));
    const uint8_t payload[] = {1, 2, 3};
    assert(usb_device_transport_event_callback(
        &owner,
        (USBDeviceControllerEvent){
            .type = 7,
            .endpoint_address = 0x81,
            .payload = payload,
            .payload_length = sizeof(payload),
        }
    ) == 0);
    assert(trace.event_count == 1 && trace.worker_count == 1);
    assert(trace.last_event_type == 7);
    assert(owner.worker_signal_count == 1);
}

static void test_endpoint_contract(void) {
    reset_tinyusb();
    USBDeviceControllerOwner owner = {0};
    assert(usb_device_prepare_resources(&owner) == 0);
    assert(usb_endpoint_initialize(
        &owner, 0x84, TUSB_XFER_BULK, 512, 0) == 0);
    assert(tinyusb.open_count == 1);
    assert(tinyusb.opened[0].bLength == sizeof(tusb_desc_endpoint_t));
    assert(tinyusb.opened[0].bDescriptorType == TUSB_DESC_ENDPOINT);
    assert(tinyusb.opened[0].bEndpointAddress == 0x84);
    assert(tinyusb.opened[0].bmAttributes.xfer == TUSB_XFER_BULK);
    assert(tinyusb.opened[0].wMaxPacketSize == 512);
    assert(!usb_endpoint_is_halted(&owner, 0x84));
    assert(usb_endpoint_set_halt(&owner, 0x84, true) == 0);
    assert(usb_endpoint_is_halted(&owner, 0x84));
    assert(tinyusb.stall_count == 1 && tinyusb.stalled[0] == 0x84);
    assert(usb_endpoint_set_halt(&owner, 0x84, false) == 0);
    assert(!usb_endpoint_is_halted(&owner, 0x84));
    assert(tinyusb.clear_count == 1 && tinyusb.cleared[0] == 0x84);
    assert(usb_endpoint_deinitialize(&owner, 0x84) == 0);
    assert(tinyusb.close_count == 1 && tinyusb.closed[0] == 0x84);
    assert(usb_endpoint_set_halt(&owner, 0x84, true) ==
        USB_DEVICE_CONTROLLER_INVALID_ARGUMENT);
}

static void test_lifecycle_contract(void) {
    reset_tinyusb();
    USBDeviceControllerOwner owner = {0};
    OwnerTrace trace = {0};
    assert(usb_device_prepare_resources(&owner) == 0);
    const USBDeviceControllerFunction function = {
        .start = function_start,
        .finalize = function_finalize,
        .context = &trace,
    };
    assert(usb_device_controller_register_function(&owner, function));
    assert(usb_device_controller_register_function(&owner, function));
    assert(!usb_device_controller_register_function(&owner, function));
    assert(usb_device_lifecycle_transition(&owner) == 0);
    assert(owner.lifecycle == USB_DEVICE_CONTROLLER_ACTIVE);
    assert(trace.start_count == 2);
    assert(tinyusb.open_count == 2);
    assert(tinyusb.opened[0].bEndpointAddress == 0);
    assert(tinyusb.opened[1].bEndpointAddress == 0x80);
    assert(usb_device_finalize_callbacks(&owner) == 0);
    assert(owner.callbacks_finalized);
    assert(trace.finalize_count == 2);
    assert(usb_device_shutdown(&owner) == 0);
    assert(owner.lifecycle == USB_DEVICE_CONTROLLER_STOPPED);
    assert(tinyusb.close_all_count == 1);
    assert(tinyusb.disconnect_count == 1);
    assert(usb_device_deinitialize(&owner) == 0);
    assert(!owner.resources_prepared);
    assert(owner.function_count == 0);
    assert(tinyusb.deinit_count == 1);

    usb_device_method_slot4_noop();
    usb_device_method_slot9_noop();
    usb_device_method_slot11_noop();
    assert(usb_device_method_slot5_return_two() == 2);
}

static void test_observable_failures(void) {
    reset_tinyusb();
    USBDeviceControllerOwner owner = {0};
    OwnerTrace trace = {.start_status = 1};
    assert(usb_device_prepare_resources(&owner) == 0);
    assert(usb_device_controller_register_function(
        &owner,
        (USBDeviceControllerFunction){
            .start = function_start,
            .finalize = function_finalize,
            .context = &trace,
        }
    ));
    assert(usb_device_lifecycle_transition(&owner) ==
        USB_DEVICE_CONTROLLER_CALLBACK_ERROR);
    assert(owner.lifecycle == USB_DEVICE_CONTROLLER_STOPPED);

    trace.start_status = 0;
    tinyusb.open_result = false;
    assert(usb_device_lifecycle_transition(&owner) ==
        USB_DEVICE_CONTROLLER_RESOURCE_ERROR);
    assert(owner.lifecycle == USB_DEVICE_CONTROLLER_STOPPED);
}

int main(void) {
    test_resource_and_callback_contract();
    test_endpoint_contract();
    test_lifecycle_contract();
    test_observable_failures();
    puts("usb_device_controller_owner_tests: ok");
    return 0;
}
