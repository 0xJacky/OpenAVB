#pragma once

#include "avp/devices/simplevirtio/simple_virtio_usb_async.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


#define SIMPLE_VIRTIO_USB_ADAPTER_PUBLIC __attribute__((used, retain))

enum {
    SIMPLE_VIRTIO_USB_BUS_BASE = 0x30000000,
    SIMPLE_VIRTIO_USB_SLOT = 16,
    SIMPLE_VIRTIO_USB_SLOT_STRIDE = 0x10000,
};

typedef enum {
    SIMPLE_VIRTIO_USB_TUPLE_CONTROLLER = 1,
    SIMPLE_VIRTIO_USB_TUPLE_DEVICE = 2,
    SIMPLE_VIRTIO_USB_TUPLE_QUEUE = 3,
    SIMPLE_VIRTIO_USB_TUPLE_BUFFER = 4,
    SIMPLE_VIRTIO_USB_TUPLE_COMPLETION = 5,
} SimpleVirtioUSBTupleType;

typedef struct {
    void *current;
    uintptr_t lower_bound;
    uintptr_t upper_bound;
    uintptr_t type;
} SimpleVirtioUSBTuple;

_Static_assert(sizeof(SimpleVirtioUSBTuple) == 0x20, "USB tuple size");

typedef struct {
    uint32_t declared_length;
    uint8_t endpoint;
    uint8_t type;
    uint16_t reserved;
    SimpleVirtioUSBTuple payload;
} SimpleVirtioUSBPayloadDispatchArguments;

SIMPLE_VIRTIO_USB_ADAPTER_PUBLIC uintptr_t simple_virtio_bus_base(void);
SIMPLE_VIRTIO_USB_ADAPTER_PUBLIC uint32_t simple_virtio_usb_slot(void);
SIMPLE_VIRTIO_USB_ADAPTER_PUBLIC uintptr_t simple_virtio_usb_mmio_base(void);

SIMPLE_VIRTIO_USB_ADAPTER_PUBLIC bool
simple_virtio_usb_endpoint_queue_initialize(
    SimpleVirtioUSBAsync *usb,
    uint32_t queue_index,
    uint8_t endpoint
);

SIMPLE_VIRTIO_USB_ADAPTER_PUBLIC bool
simple_virtio_usb_validate_controller_tuple(SimpleVirtioUSBTuple tuple);
SIMPLE_VIRTIO_USB_ADAPTER_PUBLIC bool
simple_virtio_usb_validate_device_tuple(SimpleVirtioUSBTuple tuple);
SIMPLE_VIRTIO_USB_ADAPTER_PUBLIC bool
simple_virtio_usb_validate_queue_tuple(SimpleVirtioUSBTuple tuple);
SIMPLE_VIRTIO_USB_ADAPTER_PUBLIC bool
simple_virtio_usb_validate_buffer_tuple(SimpleVirtioUSBTuple tuple);
SIMPLE_VIRTIO_USB_ADAPTER_PUBLIC bool
simple_virtio_usb_validate_completion_tuple(SimpleVirtioUSBTuple tuple);

SIMPLE_VIRTIO_USB_ADAPTER_PUBLIC SimpleVirtioUSBTuple
simple_virtio_usb_outlined_load_device_bounds_x19(SimpleVirtioUSBTuple tuple);
SIMPLE_VIRTIO_USB_ADAPTER_PUBLIC SimpleVirtioUSBTuple
simple_virtio_usb_outlined_load_device_bounds_x25(SimpleVirtioUSBTuple tuple);
SIMPLE_VIRTIO_USB_ADAPTER_PUBLIC SimpleVirtioUSBTuple
simple_virtio_usb_outlined_load_device_bounds_x21(SimpleVirtioUSBTuple tuple);

SIMPLE_VIRTIO_USB_ADAPTER_PUBLIC bool
usb_event_outlined_prepare_payload_dispatch_arguments(
    const uint8_t *event,
    uint32_t event_size,
    SimpleVirtioUSBPayloadDispatchArguments *arguments
);
