#include "avp/devices/usb/usb_descriptor_adapter.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>


static USBDescriptorSpan byte_span(uint8_t *bytes, size_t size) {
    return (USBDescriptorSpan){
        .current = bytes,
        .lower_bound = bytes,
        .upper_bound = bytes + size,
        .type = USB_DESCRIPTOR_SPAN_TYPE_BYTE,
    };
}

static tusb_desc_configuration_t configuration_descriptor(void) {
    return (tusb_desc_configuration_t){
        .bLength = sizeof(tusb_desc_configuration_t),
        .bDescriptorType = TUSB_DESC_CONFIGURATION,
        .wTotalLength = 0x1b,
        .bNumInterfaces = 1,
        .bConfigurationValue = 1,
        .iConfiguration = 5,
        .bmAttributes = 0x80,
        .bMaxPower = 0xfa,
    };
}

static void test_byte_and_configuration_validation(void) {
    uint8_t bytes[16] = {0};
    USBDescriptorSpan bytes_view = byte_span(bytes, sizeof(bytes));
    assert(usb_descriptor_validate_byte_span(bytes_view));
    assert(usb_descriptor_validate_byte_span_thunk(bytes_view));
    bytes_view.type = USB_DESCRIPTOR_SPAN_TYPE_CONFIGURATION;
    assert(!usb_descriptor_validate_byte_span(bytes_view));
    bytes_view = byte_span(bytes, sizeof(bytes));
    bytes_view.current = bytes + sizeof(bytes);
    assert(!usb_descriptor_validate_byte_span_thunk(bytes_view));

    tusb_desc_configuration_t descriptor = configuration_descriptor();
    USBDescriptorSpan configuration = {
        .current = (uint8_t *)&descriptor,
        .lower_bound = (uint8_t *)&descriptor,
        .upper_bound = (uint8_t *)&descriptor + sizeof(descriptor),
        .type = USB_DESCRIPTOR_SPAN_TYPE_CONFIGURATION,
    };
    assert(usb_configuration_descriptor_validate_tuple(configuration));
    assert(usb_descriptor_validate_span_thunk(configuration));
    descriptor.bDescriptorType = TUSB_DESC_INTERFACE;
    assert(!usb_configuration_descriptor_validate_tuple(configuration));
    descriptor = configuration_descriptor();
    descriptor.wTotalLength = 8;
    assert(!usb_descriptor_validate_span_thunk(configuration));
}

static void test_record_and_span_outliners(void) {
    USBDescriptorRecord record = {
        .descriptor_count = 2,
        .total_length = 18,
    };
    USBDescriptorRecordTuple tuple = {
        .current = &record,
        .lower_bound = &record,
        .upper_bound = &record + 1,
        .type = USB_DESCRIPTOR_SPAN_TYPE_RECORD,
    };
    assert(usb_descriptor_validate_record_tuple(tuple));
    tuple.type = USB_DESCRIPTOR_SPAN_TYPE_BYTE;
    assert(!usb_descriptor_validate_record_tuple(tuple));

    uint8_t bytes[12] = {0};
    USBDescriptorSpan original = byte_span(bytes, sizeof(bytes));
    USBDescriptorSpan loaded =
        usb_descriptor_outlined_load_x20_span_bounds(&original);
    assert(memcmp(&loaded, &original, sizeof(original)) == 0);

    USBDescriptorSpan restored =
        usb_descriptor_outlined_restore_destination_bounds(
            bytes + 2,
            bytes,
            bytes + sizeof(bytes),
            USB_DESCRIPTOR_SPAN_TYPE_BYTE
        );
    assert(restored.current == bytes + 2);
    assert(restored.lower_bound == bytes);
    assert(restored.upper_bound == bytes + sizeof(bytes));
    assert(restored.type == USB_DESCRIPTOR_SPAN_TYPE_BYTE);

    restored = usb_descriptor_outlined_restore_destination_span_x27(
        bytes + 4,
        original
    );
    assert(restored.current == bytes + 4);
    assert(restored.lower_bound == bytes);
}

static void test_source_offset_and_copy_stage(void) {
    uint8_t bytes[16];
    for (size_t index = 0; index < sizeof(bytes); ++index) {
        bytes[index] = (uint8_t)index;
    }
    const USBDescriptorSpan embedded = byte_span(bytes, sizeof(bytes));
    USBDescriptorSpan source;
    assert(usb_descriptor_outlined_load_source_span_x8_offset_x19(
        embedded,
        5,
        &source
    ));
    assert(source.current == bytes + 5);
    assert(*source.current == 5);
    assert(!usb_descriptor_outlined_load_source_span_x8_offset_x19(
        embedded,
        sizeof(bytes),
        &source
    ));

    USBDescriptorCopyStage stage;
    assert(usb_descriptor_outlined_offset_source_current_and_stage_copy_length(
        embedded,
        4,
        7,
        &stage
    ));
    assert(stage.source.current == bytes + 4);
    assert(stage.copy_length == 7);
    assert(!usb_descriptor_outlined_offset_source_current_and_stage_copy_length(
        embedded,
        12,
        5,
        &stage
    ));
}

static void test_configuration_header_copy(void) {
    uint8_t output[16] = {0};
    const tusb_desc_configuration_t descriptor = configuration_descriptor();
    assert(usb_descriptor_outlined_copy_configuration_header_x21(
        byte_span(output, sizeof(output)),
        &descriptor
    ));
    assert(memcmp(output, &descriptor, sizeof(descriptor)) == 0);

    assert(!usb_descriptor_outlined_copy_configuration_header_x21(
        byte_span(output, sizeof(descriptor) - 1),
        &descriptor
    ));
}

int main(void) {
    test_byte_and_configuration_validation();
    test_record_and_span_outliners();
    test_source_offset_and_copy_stage();
    test_configuration_header_copy();
    puts("usb_descriptor_adapter_tests: ok");
    return 0;
}
