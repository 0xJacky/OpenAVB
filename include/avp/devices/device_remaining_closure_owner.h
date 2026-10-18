#pragma once

#include "avp/protocol/sep/sep_protocol_helper_adapter.h"
#include "avp/devices/simplevirtio/simplevirtio_queue_adapter.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


#define DEVICE_REMAINING_CLOSURE_PUBLIC __attribute__((used, retain))

enum {
    SIMPLE_VIRTIO_USB_DEVICE_IDENTIFIER = 0x1a01,
    SIMPLE_VIRTIO_DEVICE_VIEW_SIZE = 0x60,
    SIMPLE_VIRTIO_DEVICE_TYPE_IDENTITY = 0x70028f40,
    SEP_DESCRIPTOR_TABLE_BASE = 0x00146080,
};

typedef struct {
    uint8_t bytes[SIMPLE_VIRTIO_DEVICE_VIEW_SIZE];
} SimpleVirtioClosureDevice;

typedef struct {
    SimpleVirtioClosureDevice *current;
    uintptr_t lower_bound;
    uintptr_t upper_bound;
    uintptr_t type;
} SimpleVirtioClosureDeviceView;

typedef struct {
    uintptr_t address;
    bool carry;
} SEPDescriptorAddressResult;

typedef struct {
    SimpleVirtioQueueDevice usb_device;
    SimpleVirtioAdapterIO usb_io;
    uintptr_t usb_base;
    uint32_t configuration_offset;
    uint64_t stack_guard;
    uint32_t local_initialize_count;
    uint8_t usb_initialized;
    uint8_t configuration_enabled;
} DeviceRemainingClosureOwner;

_Static_assert(sizeof(SimpleVirtioClosureDevice) ==
    SIMPLE_VIRTIO_DEVICE_VIEW_SIZE, "SimpleVirtio closure device size");
_Static_assert(sizeof(SimpleVirtioClosureDeviceView) == 0x20,
    "SimpleVirtio closure view size");

DEVICE_REMAINING_CLOSURE_PUBLIC void device_remaining_closure_owner_init(
    DeviceRemainingClosureOwner *owner,
    const SimpleVirtioAdapterIO *usb_io,
    uintptr_t usb_base,
    uint32_t configuration_offset,
    uint64_t stack_guard
);

DEVICE_REMAINING_CLOSURE_PUBLIC _Noreturn void
simple_virtio_usb_endpoint_slot_fatal(void);

DEVICE_REMAINING_CLOSURE_PUBLIC SimpleVirtioAdapterStatus
simple_virtio_usb_local_initialize(DeviceRemainingClosureOwner *owner);

DEVICE_REMAINING_CLOSURE_PUBLIC bool
sep_runtime_transport_stack_guard_compare_x8(
    const DeviceRemainingClosureOwner *owner,
    uint64_t saved_guard
);

DEVICE_REMAINING_CLOSURE_PUBLIC bool sep_runtime_stack_guard_compare(
    const DeviceRemainingClosureOwner *owner,
    uint64_t saved_guard
);

DEVICE_REMAINING_CLOSURE_PUBLIC uint64_t sep_runtime_stack_guard_load(
    const DeviceRemainingClosureOwner *owner
);

DEVICE_REMAINING_CLOSURE_PUBLIC int
j_sep_bootstrap_configuration_exchange(
    SEPProtocolHelperSession *session,
    SEPBootstrapResult *result
);

DEVICE_REMAINING_CLOSURE_PUBLIC SEPDescriptorAddressResult
sep_descriptor_address_add_checked(uintptr_t descriptor_offset);

DEVICE_REMAINING_CLOSURE_PUBLIC SimpleVirtioClosureDeviceView
simplevirtio_device_bounded_view(
    SimpleVirtioClosureDevice *device
);

DEVICE_REMAINING_CLOSURE_PUBLIC bool
simple_virtio_storage_stack_guard_compare_saved_x8(
    const DeviceRemainingClosureOwner *owner,
    uint64_t saved_guard
);
