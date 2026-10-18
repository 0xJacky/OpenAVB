#include "avp/devices/usb/usb_device_dispatch_adapter.h"

#include "device/dcd.h"
#include "device/usbd_pvt.h"


enum {
    USB_DISPATCH_SLOT_INITIALIZE = 0,
    USB_DISPATCH_SLOT_RELEASE_CONTROL_BUFFER = 1,
    USB_DISPATCH_SLOT_SHUTDOWN = 3,
    USB_DISPATCH_SLOT_SET_ADDRESS = 4,
    USB_DISPATCH_SLOT_STATUS = 5,
    USB_DISPATCH_SLOT_TRANSFER_SUBMIT = 7,
    USB_DISPATCH_SLOT_ENDPOINT_HALT = 8,
    USB_DISPATCH_SLOT_ENDPOINT_COMMAND = 9,
    USB_DISPATCH_SLOT_CONTROL_COMMAND = 10,
    USB_DISPATCH_SLOT_TEST_MODE = 11,
    USB_DISPATCH_SLOT_ENDPOINT_RESET = 12,
    USB_DISPATCH_SLOT_RESET_OPTIONAL = 14,
    USB_DISPATCH_SLOT_TRANSFER_COMPLETE = 17,
};

static void clear_bytes(void *object, size_t size) {
    uint8_t *bytes = object;
    for (size_t index = 0; index < size; ++index) {
        bytes[index] = 0;
    }
}

static void copy_bytes(void *destination, const void *source, size_t size) {
    uint8_t *output = destination;
    const uint8_t *input = source;
    for (size_t index = 0; index < size; ++index) {
        output[index] = input[index];
    }
}

uint64_t usb_device_dispatch_source_token(void) {
    return UINT64_C(0x0a033545e4350115);
}

USBDeviceDispatchTableSpan usb_device_dispatch_table_span(
    USBDeviceDispatchRouter *router
) {
    return (USBDeviceDispatchTableSpan){
        .current = &router->local,
        .lower_bound = (uintptr_t)&router->local,
        .upper_bound = (uintptr_t)&router->local + sizeof(router->local),
        .type = USB_DEVICE_DISPATCH_TABLE_TYPE,
    };
}

static bool table_span_valid(USBDeviceDispatchTableSpan source) {
    const uintptr_t current = (uintptr_t)source.current;
    return source.type == USB_DEVICE_DISPATCH_TABLE_TYPE &&
        source.current != 0 && source.lower_bound <= current &&
        current < source.upper_bound &&
        sizeof(*source.current) <= source.upper_bound - current;
}

int usb_device_dispatch_table_install_or_clear(
    USBDeviceDispatchRouter *router,
    USBDeviceDispatchTableSpan source
) {
    if (source.current == 0 && source.lower_bound == 0 &&
        source.upper_bound == 0 && source.type == 0) {
        clear_bytes(&router->local, sizeof(router->local));
        return USB_DEVICE_DISPATCH_OK;
    }
    if (!table_span_valid(source)) {
        return USB_DEVICE_DISPATCH_INVALID_TABLE;
    }
    copy_bytes(&router->local, source.current, sizeof(router->local));
    return USB_DEVICE_DISPATCH_OK;
}

static int record_missing_callback(
    USBDeviceDispatchRouter *router,
    uint8_t slot,
    uint32_t source_line
) {
    router->last_fault = (USBDeviceDispatchFault){
        .source_token = usb_device_dispatch_source_token(),
        .source_line = source_line,
        .slot = slot,
    };
    ++router->fault_count;
    if (router->fault_handler != 0) {
        router->fault_handler(&router->last_fault, router->fault_context);
    }
    return USB_DEVICE_DISPATCH_MISSING_CALLBACK;
}

int usb_device_dispatch_slot0_missing_callback_fatal(
    USBDeviceDispatchRouter *router
) {
    return record_missing_callback(router, 0, 0x27);
}

int usb_device_dispatch_slot1_missing_callback_fatal(
    USBDeviceDispatchRouter *router
) {
    return record_missing_callback(router, 1, 0x2e);
}

int usb_device_dispatch_slot2_missing_callback_fatal(
    USBDeviceDispatchRouter *router
) {
    return record_missing_callback(router, 2, 0x35);
}

int usb_device_dispatch_slot3_missing_callback_fatal(
    USBDeviceDispatchRouter *router
) {
    return record_missing_callback(router, 3, 0x3c);
}

int usb_device_dispatch_slot4_missing_callback_fatal(
    USBDeviceDispatchRouter *router
) {
    return record_missing_callback(router, 4, 0x42);
}

int usb_device_dispatch_slot5_missing_callback_fatal(
    USBDeviceDispatchRouter *router
) {
    return record_missing_callback(router, 5, 0x48);
}

int usb_device_dispatch_slot7_missing_callback_fatal(
    USBDeviceDispatchRouter *router
) {
    return record_missing_callback(router, 7, 0x54);
}

int usb_device_dispatch_slot8_missing_callback_fatal(
    USBDeviceDispatchRouter *router
) {
    return record_missing_callback(router, 8, 0x5a);
}

int usb_device_dispatch_slot9_missing_callback_fatal(
    USBDeviceDispatchRouter *router
) {
    return record_missing_callback(router, 9, 0x60);
}

int usb_device_dispatch_slot10_missing_callback_fatal(
    USBDeviceDispatchRouter *router
) {
    return record_missing_callback(router, 10, 0x66);
}

int usb_device_dispatch_slot11_missing_callback_fatal(
    USBDeviceDispatchRouter *router
) {
    return record_missing_callback(router, 11, 0x6c);
}

int usb_device_dispatch_slot12_missing_callback_fatal(
    USBDeviceDispatchRouter *router
) {
    return record_missing_callback(router, 12, 0x72);
}

static USBDeviceDispatchTable *active_table(USBDeviceDispatchRouter *router) {
    return router->use_remote_backend ? &router->remote : &router->local;
}

int usb_device_dispatch_control_transfer_buffer_release(
    USBDeviceDispatchRouter *router
) {
    USBDeviceDispatchCallback callback =
        active_table(router)->slots[USB_DISPATCH_SLOT_RELEASE_CONTROL_BUFFER].callback;
    if (callback == 0) {
        return usb_device_dispatch_slot1_missing_callback_fatal(router);
    }
    return callback(0, 0, 0, 0);
}

int usb_device_dispatch_shutdown(USBDeviceDispatchRouter *router) {
    USBDeviceDispatchCallback callback =
        active_table(router)->slots[USB_DISPATCH_SLOT_SHUTDOWN].callback;
    if (callback == 0) {
        return usb_device_dispatch_slot3_missing_callback_fatal(router);
    }
    return callback(0, 0, 0, 0);
}

int usb_device_dispatch_set_address(
    USBDeviceDispatchRouter *router,
    uint8_t address
) {
    USBDeviceDispatchCallback callback =
        router->local.slots[USB_DISPATCH_SLOT_SET_ADDRESS].callback;
    if (callback == 0) {
        return usb_device_dispatch_slot4_missing_callback_fatal(router);
    }
    return callback(address, 0, 0, 0);
}

int usb_device_dispatch_slot5(USBDeviceDispatchRouter *router) {
    USBDeviceDispatchCallback callback =
        router->local.slots[USB_DISPATCH_SLOT_STATUS].callback;
    if (callback == 0) {
        return usb_device_dispatch_slot5_missing_callback_fatal(router);
    }
    return callback(0, 0, 0, 0);
}

int usb_device_dispatch_transfer_submit(
    USBDeviceDispatchRouter *router,
    uintptr_t request_current,
    uintptr_t request_lower,
    uintptr_t request_upper,
    uintptr_t request_type
) {
    USBDeviceDispatchCallback callback =
        router->local.slots[USB_DISPATCH_SLOT_TRANSFER_SUBMIT].callback;
    if (callback == 0) {
        return usb_device_dispatch_slot7_missing_callback_fatal(router);
    }
    return callback(
        request_current,
        request_lower,
        request_upper,
        request_type
    );
}

int usb_device_dispatch_endpoint_set_halt(
    USBDeviceDispatchRouter *router,
    uint8_t endpoint_address,
    bool halted
) {
    USBDeviceDispatchCallback callback =
        router->local.slots[USB_DISPATCH_SLOT_ENDPOINT_HALT].callback;
    if (callback == 0) {
        return usb_device_dispatch_slot8_missing_callback_fatal(router);
    }
    return callback(endpoint_address, halted, 0, 0);
}

int usb_device_dispatch_slot9(
    USBDeviceDispatchRouter *router,
    uintptr_t argument
) {
    USBDeviceDispatchCallback callback =
        router->local.slots[USB_DISPATCH_SLOT_ENDPOINT_COMMAND].callback;
    if (callback == 0) {
        return usb_device_dispatch_slot9_missing_callback_fatal(router);
    }
    return callback(argument, 0, 0, 0);
}

int usb_device_dispatch_slot10(
    USBDeviceDispatchRouter *router,
    uintptr_t argument
) {
    USBDeviceDispatchCallback callback =
        router->local.slots[USB_DISPATCH_SLOT_CONTROL_COMMAND].callback;
    if (callback == 0) {
        return usb_device_dispatch_slot10_missing_callback_fatal(router);
    }
    return callback(argument, 0, 0, 0);
}

int usb_device_dispatch_set_test_mode(
    USBDeviceDispatchRouter *router,
    uint8_t selector
) {
    USBDeviceDispatchCallback callback =
        router->local.slots[USB_DISPATCH_SLOT_TEST_MODE].callback;
    if (callback == 0) {
        return usb_device_dispatch_slot11_missing_callback_fatal(router);
    }
    return callback(selector, 0, 0, 0);
}

int usb_device_dispatch_slot12(
    USBDeviceDispatchRouter *router,
    uintptr_t argument
) {
    USBDeviceDispatchCallback callback =
        router->local.slots[USB_DISPATCH_SLOT_ENDPOINT_RESET].callback;
    if (callback == 0) {
        return usb_device_dispatch_slot12_missing_callback_fatal(router);
    }
    return callback(argument, 0, 0, 0);
}

int usb_device_dispatch_slot14_optional(USBDeviceDispatchRouter *router) {
    USBDeviceDispatchCallback callback =
        router->local.slots[USB_DISPATCH_SLOT_RESET_OPTIONAL].callback;
    return callback == 0 ? USB_DEVICE_DISPATCH_OK :
        callback(0, 0, 0, 0);
}

int usb_device_dispatch_transfer_completion_optional(
    USBDeviceDispatchRouter *router,
    uintptr_t request_current,
    uintptr_t request_lower,
    uintptr_t request_upper,
    uintptr_t request_type
) {
    USBDeviceDispatchCallback callback =
        router->local.slots[USB_DISPATCH_SLOT_TRANSFER_COMPLETE].callback;
    return callback == 0 ? USB_DEVICE_DISPATCH_OK : callback(
        request_current,
        request_lower,
        request_upper,
        request_type
    );
}

static int tinyusb_set_address(
    uintptr_t address,
    uintptr_t unused1,
    uintptr_t unused2,
    uintptr_t unused3
) {
    (void)unused1;
    (void)unused2;
    (void)unused3;
    dcd_set_address(0, (uint8_t)address);
    return USB_DEVICE_DISPATCH_OK;
}

static int tinyusb_set_endpoint_halt(
    uintptr_t endpoint_address,
    uintptr_t halted,
    uintptr_t unused2,
    uintptr_t unused3
) {
    (void)unused2;
    (void)unused3;
    if (halted != 0) {
        usbd_edpt_stall(0, (uint8_t)endpoint_address);
    } else {
        usbd_edpt_clear_stall(0, (uint8_t)endpoint_address);
    }
    return USB_DEVICE_DISPATCH_OK;
}

void usb_device_dispatch_install_tinyusb_slots(
    USBDeviceDispatchTable *table
) {
    table->slots[USB_DISPATCH_SLOT_SET_ADDRESS] = (USBDeviceDispatchSlot){
        .callback = tinyusb_set_address,
        .callback_type = USB_DEVICE_DISPATCH_TABLE_TYPE +
            USB_DISPATCH_SLOT_SET_ADDRESS,
    };
    table->slots[USB_DISPATCH_SLOT_ENDPOINT_HALT] =
        (USBDeviceDispatchSlot){
            .callback = tinyusb_set_endpoint_halt,
            .callback_type = USB_DEVICE_DISPATCH_TABLE_TYPE +
                USB_DISPATCH_SLOT_ENDPOINT_HALT,
        };
}
