#include "avp/devices/usb/usb_device_controller_owner.h"

#include "device/dcd.h"
#include "device/usbd_pvt.h"
#include "tusb.h"


static void clear_bytes(void *object, size_t size) {
    uint8_t *bytes = object;
    for (size_t index = 0; index < size; ++index) {
        bytes[index] = 0;
    }
}

static int endpoint_index(uint8_t endpoint_address) {
    const uint8_t number = tu_edpt_number(endpoint_address);
    if (number >= USB_DEVICE_CONTROLLER_ENDPOINT_COUNT / 2) {
        return -1;
    }
    return (int)(number * 2 + (tu_edpt_dir(endpoint_address) == TUSB_DIR_IN));
}

static USBDeviceControllerEndpoint *endpoint_record(
    USBDeviceControllerOwner *owner,
    uint8_t endpoint_address
) {
    const int index = endpoint_index(endpoint_address);
    return owner == 0 || index < 0 ? 0 : &owner->endpoints[index];
}

static const USBDeviceControllerEndpoint *endpoint_record_const(
    const USBDeviceControllerOwner *owner,
    uint8_t endpoint_address
) {
    const int index = endpoint_index(endpoint_address);
    return owner == 0 || index < 0 ? 0 : &owner->endpoints[index];
}

bool usb_device_install_callback_pair(
    USBDeviceControllerOwner *owner,
    USBDeviceControllerCallbackPair callbacks
) {
    if (owner == 0) {
        return false;
    }
    owner->callbacks = callbacks;
    return true;
}

bool usb_device_controller_register_function(
    USBDeviceControllerOwner *owner,
    USBDeviceControllerFunction function
) {
    if (owner == 0 || owner->function_count >=
        USB_DEVICE_CONTROLLER_FUNCTION_COUNT) {
        return false;
    }
    owner->functions[owner->function_count++] = function;
    return true;
}

int usb_device_prepare_resources(USBDeviceControllerOwner *owner) {
    if (owner == 0) {
        return USB_DEVICE_CONTROLLER_INVALID_ARGUMENT;
    }
    clear_bytes(owner->endpoints, sizeof(owner->endpoints));
    clear_bytes(owner->control_buffer, sizeof(owner->control_buffer));
    owner->control_length = 0;
    owner->control_buffer_leased = false;
    owner->resources_prepared = true;
    owner->callbacks_finalized = false;
    owner->worker_event_created = true;
    owner->worker_signal_count = 0;
    owner->lifecycle = USB_DEVICE_CONTROLLER_STOPPED;
    return USB_DEVICE_CONTROLLER_OK;
}

uint32_t usb_control_transfer_buffer_capacity(
    const USBDeviceControllerOwner *owner
) {
    return owner != 0 && owner->resources_prepared ?
        USB_DEVICE_CONTROLLER_CONTROL_CAPACITY : 0;
}

uint8_t *usb_device_controller_acquire_control_buffer(
    USBDeviceControllerOwner *owner
) {
    if (usb_control_transfer_buffer_capacity(owner) == 0 ||
        owner->control_buffer_leased) {
        return 0;
    }
    owner->control_buffer_leased = true;
    return owner->control_buffer;
}

void usb_control_transfer_buffer_release(USBDeviceControllerOwner *owner) {
    if (owner != 0) {
        owner->control_length = 0;
        owner->control_buffer_leased = false;
    }
}

int usb_endpoint_initialize(
    USBDeviceControllerOwner *owner,
    uint8_t endpoint_address,
    tusb_xfer_type_t transfer_type,
    uint16_t max_packet_size,
    uint8_t interval
) {
    USBDeviceControllerEndpoint *const endpoint =
        endpoint_record(owner, endpoint_address);
    if (endpoint == 0 || max_packet_size == 0 ||
        transfer_type > TUSB_XFER_INTERRUPT) {
        return USB_DEVICE_CONTROLLER_INVALID_ARGUMENT;
    }
    endpoint->descriptor = (tusb_desc_endpoint_t){
        .bLength = sizeof(tusb_desc_endpoint_t),
        .bDescriptorType = TUSB_DESC_ENDPOINT,
        .bEndpointAddress = endpoint_address,
        .bmAttributes.xfer = transfer_type,
        .wMaxPacketSize = max_packet_size,
        .bInterval = interval,
    };
    if (!usbd_edpt_open(0, &endpoint->descriptor)) {
        return USB_DEVICE_CONTROLLER_RESOURCE_ERROR;
    }
    endpoint->initialized = true;
    endpoint->halted = false;
    return USB_DEVICE_CONTROLLER_OK;
}

int usb_device_transport_event_callback(
    USBDeviceControllerOwner *owner,
    USBDeviceControllerEvent event
) {
    if (owner == 0 ||
        (event.payload_length != 0 && event.payload == 0)) {
        return USB_DEVICE_CONTROLLER_INVALID_ARGUMENT;
    }
    owner->last_event = event;
    if (owner->callbacks.event != 0) {
        owner->callbacks.event(owner, &owner->last_event, owner->callbacks.context);
    }
    usb_device_signal_worker(owner);
    return USB_DEVICE_CONTROLLER_OK;
}

int usb_endpoint_deinitialize(
    USBDeviceControllerOwner *owner,
    uint8_t endpoint_address
) {
    USBDeviceControllerEndpoint *const endpoint =
        endpoint_record(owner, endpoint_address);
    if (endpoint == 0) {
        return USB_DEVICE_CONTROLLER_INVALID_ARGUMENT;
    }
    if (endpoint->initialized) {
        usbd_edpt_close(0, endpoint_address);
        clear_bytes(endpoint, sizeof(*endpoint));
    }
    return USB_DEVICE_CONTROLLER_OK;
}

int usb_device_shutdown(USBDeviceControllerOwner *owner) {
    if (owner == 0) {
        return USB_DEVICE_CONTROLLER_INVALID_ARGUMENT;
    }
    owner->lifecycle = USB_DEVICE_CONTROLLER_STOPPING;
    for (uint8_t number = 0;
         number < USB_DEVICE_CONTROLLER_ENDPOINT_COUNT / 2; ++number) {
        (void)usb_endpoint_deinitialize(owner, number);
        (void)usb_endpoint_deinitialize(owner, (uint8_t)(number | 0x80));
    }
    dcd_edpt_close_all(0);
    (void)tud_disconnect();
    owner->lifecycle = USB_DEVICE_CONTROLLER_STOPPED;
    return USB_DEVICE_CONTROLLER_OK;
}

void usb_device_signal_worker(USBDeviceControllerOwner *owner) {
    if (owner == 0 || !owner->worker_event_created) {
        return;
    }
    ++owner->worker_signal_count;
    if (owner->callbacks.worker != 0) {
        owner->callbacks.worker(owner, owner->callbacks.context);
    }
}

void usb_device_method_slot4_noop(void) {
}

int usb_device_method_slot5_return_two(void) {
    return 2;
}

int usb_endpoint_set_halt(
    USBDeviceControllerOwner *owner,
    uint8_t endpoint_address,
    bool halted
) {
    USBDeviceControllerEndpoint *const endpoint =
        endpoint_record(owner, endpoint_address);
    if (endpoint == 0 || !endpoint->initialized) {
        return USB_DEVICE_CONTROLLER_INVALID_ARGUMENT;
    }
    if (halted) {
        usbd_edpt_stall(0, endpoint_address);
    } else {
        usbd_edpt_clear_stall(0, endpoint_address);
    }
    endpoint->halted = halted;
    return USB_DEVICE_CONTROLLER_OK;
}

void usb_device_method_slot9_noop(void) {
}

bool usb_endpoint_is_halted(
    const USBDeviceControllerOwner *owner,
    uint8_t endpoint_address
) {
    const USBDeviceControllerEndpoint *const endpoint =
        endpoint_record_const(owner, endpoint_address);
    return endpoint != 0 && endpoint->initialized && endpoint->halted;
}

void usb_device_method_slot11_noop(void) {
}

int usb_device_lifecycle_transition(USBDeviceControllerOwner *owner) {
    if (owner == 0 || !owner->resources_prepared) {
        return USB_DEVICE_CONTROLLER_INVALID_ARGUMENT;
    }
    owner->lifecycle = USB_DEVICE_CONTROLLER_STARTING;
    for (uint8_t index = 0; index < owner->function_count; ++index) {
        USBDeviceControllerFunction *const function = &owner->functions[index];
        if (function->start != 0 &&
            function->start(owner, function->context) != 0) {
            owner->lifecycle = USB_DEVICE_CONTROLLER_STOPPED;
            return USB_DEVICE_CONTROLLER_CALLBACK_ERROR;
        }
    }
    if (usb_endpoint_initialize(
            owner, 0, TUSB_XFER_CONTROL, CFG_TUD_ENDPOINT0_SIZE, 0) != 0 ||
        usb_endpoint_initialize(
            owner, 0x80, TUSB_XFER_CONTROL, CFG_TUD_ENDPOINT0_SIZE, 0) != 0) {
        (void)usb_device_shutdown(owner);
        return USB_DEVICE_CONTROLLER_RESOURCE_ERROR;
    }
    owner->lifecycle = USB_DEVICE_CONTROLLER_ACTIVE;
    return USB_DEVICE_CONTROLLER_OK;
}

int usb_device_finalize_callbacks(USBDeviceControllerOwner *owner) {
    if (owner == 0) {
        return USB_DEVICE_CONTROLLER_INVALID_ARGUMENT;
    }
    for (uint8_t index = 0; index < owner->function_count; ++index) {
        USBDeviceControllerFunction *const function = &owner->functions[index];
        if (function->finalize != 0 &&
            function->finalize(owner, function->context) != 0) {
            return USB_DEVICE_CONTROLLER_CALLBACK_ERROR;
        }
    }
    owner->callbacks_finalized = true;
    return USB_DEVICE_CONTROLLER_OK;
}

int usb_device_deinitialize(USBDeviceControllerOwner *owner) {
    if (owner == 0) {
        return USB_DEVICE_CONTROLLER_INVALID_ARGUMENT;
    }
    (void)usb_device_shutdown(owner);
    usb_control_transfer_buffer_release(owner);
    clear_bytes(owner->functions, sizeof(owner->functions));
    owner->function_count = 0;
    owner->callbacks = (USBDeviceControllerCallbackPair){0};
    owner->resources_prepared = false;
    owner->worker_event_created = false;
    owner->callbacks_finalized = false;
    return tusb_deinit(0) ? USB_DEVICE_CONTROLLER_OK :
        USB_DEVICE_CONTROLLER_RESOURCE_ERROR;
}
