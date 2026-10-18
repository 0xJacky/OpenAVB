#include "avp/devices/usb/usb_descriptor_adapter.h"


static bool span_contains(USBDescriptorSpan span, size_t size) {
    const uintptr_t current = (uintptr_t)span.current;
    const uintptr_t lower = (uintptr_t)span.lower_bound;
    const uintptr_t upper = (uintptr_t)span.upper_bound;
    return lower <= current && current < upper && size <= upper - current;
}

bool usb_descriptor_validate_byte_span_thunk(USBDescriptorSpan span) {
    return usb_descriptor_validate_byte_span(span);
}

bool usb_descriptor_validate_span_thunk(USBDescriptorSpan span) {
    return usb_configuration_descriptor_validate_tuple(span);
}

bool usb_descriptor_validate_record_tuple(USBDescriptorRecordTuple tuple) {
    if (tuple.type != USB_DESCRIPTOR_SPAN_TYPE_RECORD || tuple.current == 0) {
        return false;
    }
    const uintptr_t current = (uintptr_t)tuple.current;
    const uintptr_t lower = (uintptr_t)tuple.lower_bound;
    const uintptr_t upper = (uintptr_t)tuple.upper_bound;
    return lower <= current && current < upper &&
        sizeof(*tuple.current) <= upper - current;
}

USBDescriptorSpan usb_descriptor_outlined_load_x20_span_bounds(
    const USBDescriptorSpan *span
) {
    return *span;
}

USBDescriptorSpan usb_descriptor_outlined_restore_destination_bounds(
    uint8_t *current,
    uint8_t *lower_bound,
    uint8_t *upper_bound,
    uintptr_t type
) {
    return (USBDescriptorSpan){
        .current = current,
        .lower_bound = lower_bound,
        .upper_bound = upper_bound,
        .type = type,
    };
}

bool usb_descriptor_validate_byte_span(USBDescriptorSpan span) {
    return span.type == USB_DESCRIPTOR_SPAN_TYPE_BYTE &&
        span_contains(span, 1);
}

bool usb_configuration_descriptor_validate_tuple(USBDescriptorSpan span) {
    if (span.type != USB_DESCRIPTOR_SPAN_TYPE_CONFIGURATION ||
        !span_contains(span, sizeof(tusb_desc_configuration_t))) {
        return false;
    }
    const tusb_desc_configuration_t *descriptor =
        (const tusb_desc_configuration_t *)span.current;
    return descriptor->bLength == sizeof(*descriptor) &&
        descriptor->bDescriptorType == TUSB_DESC_CONFIGURATION &&
        descriptor->wTotalLength >= sizeof(*descriptor);
}

bool usb_descriptor_outlined_load_source_span_x8_offset_x19(
    USBDescriptorSpan embedded,
    size_t offset,
    USBDescriptorSpan *source
) {
    if (source == 0 || !span_contains(embedded, offset + 1)) {
        return false;
    }
    *source = embedded;
    source->current += offset;
    return true;
}

bool usb_descriptor_outlined_offset_source_current_and_stage_copy_length(
    USBDescriptorSpan source,
    size_t offset,
    size_t copy_length,
    USBDescriptorCopyStage *stage
) {
    if (stage == 0 || offset > SIZE_MAX - copy_length ||
        !span_contains(source, offset + copy_length)) {
        return false;
    }
    source.current += offset;
    *stage = (USBDescriptorCopyStage){
        .source = source,
        .copy_length = copy_length,
    };
    return true;
}

USBDescriptorSpan usb_descriptor_outlined_restore_destination_span_x27(
    uint8_t *current,
    USBDescriptorSpan saved
) {
    saved.current = current;
    return saved;
}

bool usb_descriptor_outlined_copy_configuration_header_x21(
    USBDescriptorSpan destination,
    const tusb_desc_configuration_t *header
) {
    if (header == 0 || destination.type != USB_DESCRIPTOR_SPAN_TYPE_BYTE ||
        !span_contains(destination, sizeof(*header)) ||
        header->bLength != sizeof(*header) ||
        header->bDescriptorType != TUSB_DESC_CONFIGURATION ||
        header->wTotalLength < sizeof(*header)) {
        return false;
    }
    const uint8_t *source = (const uint8_t *)header;
    for (size_t index = 0; index < sizeof(*header); ++index) {
        destination.current[index] = source[index];
    }
    return true;
}
