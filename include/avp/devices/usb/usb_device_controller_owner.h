#pragma once

#include "common/tusb_types.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


#define USB_DEVICE_CONTROLLER_OWNER_PUBLIC __attribute__((used, retain))

enum {
    USB_DEVICE_CONTROLLER_ENDPOINT_COUNT = 12,
    USB_DEVICE_CONTROLLER_FUNCTION_COUNT = 2,
    USB_DEVICE_CONTROLLER_CONTROL_CAPACITY = 0x1000,
    USB_DEVICE_CONTROLLER_OK = 0,
    USB_DEVICE_CONTROLLER_INVALID_ARGUMENT = -1,
    USB_DEVICE_CONTROLLER_RESOURCE_ERROR = -2,
    USB_DEVICE_CONTROLLER_CALLBACK_ERROR = -3,
};

typedef enum {
    USB_DEVICE_CONTROLLER_STOPPED = 0,
    USB_DEVICE_CONTROLLER_STARTING = 1,
    USB_DEVICE_CONTROLLER_ACTIVE = 2,
    USB_DEVICE_CONTROLLER_STOPPING = 3,
} USBDeviceControllerLifecycle;

typedef struct {
    uint8_t type;
    uint8_t endpoint_address;
    const uint8_t *payload;
    uint32_t payload_length;
} USBDeviceControllerEvent;

struct USBDeviceControllerOwner;

typedef void (*USBDeviceControllerEventCallback)(
    struct USBDeviceControllerOwner *owner,
    const USBDeviceControllerEvent *event,
    void *context
);

typedef void (*USBDeviceControllerWorkerCallback)(
    struct USBDeviceControllerOwner *owner,
    void *context
);

typedef int (*USBDeviceControllerFunctionCallback)(
    struct USBDeviceControllerOwner *owner,
    void *context
);

typedef struct {
    USBDeviceControllerEventCallback event;
    USBDeviceControllerWorkerCallback worker;
    void *context;
} USBDeviceControllerCallbackPair;

typedef struct {
    USBDeviceControllerFunctionCallback start;
    USBDeviceControllerFunctionCallback finalize;
    void *context;
} USBDeviceControllerFunction;

typedef struct {
    tusb_desc_endpoint_t descriptor;
    bool initialized;
    bool halted;
} USBDeviceControllerEndpoint;

typedef struct USBDeviceControllerOwner {
    USBDeviceControllerLifecycle lifecycle;
    USBDeviceControllerCallbackPair callbacks;
    USBDeviceControllerFunction functions[USB_DEVICE_CONTROLLER_FUNCTION_COUNT];
    uint8_t function_count;
    USBDeviceControllerEndpoint endpoints[USB_DEVICE_CONTROLLER_ENDPOINT_COUNT];
    uint8_t control_buffer[USB_DEVICE_CONTROLLER_CONTROL_CAPACITY];
    uint32_t control_length;
    bool resources_prepared;
    bool control_buffer_leased;
    bool callbacks_finalized;
    bool worker_event_created;
    uint32_t worker_signal_count;
    USBDeviceControllerEvent last_event;
} USBDeviceControllerOwner;

USB_DEVICE_CONTROLLER_OWNER_PUBLIC bool usb_device_install_callback_pair(
    USBDeviceControllerOwner *owner,
    USBDeviceControllerCallbackPair callbacks
);
USB_DEVICE_CONTROLLER_OWNER_PUBLIC int usb_device_prepare_resources(
    USBDeviceControllerOwner *owner
);
USB_DEVICE_CONTROLLER_OWNER_PUBLIC uint32_t
usb_control_transfer_buffer_capacity(const USBDeviceControllerOwner *owner);
USB_DEVICE_CONTROLLER_OWNER_PUBLIC void usb_control_transfer_buffer_release(
    USBDeviceControllerOwner *owner
);
USB_DEVICE_CONTROLLER_OWNER_PUBLIC int usb_endpoint_initialize(
    USBDeviceControllerOwner *owner,
    uint8_t endpoint_address,
    tusb_xfer_type_t transfer_type,
    uint16_t max_packet_size,
    uint8_t interval
);
USB_DEVICE_CONTROLLER_OWNER_PUBLIC int usb_device_transport_event_callback(
    USBDeviceControllerOwner *owner,
    USBDeviceControllerEvent event
);
USB_DEVICE_CONTROLLER_OWNER_PUBLIC int usb_device_shutdown(
    USBDeviceControllerOwner *owner
);
USB_DEVICE_CONTROLLER_OWNER_PUBLIC int usb_endpoint_deinitialize(
    USBDeviceControllerOwner *owner,
    uint8_t endpoint_address
);
USB_DEVICE_CONTROLLER_OWNER_PUBLIC void usb_device_signal_worker(
    USBDeviceControllerOwner *owner
);
USB_DEVICE_CONTROLLER_OWNER_PUBLIC void usb_device_method_slot4_noop(void);
USB_DEVICE_CONTROLLER_OWNER_PUBLIC int usb_device_method_slot5_return_two(void);
USB_DEVICE_CONTROLLER_OWNER_PUBLIC int usb_endpoint_set_halt(
    USBDeviceControllerOwner *owner,
    uint8_t endpoint_address,
    bool halted
);
USB_DEVICE_CONTROLLER_OWNER_PUBLIC void usb_device_method_slot9_noop(void);
USB_DEVICE_CONTROLLER_OWNER_PUBLIC bool usb_endpoint_is_halted(
    const USBDeviceControllerOwner *owner,
    uint8_t endpoint_address
);
USB_DEVICE_CONTROLLER_OWNER_PUBLIC void usb_device_method_slot11_noop(void);
USB_DEVICE_CONTROLLER_OWNER_PUBLIC int usb_device_lifecycle_transition(
    USBDeviceControllerOwner *owner
);
USB_DEVICE_CONTROLLER_OWNER_PUBLIC int usb_device_finalize_callbacks(
    USBDeviceControllerOwner *owner
);
USB_DEVICE_CONTROLLER_OWNER_PUBLIC int usb_device_deinitialize(
    USBDeviceControllerOwner *owner
);

bool usb_device_controller_register_function(
    USBDeviceControllerOwner *owner,
    USBDeviceControllerFunction function
);
uint8_t *usb_device_controller_acquire_control_buffer(
    USBDeviceControllerOwner *owner
);
