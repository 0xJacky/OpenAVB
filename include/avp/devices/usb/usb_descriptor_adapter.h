#pragma once

#include "common/tusb_types.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


#define USB_DESCRIPTOR_ADAPTER_PUBLIC __attribute__((used, retain))

typedef enum {
    USB_DESCRIPTOR_SPAN_TYPE_BYTE = 1,
    USB_DESCRIPTOR_SPAN_TYPE_CONFIGURATION = 2,
    USB_DESCRIPTOR_SPAN_TYPE_RECORD = 3,
} USBDescriptorSpanType;

typedef struct {
    uint8_t *current;
    uint8_t *lower_bound;
    uint8_t *upper_bound;
    uintptr_t type;
} USBDescriptorSpan;

_Static_assert(sizeof(USBDescriptorSpan) == 0x20, "USB descriptor span size");

typedef struct {
    USBDescriptorSpan bytes;
    uint32_t descriptor_count;
    uint32_t total_length;
} USBDescriptorRecord;

typedef struct {
    USBDescriptorRecord *current;
    USBDescriptorRecord *lower_bound;
    USBDescriptorRecord *upper_bound;
    uintptr_t type;
} USBDescriptorRecordTuple;

typedef struct {
    USBDescriptorSpan source;
    size_t copy_length;
} USBDescriptorCopyStage;

USB_DESCRIPTOR_ADAPTER_PUBLIC bool usb_descriptor_validate_byte_span_thunk(
    USBDescriptorSpan span
);

USB_DESCRIPTOR_ADAPTER_PUBLIC bool usb_descriptor_validate_span_thunk(
    USBDescriptorSpan span
);

USB_DESCRIPTOR_ADAPTER_PUBLIC bool usb_descriptor_validate_record_tuple(
    USBDescriptorRecordTuple tuple
);

USB_DESCRIPTOR_ADAPTER_PUBLIC USBDescriptorSpan
usb_descriptor_outlined_load_x20_span_bounds(const USBDescriptorSpan *span);

USB_DESCRIPTOR_ADAPTER_PUBLIC USBDescriptorSpan
usb_descriptor_outlined_restore_destination_bounds(
    uint8_t *current,
    uint8_t *lower_bound,
    uint8_t *upper_bound,
    uintptr_t type
);

USB_DESCRIPTOR_ADAPTER_PUBLIC bool usb_descriptor_validate_byte_span(
    USBDescriptorSpan span
);

USB_DESCRIPTOR_ADAPTER_PUBLIC bool usb_configuration_descriptor_validate_tuple(
    USBDescriptorSpan span
);

USB_DESCRIPTOR_ADAPTER_PUBLIC bool
usb_descriptor_outlined_load_source_span_x8_offset_x19(
    USBDescriptorSpan embedded,
    size_t offset,
    USBDescriptorSpan *source
);

USB_DESCRIPTOR_ADAPTER_PUBLIC bool
usb_descriptor_outlined_offset_source_current_and_stage_copy_length(
    USBDescriptorSpan source,
    size_t offset,
    size_t copy_length,
    USBDescriptorCopyStage *stage
);

USB_DESCRIPTOR_ADAPTER_PUBLIC USBDescriptorSpan
usb_descriptor_outlined_restore_destination_span_x27(
    uint8_t *current,
    USBDescriptorSpan saved
);

USB_DESCRIPTOR_ADAPTER_PUBLIC bool
usb_descriptor_outlined_copy_configuration_header_x21(
    USBDescriptorSpan destination,
    const tusb_desc_configuration_t *header
);
