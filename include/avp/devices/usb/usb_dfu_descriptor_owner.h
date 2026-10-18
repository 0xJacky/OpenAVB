#pragma once

#include "common/tusb_types.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


#define USB_DFU_DESCRIPTOR_OWNER_PUBLIC __attribute__((used, retain))

enum {
    USB_DFU_DESCRIPTOR_TEXT_TYPE = 1,
    USB_DFU_DESCRIPTOR_IMPLEMENTATION_TYPE = 0x146d40,
    USB_DFU_DESCRIPTOR_FUNCTION_CAPACITY = 2,
    USB_DFU_DESCRIPTOR_STRING_CAPACITY = 8,
    USB_DFU_DESCRIPTOR_STRING_FIRST = 2,
    USB_DFU_DESCRIPTOR_STRING_ARGUMENT_COUNT = 5,
    USB_DFU_DESCRIPTOR_STRING_MAX_CHARS = 126,
    USB_DFU_DESCRIPTOR_STRING_MAX_BYTES = 254,
    USB_DFU_DESCRIPTOR_CONFIGURATION_BYTES = 27,
    USB_DFU_DESCRIPTOR_OK = 0,
    USB_DFU_DESCRIPTOR_INVALID_ARGUMENT = -1,
    USB_DFU_DESCRIPTOR_BOUNDS_FAULT = -2,
    USB_DFU_DESCRIPTOR_BACKEND_FAULT = -3,
};

typedef struct {
    const char *current;
    const char *lower_bound;
    const char *upper_bound;
    uintptr_t type;
} USBDFUDescriptorTextSpan;

typedef struct {
    uintptr_t current;
    uintptr_t lower_bound;
    uintptr_t upper_bound;
    uintptr_t type;
} USBDFUDescriptorImplementationSpan;

typedef struct {
    uint8_t reserved0[0x20];
    uint32_t implementation_count;
    uint8_t reserved24[0x114];
    USBDFUDescriptorImplementationSpan
        implementations[USB_DFU_DESCRIPTOR_FUNCTION_CAPACITY];
} USBDFUDescriptorFunctionState;

typedef struct {
    uint8_t bytes[USB_DFU_DESCRIPTOR_STRING_MAX_BYTES];
    uint16_t length;
    bool occupied;
} USBDFUStringDescriptor;

typedef struct {
    tusb_desc_device_t device;
    uint8_t configuration[USB_DFU_DESCRIPTOR_CONFIGURATION_BYTES];
    uint16_t configuration_length;
    USBDFUStringDescriptor strings[USB_DFU_DESCRIPTOR_STRING_CAPACITY];
    uint8_t string_indices[USB_DFU_DESCRIPTOR_STRING_ARGUMENT_COUNT];
} USBDFUDescriptorSet;

typedef struct {
    USBDFUDescriptorTextSpan strings[USB_DFU_DESCRIPTOR_STRING_ARGUMENT_COUNT];
    uint8_t security_domain;
} USBDFUDescriptorArguments;

typedef struct {
    uint64_t source_token;
    uint32_t source_line;
    uint32_t fault_count;
} USBDFUDescriptorFault;

typedef int (*USBDFUDescriptorInitializeCallback)(
    const USBDFUDescriptorSet *descriptors,
    void *context
);

typedef struct {
    bool use_remote_backend;
    USBDFUDescriptorInitializeCallback local_initialize;
    USBDFUDescriptorInitializeCallback remote_initialize;
    void *context;
} USBDFUDescriptorBackend;

typedef struct {
    USBDFUDescriptorImplementationSpan span;
    bool valid;
    bool current_at_or_above_lower_bound;
} USBDFUDescriptorImplementationBounds;

_Static_assert(sizeof(USBDFUDescriptorTextSpan) == 0x20,
    "USB DFU text span size");
_Static_assert(sizeof(USBDFUDescriptorImplementationSpan) == 0x20,
    "USB DFU implementation span size");
_Static_assert(offsetof(USBDFUDescriptorFunctionState, implementation_count) ==
    0x20, "USB function count offset");
_Static_assert(offsetof(USBDFUDescriptorFunctionState, implementations) ==
    0x138, "USB function implementation array offset");
_Static_assert(sizeof(tusb_desc_device_t) == 18, "USB device descriptor size");
_Static_assert(sizeof(tusb_desc_configuration_t) == 9,
    "USB configuration descriptor size");
_Static_assert(sizeof(tusb_desc_interface_t) == 9,
    "USB interface descriptor size");
_Static_assert(sizeof(tusb_desc_dfu_functional_t) == 9,
    "USB DFU functional descriptor size");

USB_DFU_DESCRIPTOR_OWNER_PUBLIC int
usb_descriptor_build_return_success_thunk(void);

USB_DFU_DESCRIPTOR_OWNER_PUBLIC bool usb_build_dfu_descriptors(
    USBDFUDescriptorSet *set,
    const USBDFUDescriptorArguments *arguments
);

USB_DFU_DESCRIPTOR_OWNER_PUBLIC bool usb_build_string_descriptor(
    USBDFUDescriptorTextSpan source,
    uint8_t *destination,
    size_t capacity,
    uint16_t *length
);

USB_DFU_DESCRIPTOR_OWNER_PUBLIC bool usb_insert_string_descriptor(
    USBDFUDescriptorSet *set,
    USBDFUDescriptorTextSpan source,
    uint8_t *index
);

USB_DFU_DESCRIPTOR_OWNER_PUBLIC bool usb_build_configuration_descriptors(
    USBDFUDescriptorSet *set
);

USB_DFU_DESCRIPTOR_OWNER_PUBLIC bool
usb_function_register_implementation_span(
    USBDFUDescriptorImplementationSpan span
);

USB_DFU_DESCRIPTOR_OWNER_PUBLIC int
usb_function_registration_bounds_panic(void);

USB_DFU_DESCRIPTOR_OWNER_PUBLIC USBDFUDescriptorFunctionState *
usb_outlined_load_device_state_x19(void);

USB_DFU_DESCRIPTOR_OWNER_PUBLIC USBDFUDescriptorImplementationBounds
usb_function_outlined_load_implementation_bounds(uint32_t index);

USB_DFU_DESCRIPTOR_OWNER_PUBLIC bool
usb_descriptor_outlined_stack_guard_compare_saved_x8(uintptr_t saved_guard);

USB_DFU_DESCRIPTOR_OWNER_PUBLIC int usb_dfu_device_initialize(
    USBDFUDescriptorSet *set,
    const USBDFUDescriptorArguments *arguments,
    const USBDFUDescriptorBackend *backend
);

USB_DFU_DESCRIPTOR_OWNER_PUBLIC USBDFUDescriptorArguments
usb_dfu_outlined_prepare_descriptor_arguments(
    const USBDFUDescriptorTextSpan
        strings[USB_DFU_DESCRIPTOR_STRING_ARGUMENT_COUNT],
    uint8_t security_domain
);

void usb_dfu_descriptor_owner_bind_state(
    USBDFUDescriptorFunctionState *state,
    USBDFUDescriptorFault *fault,
    uintptr_t stack_guard
);
