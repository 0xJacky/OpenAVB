#include "avp/devices/usb/usb_dfu_descriptor_owner.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>


typedef struct {
    uint32_t local_calls;
    uint32_t remote_calls;
    uint16_t observed_product_id;
} BackendTrace;

static USBDFUDescriptorTextSpan text_span(const char *text) {
    const size_t size = strlen(text) + 1;
    return (USBDFUDescriptorTextSpan){
        .current = text,
        .lower_bound = text,
        .upper_bound = text + size,
        .type = USB_DFU_DESCRIPTOR_TEXT_TYPE,
    };
}

static USBDFUDescriptorArguments arguments(uint8_t security_domain) {
    const USBDFUDescriptorTextSpan strings[] = {
        text_span("Apple Inc."),
        text_span("Apple Mobile Device (DFU Mode)"),
        text_span("CPID:8010"),
        text_span("DFU Configuration"),
        text_span("DFU Interface"),
    };
    return usb_dfu_outlined_prepare_descriptor_arguments(
        strings,
        security_domain
    );
}

static int local_initialize(
    const USBDFUDescriptorSet *descriptors,
    void *context
) {
    BackendTrace *trace = context;
    ++trace->local_calls;
    trace->observed_product_id = descriptors->device.idProduct;
    return 41;
}

static int remote_initialize(
    const USBDFUDescriptorSet *descriptors,
    void *context
) {
    BackendTrace *trace = context;
    ++trace->remote_calls;
    trace->observed_product_id = descriptors->device.idProduct;
    return 73;
}

static void test_string_descriptor_contract(void) {
    uint8_t descriptor[USB_DFU_DESCRIPTOR_STRING_MAX_BYTES] = {0};
    uint16_t length = 0;
    assert(usb_build_string_descriptor(
        text_span("DFU"), descriptor, sizeof(descriptor), &length));
    const uint8_t expected[] = {8, TUSB_DESC_STRING, 'D', 0, 'F', 0, 'U', 0};
    assert(length == sizeof(expected));
    assert(memcmp(descriptor, expected, sizeof(expected)) == 0);

    char maximum[USB_DFU_DESCRIPTOR_STRING_MAX_CHARS + 1];
    memset(maximum, 'x', sizeof(maximum) - 1);
    maximum[sizeof(maximum) - 1] = '\0';
    assert(usb_build_string_descriptor(
        text_span(maximum), descriptor, sizeof(descriptor), &length));
    assert(length == USB_DFU_DESCRIPTOR_STRING_MAX_BYTES);

    char oversized[USB_DFU_DESCRIPTOR_STRING_MAX_CHARS + 2];
    memset(oversized, 'y', sizeof(oversized) - 1);
    oversized[sizeof(oversized) - 1] = '\0';
    assert(!usb_build_string_descriptor(
        text_span(oversized), descriptor, sizeof(descriptor), &length));
    assert(!usb_build_string_descriptor(
        text_span("DFU"), descriptor, 7, &length));
}

static void test_descriptor_build_contract(void) {
    USBDFUDescriptorSet set;
    const USBDFUDescriptorArguments source = arguments(1);
    assert(usb_build_dfu_descriptors(&set, &source));
    assert(set.device.bLength == 18);
    assert(set.device.bDescriptorType == TUSB_DESC_DEVICE);
    assert(set.device.idVendor == 0x05ac);
    assert(set.device.idProduct == 0x1226 + 1);
    assert(set.device.iManufacturer == 2);
    assert(set.device.iProduct == 3);
    assert(set.device.iSerialNumber == 4);
    assert(set.string_indices[3] == 5);
    assert(set.string_indices[4] == 6);

    assert(set.configuration_length == USB_DFU_DESCRIPTOR_CONFIGURATION_BYTES);
    const tusb_desc_configuration_t *configuration =
        (const tusb_desc_configuration_t *)set.configuration;
    const tusb_desc_interface_t *interface =
        (const tusb_desc_interface_t *)(set.configuration + 9);
    const tusb_desc_dfu_functional_t *functional =
        (const tusb_desc_dfu_functional_t *)(set.configuration + 18);
    assert(configuration->bLength == 9);
    assert(configuration->bDescriptorType == TUSB_DESC_CONFIGURATION);
    assert(configuration->wTotalLength == 27);
    assert(configuration->bNumInterfaces == 1);
    assert(configuration->iConfiguration == 5);
    assert(configuration->bmAttributes == 0x80);
    assert(configuration->bMaxPower == 0xfa);
    assert(interface->bDescriptorType == TUSB_DESC_INTERFACE);
    assert(interface->bInterfaceClass == 0xfe);
    assert(interface->bInterfaceSubClass == 1);
    assert(interface->bInterfaceProtocol == 2);
    assert(interface->iInterface == 6);
    assert(functional->bDescriptorType == TUSB_DESC_FUNCTIONAL);
    assert(functional->bAttributes == 1);
    assert(functional->wDetachTimeOut == 10);
    assert(functional->wTransferSize == 0x800);
    assert(functional->bcdDFUVersion == 0x0110);
}

static void test_string_slot_exhaustion(void) {
    USBDFUDescriptorSet set = {0};
    for (uint8_t expected = USB_DFU_DESCRIPTOR_STRING_FIRST;
         expected < USB_DFU_DESCRIPTOR_STRING_CAPACITY; ++expected) {
        uint8_t index = 0;
        assert(usb_insert_string_descriptor(&set, text_span("x"), &index));
        assert(index == expected);
    }
    uint8_t index = 0;
    assert(!usb_insert_string_descriptor(&set, text_span("full"), &index));
}

static void test_function_registration_and_outliners(void) {
    USBDFUDescriptorFunctionState state = {0};
    USBDFUDescriptorFault fault = {0};
    usb_dfu_descriptor_owner_bind_state(&state, &fault, (uintptr_t)0xa55a);
    assert(usb_outlined_load_device_state_x19() == &state);

    const USBDFUDescriptorImplementationSpan first = {
        .current = 0x1100,
        .lower_bound = 0x1000,
        .upper_bound = 0x1200,
        .type = USB_DFU_DESCRIPTOR_IMPLEMENTATION_TYPE,
    };
    const USBDFUDescriptorImplementationSpan second = {
        .current = 0x2200,
        .lower_bound = 0x2000,
        .upper_bound = 0x2400,
        .type = USB_DFU_DESCRIPTOR_IMPLEMENTATION_TYPE,
    };
    assert(usb_function_register_implementation_span(first));
    assert(usb_function_register_implementation_span(second));
    assert(state.implementation_count == 2);
    assert(!usb_function_register_implementation_span(first));
    assert(fault.fault_count == 1);
    assert(fault.source_token == UINT64_C(0x0a033545e4350115));

    USBDFUDescriptorImplementationBounds bounds =
        usb_function_outlined_load_implementation_bounds(0);
    assert(bounds.valid);
    assert(bounds.current_at_or_above_lower_bound);
    assert(bounds.span.current == first.current);
    assert(!usb_function_outlined_load_implementation_bounds(2).valid);
    assert(usb_descriptor_outlined_stack_guard_compare_saved_x8(
        (uintptr_t)0xa55a));
    assert(!usb_descriptor_outlined_stack_guard_compare_saved_x8(
        (uintptr_t)0xa55b));
    assert(usb_descriptor_build_return_success_thunk() == 0);
}

static void test_device_initialization_routing(void) {
    USBDFUDescriptorSet set;
    const USBDFUDescriptorArguments source = arguments(2);
    BackendTrace trace = {0};
    USBDFUDescriptorBackend backend = {
        .local_initialize = local_initialize,
        .remote_initialize = remote_initialize,
        .context = &trace,
    };
    assert(usb_dfu_device_initialize(&set, &source, &backend) == 41);
    assert(trace.local_calls == 1);
    assert(trace.remote_calls == 0);
    assert(trace.observed_product_id == 0x1226 + 2);

    backend.use_remote_backend = true;
    assert(usb_dfu_device_initialize(&set, &source, &backend) == 73);
    assert(trace.local_calls == 1);
    assert(trace.remote_calls == 1);
    backend.remote_initialize = 0;
    assert(usb_dfu_device_initialize(&set, &source, &backend) ==
        USB_DFU_DESCRIPTOR_BACKEND_FAULT);
}

int main(void) {
    test_string_descriptor_contract();
    test_descriptor_build_contract();
    test_string_slot_exhaustion();
    test_function_registration_and_outliners();
    test_device_initialization_routing();
    puts("usb_dfu_descriptor_owner_tests: ok");
    return 0;
}
