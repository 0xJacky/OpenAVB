#include "avp/devices/usb/usb_device_dispatch_adapter.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>


static struct {
    unsigned calls;
    uintptr_t arguments[4];
    int result;
} callback_probe;

static struct {
    unsigned address_calls;
    uint8_t address_rhport;
    uint8_t address;
    unsigned stall_calls;
    unsigned clear_stall_calls;
    uint8_t endpoint_rhport;
    uint8_t endpoint_address;
} tinyusb_probe;

static struct {
    unsigned calls;
    USBDeviceDispatchFault fault;
} fault_probe;

void dcd_set_address(uint8_t rhport, uint8_t address) {
    ++tinyusb_probe.address_calls;
    tinyusb_probe.address_rhport = rhport;
    tinyusb_probe.address = address;
}

void usbd_edpt_stall(uint8_t rhport, uint8_t endpoint_address) {
    ++tinyusb_probe.stall_calls;
    tinyusb_probe.endpoint_rhport = rhport;
    tinyusb_probe.endpoint_address = endpoint_address;
}

void usbd_edpt_clear_stall(uint8_t rhport, uint8_t endpoint_address) {
    ++tinyusb_probe.clear_stall_calls;
    tinyusb_probe.endpoint_rhport = rhport;
    tinyusb_probe.endpoint_address = endpoint_address;
}

static int capture_callback(
    uintptr_t argument0,
    uintptr_t argument1,
    uintptr_t argument2,
    uintptr_t argument3
) {
    ++callback_probe.calls;
    callback_probe.arguments[0] = argument0;
    callback_probe.arguments[1] = argument1;
    callback_probe.arguments[2] = argument2;
    callback_probe.arguments[3] = argument3;
    return callback_probe.result;
}

static void capture_fault(
    const USBDeviceDispatchFault *fault,
    void *context
) {
    assert(context == &fault_probe);
    ++fault_probe.calls;
    fault_probe.fault = *fault;
}

static USBDeviceDispatchTableSpan table_span_for(
    USBDeviceDispatchTable *table
) {
    return (USBDeviceDispatchTableSpan){
        .current = table,
        .lower_bound = (uintptr_t)table,
        .upper_bound = (uintptr_t)table + sizeof(*table),
        .type = USB_DEVICE_DISPATCH_TABLE_TYPE,
    };
}

static void reset_callback_probe(int result) {
    memset(&callback_probe, 0, sizeof(callback_probe));
    callback_probe.result = result;
}

static void test_table_install_span_and_clear(void) {
    USBDeviceDispatchRouter router = {0};
    USBDeviceDispatchTable source = {0};
    source.slots[5] = (USBDeviceDispatchSlot){capture_callback, 0x505};
    assert(usb_device_dispatch_table_install_or_clear(
        &router,
        table_span_for(&source)
    ) == USB_DEVICE_DISPATCH_OK);
    assert(router.local.slots[5].callback == capture_callback);
    assert(router.local.slots[5].callback_type == 0x505);

    USBDeviceDispatchTableSpan live = usb_device_dispatch_table_span(&router);
    assert(live.current == &router.local);
    assert(live.lower_bound == (uintptr_t)&router.local);
    assert(live.upper_bound - live.lower_bound ==
        USB_DEVICE_DISPATCH_TABLE_SIZE);
    assert(live.type == USB_DEVICE_DISPATCH_TABLE_TYPE);

    USBDeviceDispatchTableSpan invalid = table_span_for(&source);
    invalid.upper_bound -= 1;
    assert(usb_device_dispatch_table_install_or_clear(&router, invalid) ==
        USB_DEVICE_DISPATCH_INVALID_TABLE);
    assert(router.local.slots[5].callback == capture_callback);

    assert(usb_device_dispatch_table_install_or_clear(
        &router,
        (USBDeviceDispatchTableSpan){0}
    ) == USB_DEVICE_DISPATCH_OK);
    assert(router.local.slots[5].callback == 0);
}

static void test_local_and_remote_routing(void) {
    USBDeviceDispatchRouter router = {0};
    router.local.slots[1].callback = capture_callback;
    router.local.slots[3].callback = capture_callback;
    router.remote.slots[1].callback = capture_callback;
    router.remote.slots[3].callback = capture_callback;

    reset_callback_probe(11);
    assert(usb_device_dispatch_control_transfer_buffer_release(&router) == 11);
    assert(callback_probe.calls == 1);
    reset_callback_probe(12);
    assert(usb_device_dispatch_shutdown(&router) == 12);
    assert(callback_probe.calls == 1);

    router.use_remote_backend = true;
    reset_callback_probe(21);
    assert(usb_device_dispatch_control_transfer_buffer_release(&router) == 21);
    assert(callback_probe.calls == 1);
    reset_callback_probe(22);
    assert(usb_device_dispatch_shutdown(&router) == 22);
    assert(callback_probe.calls == 1);
}

static void test_required_and_optional_slot_transcript(void) {
    USBDeviceDispatchRouter router = {0};
    const uint8_t slots[] = {5, 7, 9, 10, 11, 12, 14, 17};
    for (size_t index = 0; index < sizeof(slots); ++index) {
        router.local.slots[slots[index]].callback = capture_callback;
    }

    reset_callback_probe(5);
    assert(usb_device_dispatch_slot5(&router) == 5);
    assert(callback_probe.calls == 1);

    reset_callback_probe(7);
    assert(usb_device_dispatch_transfer_submit(
        &router,
        0x1000,
        0x1000,
        0x1070,
        5
    ) == 7);
    assert(callback_probe.calls == 1);
    assert(callback_probe.arguments[0] == 0x1000);
    assert(callback_probe.arguments[1] == 0x1000);
    assert(callback_probe.arguments[2] == 0x1070);
    assert(callback_probe.arguments[3] == 5);

    reset_callback_probe(9);
    assert(usb_device_dispatch_slot9(&router, 0x81) == 9);
    assert(callback_probe.arguments[0] == 0x81);
    reset_callback_probe(10);
    assert(usb_device_dispatch_slot10(&router, 0x55) == 10);
    assert(callback_probe.arguments[0] == 0x55);
    reset_callback_probe(11);
    assert(usb_device_dispatch_set_test_mode(&router, 4) == 11);
    assert(callback_probe.arguments[0] == 4);
    reset_callback_probe(12);
    assert(usb_device_dispatch_slot12(&router, 0x80) == 12);
    assert(callback_probe.arguments[0] == 0x80);
    reset_callback_probe(14);
    assert(usb_device_dispatch_slot14_optional(&router) == 14);
    reset_callback_probe(17);
    assert(usb_device_dispatch_transfer_completion_optional(
        &router,
        1,
        2,
        3,
        4
    ) == 17);
    assert(callback_probe.arguments[0] == 1);
    assert(callback_probe.arguments[3] == 4);

    router.local.slots[14].callback = 0;
    router.local.slots[17].callback = 0;
    assert(usb_device_dispatch_slot14_optional(&router) ==
        USB_DEVICE_DISPATCH_OK);
    assert(usb_device_dispatch_transfer_completion_optional(
        &router,
        1,
        2,
        3,
        4
    ) == USB_DEVICE_DISPATCH_OK);
}

typedef int (*MissingCallbackFunction)(USBDeviceDispatchRouter *router);

static void test_missing_callback_fault_contracts(void) {
    static const struct {
        MissingCallbackFunction function;
        uint8_t slot;
        uint32_t source_line;
    } cases[] = {
        {usb_device_dispatch_slot0_missing_callback_fatal, 0, 0x27},
        {usb_device_dispatch_slot1_missing_callback_fatal, 1, 0x2e},
        {usb_device_dispatch_slot2_missing_callback_fatal, 2, 0x35},
        {usb_device_dispatch_slot3_missing_callback_fatal, 3, 0x3c},
        {usb_device_dispatch_slot4_missing_callback_fatal, 4, 0x42},
        {usb_device_dispatch_slot5_missing_callback_fatal, 5, 0x48},
        {usb_device_dispatch_slot7_missing_callback_fatal, 7, 0x54},
        {usb_device_dispatch_slot8_missing_callback_fatal, 8, 0x5a},
        {usb_device_dispatch_slot9_missing_callback_fatal, 9, 0x60},
        {usb_device_dispatch_slot10_missing_callback_fatal, 10, 0x66},
        {usb_device_dispatch_slot11_missing_callback_fatal, 11, 0x6c},
        {usb_device_dispatch_slot12_missing_callback_fatal, 12, 0x72},
    };
    USBDeviceDispatchRouter router = {
        .fault_handler = capture_fault,
        .fault_context = &fault_probe,
    };
    memset(&fault_probe, 0, sizeof(fault_probe));
    for (size_t index = 0; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        assert(cases[index].function(&router) ==
            USB_DEVICE_DISPATCH_MISSING_CALLBACK);
        assert(router.last_fault.slot == cases[index].slot);
        assert(router.last_fault.source_line == cases[index].source_line);
        assert(router.last_fault.source_token ==
            UINT64_C(0x0a033545e4350115));
    }
    assert(router.fault_count == sizeof(cases) / sizeof(cases[0]));
    assert(fault_probe.calls == router.fault_count);
    assert(usb_device_dispatch_source_token() ==
        UINT64_C(0x0a033545e4350115));

    assert(usb_device_dispatch_slot5(&router) ==
        USB_DEVICE_DISPATCH_MISSING_CALLBACK);
    assert(router.last_fault.slot == 5);
}

static void test_tinyusb_slots(void) {
    USBDeviceDispatchRouter router = {0};
    usb_device_dispatch_install_tinyusb_slots(&router.local);
    memset(&tinyusb_probe, 0, sizeof(tinyusb_probe));

    assert(usb_device_dispatch_set_address(&router, 23) ==
        USB_DEVICE_DISPATCH_OK);
    assert(tinyusb_probe.address_calls == 1);
    assert(tinyusb_probe.address_rhport == 0);
    assert(tinyusb_probe.address == 23);

    assert(usb_device_dispatch_endpoint_set_halt(&router, 0x82, true) ==
        USB_DEVICE_DISPATCH_OK);
    assert(tinyusb_probe.stall_calls == 1);
    assert(tinyusb_probe.endpoint_rhport == 0);
    assert(tinyusb_probe.endpoint_address == 0x82);
    assert(usb_device_dispatch_endpoint_set_halt(&router, 0x82, false) ==
        USB_DEVICE_DISPATCH_OK);
    assert(tinyusb_probe.clear_stall_calls == 1);
}

int main(void) {
    test_table_install_span_and_clear();
    test_local_and_remote_routing();
    test_required_and_optional_slot_transcript();
    test_missing_callback_fault_contracts();
    test_tinyusb_slots();
    puts("usb_device_dispatch_adapter_tests: ok");
    return 0;
}
