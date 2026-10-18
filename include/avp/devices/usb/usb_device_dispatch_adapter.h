#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


#define USB_DEVICE_DISPATCH_PUBLIC __attribute__((used, retain))

enum {
    USB_DEVICE_DISPATCH_SLOT_COUNT = 18,
    USB_DEVICE_DISPATCH_TABLE_SIZE = 0x120,
    USB_DEVICE_DISPATCH_TABLE_TYPE = 0x146d60,
    USB_DEVICE_DISPATCH_OK = 0,
    USB_DEVICE_DISPATCH_INVALID_TABLE = -1,
    USB_DEVICE_DISPATCH_MISSING_CALLBACK = -2,
};

typedef int (*USBDeviceDispatchCallback)(
    uintptr_t argument0,
    uintptr_t argument1,
    uintptr_t argument2,
    uintptr_t argument3
);

typedef struct {
    USBDeviceDispatchCallback callback;
    uintptr_t callback_type;
} USBDeviceDispatchSlot;

typedef struct {
    USBDeviceDispatchSlot slots[USB_DEVICE_DISPATCH_SLOT_COUNT];
} USBDeviceDispatchTable;

typedef struct {
    USBDeviceDispatchTable *current;
    uintptr_t lower_bound;
    uintptr_t upper_bound;
    uintptr_t type;
} USBDeviceDispatchTableSpan;

typedef struct {
    uint64_t source_token;
    uint32_t source_line;
    uint8_t slot;
} USBDeviceDispatchFault;

typedef void (*USBDeviceDispatchFaultHandler)(
    const USBDeviceDispatchFault *fault,
    void *context
);

typedef struct {
    USBDeviceDispatchTable local;
    USBDeviceDispatchTable remote;
    bool use_remote_backend;
    USBDeviceDispatchFault last_fault;
    size_t fault_count;
    USBDeviceDispatchFaultHandler fault_handler;
    void *fault_context;
} USBDeviceDispatchRouter;

_Static_assert(sizeof(USBDeviceDispatchSlot) == 0x10,
    "USB dispatch slot size");
_Static_assert(sizeof(USBDeviceDispatchTable) == USB_DEVICE_DISPATCH_TABLE_SIZE,
    "USB dispatch table size");
_Static_assert(sizeof(USBDeviceDispatchTableSpan) == 0x20,
    "USB dispatch table span size");

USB_DEVICE_DISPATCH_PUBLIC int
usb_device_dispatch_control_transfer_buffer_release(
    USBDeviceDispatchRouter *router
);

USB_DEVICE_DISPATCH_PUBLIC int usb_device_dispatch_shutdown(
    USBDeviceDispatchRouter *router
);

USB_DEVICE_DISPATCH_PUBLIC int usb_device_dispatch_table_install_or_clear(
    USBDeviceDispatchRouter *router,
    USBDeviceDispatchTableSpan source
);

USB_DEVICE_DISPATCH_PUBLIC int
usb_device_dispatch_slot0_missing_callback_fatal(
    USBDeviceDispatchRouter *router
);
USB_DEVICE_DISPATCH_PUBLIC int
usb_device_dispatch_slot1_missing_callback_fatal(
    USBDeviceDispatchRouter *router
);
USB_DEVICE_DISPATCH_PUBLIC int
usb_device_dispatch_slot2_missing_callback_fatal(
    USBDeviceDispatchRouter *router
);
USB_DEVICE_DISPATCH_PUBLIC int
usb_device_dispatch_slot3_missing_callback_fatal(
    USBDeviceDispatchRouter *router
);

USB_DEVICE_DISPATCH_PUBLIC int usb_device_dispatch_set_address(
    USBDeviceDispatchRouter *router,
    uint8_t address
);

USB_DEVICE_DISPATCH_PUBLIC int
usb_device_dispatch_slot4_missing_callback_fatal(
    USBDeviceDispatchRouter *router
);

USB_DEVICE_DISPATCH_PUBLIC int usb_device_dispatch_slot5(
    USBDeviceDispatchRouter *router
);

USB_DEVICE_DISPATCH_PUBLIC int
usb_device_dispatch_slot5_missing_callback_fatal(
    USBDeviceDispatchRouter *router
);

USB_DEVICE_DISPATCH_PUBLIC int usb_device_dispatch_transfer_submit(
    USBDeviceDispatchRouter *router,
    uintptr_t request_current,
    uintptr_t request_lower,
    uintptr_t request_upper,
    uintptr_t request_type
);

USB_DEVICE_DISPATCH_PUBLIC int
usb_device_dispatch_slot7_missing_callback_fatal(
    USBDeviceDispatchRouter *router
);

USB_DEVICE_DISPATCH_PUBLIC int usb_device_dispatch_endpoint_set_halt(
    USBDeviceDispatchRouter *router,
    uint8_t endpoint_address,
    bool halted
);

USB_DEVICE_DISPATCH_PUBLIC int
usb_device_dispatch_slot8_missing_callback_fatal(
    USBDeviceDispatchRouter *router
);

USB_DEVICE_DISPATCH_PUBLIC int usb_device_dispatch_slot9(
    USBDeviceDispatchRouter *router,
    uintptr_t argument
);

USB_DEVICE_DISPATCH_PUBLIC int
usb_device_dispatch_slot9_missing_callback_fatal(
    USBDeviceDispatchRouter *router
);

USB_DEVICE_DISPATCH_PUBLIC int usb_device_dispatch_slot10(
    USBDeviceDispatchRouter *router,
    uintptr_t argument
);

USB_DEVICE_DISPATCH_PUBLIC int
usb_device_dispatch_slot10_missing_callback_fatal(
    USBDeviceDispatchRouter *router
);

USB_DEVICE_DISPATCH_PUBLIC int usb_device_dispatch_set_test_mode(
    USBDeviceDispatchRouter *router,
    uint8_t selector
);

USB_DEVICE_DISPATCH_PUBLIC int
usb_device_dispatch_slot11_missing_callback_fatal(
    USBDeviceDispatchRouter *router
);

USB_DEVICE_DISPATCH_PUBLIC int usb_device_dispatch_slot12(
    USBDeviceDispatchRouter *router,
    uintptr_t argument
);

USB_DEVICE_DISPATCH_PUBLIC int
usb_device_dispatch_slot12_missing_callback_fatal(
    USBDeviceDispatchRouter *router
);

USB_DEVICE_DISPATCH_PUBLIC int usb_device_dispatch_slot14_optional(
    USBDeviceDispatchRouter *router
);

USB_DEVICE_DISPATCH_PUBLIC int
usb_device_dispatch_transfer_completion_optional(
    USBDeviceDispatchRouter *router,
    uintptr_t request_current,
    uintptr_t request_lower,
    uintptr_t request_upper,
    uintptr_t request_type
);

USB_DEVICE_DISPATCH_PUBLIC uint64_t usb_device_dispatch_source_token(void);

USB_DEVICE_DISPATCH_PUBLIC USBDeviceDispatchTableSpan
usb_device_dispatch_table_span(USBDeviceDispatchRouter *router);

USB_DEVICE_DISPATCH_PUBLIC void usb_device_dispatch_install_tinyusb_slots(
    USBDeviceDispatchTable *table
);
