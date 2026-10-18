#include "avp/devices/usb/usb_stack_closure_owner.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>


static struct {
    bool xfer_result;
    bool status_result;
    unsigned xfer_calls;
    unsigned status_calls;
    tusb_control_request_t request;
    void *buffer;
    uint16_t length;
} control_probe;

static struct {
    unsigned calls;
    uint32_t command;
    uint32_t response;
    int result;
    RuntimeRPCPacketView request;
} rpc_probe;

static struct {
    bool rhport_init_result;
    bool connect_result;
    bool disconnect_result;
    bool deinit_result;
    unsigned rhport_init_calls;
    unsigned connect_calls;
    unsigned disconnect_calls;
    unsigned deinit_calls;
} tinyusb_lifecycle_probe;

_Noreturn void runtime_firebloom_panic(void) {
    assert(0);
    __builtin_unreachable();
}

void *runtime_memmove(void *destination, const void *source, size_t size) {
    return memmove(destination, source, size);
}

void runtime_bzero(void *destination, size_t size) {
    memset(destination, 0, size);
}

void dcd_edpt_stall(uint8_t rhport, uint8_t endpoint) {
    assert(rhport == 0);
    assert(endpoint == TU_EP0_OUT || endpoint == TU_EP0_IN);
}

bool tud_control_xfer(
    uint8_t rhport,
    const tusb_control_request_t *request,
    void *buffer,
    uint16_t length
) {
    assert(rhport == 0);
    ++control_probe.xfer_calls;
    control_probe.request = *request;
    control_probe.buffer = buffer;
    control_probe.length = length;
    return control_probe.xfer_result;
}

bool tud_control_status(
    uint8_t rhport,
    const tusb_control_request_t *request
) {
    assert(rhport == 0);
    ++control_probe.status_calls;
    control_probe.request = *request;
    return control_probe.status_result;
}

bool tusb_rhport_init(uint8_t rhport, const tusb_rhport_init_t *rh_init) {
    assert(rhport == 0);
    assert(rh_init->role == TUSB_ROLE_DEVICE);
    ++tinyusb_lifecycle_probe.rhport_init_calls;
    return tinyusb_lifecycle_probe.rhport_init_result;
}

bool tud_connect(void) {
    ++tinyusb_lifecycle_probe.connect_calls;
    return tinyusb_lifecycle_probe.connect_result;
}

bool tud_disconnect(void) {
    ++tinyusb_lifecycle_probe.disconnect_calls;
    return tinyusb_lifecycle_probe.disconnect_result;
}

bool tusb_deinit(uint8_t rhport) {
    assert(rhport == 0);
    ++tinyusb_lifecycle_probe.deinit_calls;
    return tinyusb_lifecycle_probe.deinit_result;
}

static int rpc_transport(
    uint32_t command,
    RuntimeRPCPacketView request,
    uint32_t *response,
    void *context
) {
    assert(context == &rpc_probe);
    ++rpc_probe.calls;
    rpc_probe.command = command;
    rpc_probe.request = request;
    *response = rpc_probe.response;
    return rpc_probe.result;
}

static const uint8_t descriptor[] = {
    0x12, TUSB_DESC_DEVICE, 0x00, 0x02, 0, 0, 0, 0x40,
    0xac, 0x05, 0x27, 0x12, 0, 0, 1, 2, 3, 1,
};

static const uint8_t *descriptor_lookup(
    void *context,
    uint8_t descriptor_type,
    uint8_t descriptor_index,
    uint16_t language_id,
    uint16_t *length
) {
    assert(context == &control_probe);
    assert(descriptor_type == TUSB_DESC_DEVICE);
    assert(descriptor_index == 0);
    assert(language_id == 0);
    *length = sizeof(descriptor);
    return descriptor;
}

static bool custom_dispatch(
    void *context,
    const tusb_control_request_t *request,
    uint8_t **buffer,
    uint16_t *length
) {
    assert(context == &control_probe);
    assert(request->bRequest == 0x5a);
    *buffer = (uint8_t *)(uintptr_t)descriptor;
    *length = 4;
    return true;
}

static tusb_control_request_t request(
    uint8_t type,
    uint8_t code,
    uint16_t value,
    uint16_t index,
    uint16_t length
) {
    return (tusb_control_request_t){
        .bmRequestType = type,
        .bRequest = code,
        .wValue = value,
        .wIndex = index,
        .wLength = length,
    };
}

static void reset_probes(void) {
    memset(&control_probe, 0, sizeof(control_probe));
    memset(&rpc_probe, 0, sizeof(rpc_probe));
    memset(&tinyusb_lifecycle_probe, 0, sizeof(tinyusb_lifecycle_probe));
    control_probe.xfer_result = true;
    control_probe.status_result = true;
    tinyusb_lifecycle_probe.rhport_init_result = true;
    tinyusb_lifecycle_probe.connect_result = true;
    tinyusb_lifecycle_probe.disconnect_result = true;
    tinyusb_lifecycle_probe.deinit_result = true;
}

static void test_frame_and_stack_guard_contracts(void) {
    USBStackClosureOwner owner;
    USBRPCFrame90 frame = {0};
    const USBRPCFrameRegisters registers = {1, 2, 3, 4, 5, 6};
    usb_stack_closure_owner_init(
        &owner,
        0,
        0,
        0,
        USB_STACK_LOCALITY_LOCAL,
        0x1122334455667788
    );
    usb_rpc_outlined_frame90_prologue(&frame, registers);
    const USBRPCFrameRegisters restored =
        usb_rpc_outlined_frame90_epilogue(&frame);
    assert(memcmp(&restored, &registers, sizeof(restored)) == 0);
    usb_rpc_outlined_stack_guard_save_fp_minus_18(&owner, &frame);
    frame.saved_guard_fp_minus_38 = owner.stack_guard;
    assert(usb_rpc_stack_guard_load_to_x8(&owner) == owner.stack_guard);
    assert(usb_rpc_outlined_stack_guard_compare_fp_minus_18(&owner, &frame));
    assert(usb_rpc_outlined_stack_guard_compare_fp_minus_38(&owner, &frame));
    assert(usb_rpc_outlined_stack_guard_compare_saved_x8(
        &owner,
        owner.stack_guard
    ));
    ++frame.saved_guard_fp_minus_18;
    assert(!usb_rpc_outlined_stack_guard_compare_fp_minus_18(&owner, &frame));
}

static void test_root_rpc_lifecycle_packet(void) {
    USBRPCCommandOwner rpc = {
        .transport = rpc_transport,
        .transport_context = &rpc_probe,
    };
    reset_probes();
    assert(root_rpc_usb_lifecycle_notify(&rpc, 1, 2) == 0);
    assert(rpc_probe.calls == 1);
    assert(rpc_probe.command == USB_STACK_RPC_LIFECYCLE_COMMAND);
    assert(rpc_probe.request.byte_size != 0);
    const RuntimeRPCFieldHeader *phase = runtime_rpc_field_find(
        rpc_probe.request,
        1
    );
    const RuntimeRPCFieldHeader *status = runtime_rpc_field_find(
        rpc_probe.request,
        2
    );
    assert(phase != 0);
    assert(status != 0);
    assert(*(const uint8_t *)runtime_rpc_field_payload_const(phase) == 1);
    assert(*(const uint8_t *)runtime_rpc_field_payload_const(status) == 2);

    rpc_probe.response = 1;
    assert(root_rpc_usb_lifecycle_notify(&rpc, 0, 0) ==
        USB_RPC_COMMAND_REMOTE_ERROR);
    rpc_probe.response = 0;
    rpc_probe.result = -1;
    assert(root_rpc_usb_lifecycle_notify(&rpc, 0, 0) ==
        USB_RPC_COMMAND_TRANSPORT_ERROR);
}

static void test_local_lifecycle(void) {
    const USBDFUStackRegistryEntry entries[] = {
        {USB_DFU_STACK_REGISTRY_COMMAND_0X1A, 0x1a},
        {USB_DFU_STACK_REGISTRY_COMMAND_0X1F, 0x1f},
    };
    const USBDFUStackRegistry registry = {entries, 2};
    USBDFUStackLifecycle stack = {0};
    USBStackClosureOwner owner;
    reset_probes();
    usb_stack_closure_owner_init(
        &owner,
        &stack,
        &registry,
        0,
        USB_STACK_LOCALITY_LOCAL,
        0x55
    );
    assert(j_usb_dfu_stack_acquire(&owner) == 0);
    assert(stack.reference_count == 1);
    assert(usb_dfu_stack_release(&stack));

    assert(usb_dfu_device_start(&owner) == 0);
    assert(owner.started == 1);
    assert(owner.start_count == 1);
    assert(stack.reference_count == 1);
    assert(usb_dfu_device_start(&owner) == 0);
    assert(owner.start_count == 1);
    assert(usb_dfu_device_shutdown(&owner) == 0);
    assert(owner.started == 0);
    assert(stack.reference_count == 0);
    assert(usb_dfu_device_deinitialize(&owner) == 0);
    assert(owner.deinitialize_count == 1);
}

static void test_remote_lifecycle(void) {
    USBRPCCommandOwner rpc = {
        .transport = rpc_transport,
        .transport_context = &rpc_probe,
    };
    USBStackClosureOwner owner;
    reset_probes();
    usb_stack_closure_owner_init(
        &owner,
        0,
        0,
        &rpc,
        USB_STACK_LOCALITY_REMOTE,
        0x66
    );
    assert(usb_dfu_device_start(&owner) == 0);
    assert(owner.started == 1);
    assert(rpc.call_count == 3);
    assert(rpc.last_command == USB_STACK_RPC_LIFECYCLE_COMMAND);
    assert(usb_dfu_device_shutdown(&owner) == 0);
    assert(rpc.call_count == 5);
    assert(rpc.last_command == USB_RPC_COMMAND_DEVICE_DEINITIALIZE);
}

static void test_control_dispatch(void) {
    USBEP0State ep0 = {0};
    USBControlRequestOwner owner = {
        .ep0 = &ep0,
        .descriptor_lookup = descriptor_lookup,
        .custom_dispatch = custom_dispatch,
        .callback_context = &control_probe,
    };
    reset_probes();
    tusb_control_request_t setup = request(
        0x80,
        6,
        (uint16_t)TUSB_DESC_DEVICE << 8,
        0,
        sizeof(descriptor)
    );
    assert(usb_dispatch_control_request(&owner, &setup) ==
        USB_CONTROL_DISPATCH_ACCEPTED);
    assert(control_probe.xfer_calls == 1);
    assert(control_probe.buffer == descriptor);
    assert(control_probe.length == sizeof(descriptor));

    setup = request(0x00, 9, 1, 0, 0);
    assert(usb_dispatch_control_request(&owner, &setup) ==
        USB_CONTROL_DISPATCH_ACCEPTED);
    assert(owner.configuration == 1);
    setup = request(0x80, 8, 0, 0, 1);
    assert(usb_dispatch_control_request(&owner, &setup) ==
        USB_CONTROL_DISPATCH_ACCEPTED);
    assert(*(uint8_t *)control_probe.buffer == 1);

    setup = request(0x01, 11, 3, 2, 0);
    assert(usb_dispatch_control_request(&owner, &setup) ==
        USB_CONTROL_DISPATCH_ACCEPTED);
    assert(owner.interface_alternate[2] == 3);
    setup = request(0x81, 10, 0, 2, 1);
    assert(usb_dispatch_control_request(&owner, &setup) ==
        USB_CONTROL_DISPATCH_ACCEPTED);
    assert(*(uint8_t *)control_probe.buffer == 3);

    setup = request(0xc0, 0x5a, 0, 0, 4);
    assert(usb_dispatch_control_request(&owner, &setup) ==
        USB_CONTROL_DISPATCH_ACCEPTED);
    assert(control_probe.length == 4);
    setup = request(0x80, 0xff, 0, 0, 0);
    assert(usb_dispatch_control_request(&owner, &setup) ==
        USB_CONTROL_DISPATCH_STALLED);
    assert(owner.accepted_count == 6);
    assert(owner.stalled_count == 1);
    assert(usb_ep0_outlined_load_qword_type_x3() ==
        USB_STACK_CONTROL_QWORD_TYPE);
    const USBControlSourceSpan source = {
        descriptor,
        (uintptr_t)descriptor,
        (uintptr_t)descriptor + sizeof(descriptor),
        1,
    };
    const USBControlSourceSpan copied =
        usb_ep0_outlined_copy_saved_source_span_x24_x21(source);
    assert(memcmp(&source, &copied, sizeof(source)) == 0);
}

int main(void) {
    test_frame_and_stack_guard_contracts();
    test_root_rpc_lifecycle_packet();
    test_local_lifecycle();
    test_remote_lifecycle();
    test_control_dispatch();
    puts("usb stack closure owner tests passed");
    return 0;
}
