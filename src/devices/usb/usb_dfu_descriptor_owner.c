#include "avp/devices/usb/usb_dfu_descriptor_owner.h"

#include "avp/devices/usb/usb_dfu_identity_adapter.h"
#include "device/usbd.h"


enum {
    USB_DFU_DESCRIPTOR_SOURCE_LINE = 0x1c4,
    USB_DFU_INTERFACE_CLASS = 0xfe,
    USB_DFU_INTERFACE_SUBCLASS = 1,
    USB_DFU_INTERFACE_PROTOCOL = 2,
    USB_DFU_ATTRIBUTES = 1,
    USB_DFU_DETACH_TIMEOUT = 10,
    USB_DFU_TRANSFER_SIZE = 0x800,
    USB_DFU_VERSION = 0x0110,
};

static USBDFUDescriptorFunctionState *bound_state;
static USBDFUDescriptorFault *bound_fault;
static uintptr_t active_stack_guard;

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

static bool text_span_valid(USBDFUDescriptorTextSpan span, size_t *length) {
    if (span.type != USB_DFU_DESCRIPTOR_TEXT_TYPE || span.current == 0 ||
        span.lower_bound == 0 || span.upper_bound == 0 ||
        span.current < span.lower_bound || span.current > span.upper_bound) {
        return false;
    }
    const size_t available = (size_t)(span.upper_bound - span.current);
    size_t count = 0;
    while (count < available && span.current[count] != '\0') {
        ++count;
    }
    if (count == available || count > USB_DFU_DESCRIPTOR_STRING_MAX_CHARS) {
        return false;
    }
    *length = count;
    return true;
}

void usb_dfu_descriptor_owner_bind_state(
    USBDFUDescriptorFunctionState *state,
    USBDFUDescriptorFault *fault,
    uintptr_t stack_guard
) {
    bound_state = state;
    bound_fault = fault;
    active_stack_guard = stack_guard;
}

int usb_descriptor_build_return_success_thunk(void) {
    return USB_DFU_DESCRIPTOR_OK;
}

bool usb_build_string_descriptor(
    USBDFUDescriptorTextSpan source,
    uint8_t *destination,
    size_t capacity,
    uint16_t *length
) {
    size_t character_count;
    if (destination == 0 || length == 0 ||
        !text_span_valid(source, &character_count)) {
        return false;
    }
    const size_t descriptor_length = 2 + character_count * 2;
    if (descriptor_length > capacity) {
        return false;
    }
    tusb_desc_string_t *const descriptor = (tusb_desc_string_t *)destination;
    descriptor->bLength = (uint8_t)descriptor_length;
    descriptor->bDescriptorType = TUSB_DESC_STRING;
    for (size_t index = 0; index < character_count; ++index) {
        destination[2 + index * 2] = (uint8_t)source.current[index];
        destination[3 + index * 2] = 0;
    }
    *length = (uint16_t)descriptor_length;
    return true;
}

bool usb_insert_string_descriptor(
    USBDFUDescriptorSet *set,
    USBDFUDescriptorTextSpan source,
    uint8_t *index
) {
    if (set == 0 || index == 0) {
        return false;
    }
    for (uint8_t candidate = USB_DFU_DESCRIPTOR_STRING_FIRST;
         candidate < USB_DFU_DESCRIPTOR_STRING_CAPACITY; ++candidate) {
        USBDFUStringDescriptor *const string = &set->strings[candidate];
        if (!string->occupied && usb_build_string_descriptor(
                source,
                string->bytes,
                sizeof(string->bytes),
                &string->length
            )) {
            string->occupied = true;
            *index = candidate;
            return true;
        }
    }
    return false;
}

bool usb_build_configuration_descriptors(USBDFUDescriptorSet *set) {
    if (set == 0) {
        return false;
    }
    const tusb_desc_configuration_t configuration = {
        .bLength = sizeof(tusb_desc_configuration_t),
        .bDescriptorType = TUSB_DESC_CONFIGURATION,
        .wTotalLength = USB_DFU_DESCRIPTOR_CONFIGURATION_BYTES,
        .bNumInterfaces = 1,
        .bConfigurationValue = 1,
        .iConfiguration = set->string_indices[3],
        .bmAttributes = 0x80,
        .bMaxPower = 0xfa,
    };
    const tusb_desc_interface_t interface = {
        .bLength = sizeof(tusb_desc_interface_t),
        .bDescriptorType = TUSB_DESC_INTERFACE,
        .bInterfaceNumber = 0,
        .bAlternateSetting = 0,
        .bNumEndpoints = 0,
        .bInterfaceClass = USB_DFU_INTERFACE_CLASS,
        .bInterfaceSubClass = USB_DFU_INTERFACE_SUBCLASS,
        .bInterfaceProtocol = USB_DFU_INTERFACE_PROTOCOL,
        .iInterface = set->string_indices[4],
    };
    const tusb_desc_dfu_functional_t functional = {
        .bLength = sizeof(tusb_desc_dfu_functional_t),
        .bDescriptorType = TUSB_DESC_FUNCTIONAL,
        .bAttributes = USB_DFU_ATTRIBUTES,
        .wDetachTimeOut = USB_DFU_DETACH_TIMEOUT,
        .wTransferSize = USB_DFU_TRANSFER_SIZE,
        .bcdDFUVersion = USB_DFU_VERSION,
    };
    size_t offset = 0;
    copy_bytes(set->configuration + offset, &configuration, sizeof(configuration));
    offset += sizeof(configuration);
    copy_bytes(set->configuration + offset, &interface, sizeof(interface));
    offset += sizeof(interface);
    copy_bytes(set->configuration + offset, &functional, sizeof(functional));
    offset += sizeof(functional);
    set->configuration_length = (uint16_t)offset;
    return offset == sizeof(set->configuration);
}

bool usb_build_dfu_descriptors(
    USBDFUDescriptorSet *set,
    const USBDFUDescriptorArguments *arguments
) {
    if (set == 0 || arguments == 0) {
        return false;
    }
    clear_bytes(set, sizeof(*set));
    for (size_t slot = 0; slot < USB_DFU_DESCRIPTOR_STRING_ARGUMENT_COUNT;
         ++slot) {
        if (!usb_insert_string_descriptor(
                set,
                arguments->strings[slot],
                &set->string_indices[slot]
            )) {
            return false;
        }
    }
    set->device = (tusb_desc_device_t){
        .bLength = sizeof(tusb_desc_device_t),
        .bDescriptorType = TUSB_DESC_DEVICE,
        .bcdUSB = 0x0200,
        .bDeviceClass = 0,
        .bDeviceSubClass = 0,
        .bDeviceProtocol = 0,
        .bMaxPacketSize0 = 0x40,
        .idVendor = usb_get_apple_vendor_id(),
        .idProduct = usb_compute_dfu_product_id(arguments->security_domain),
        .bcdDevice = 0,
        .iManufacturer = set->string_indices[0],
        .iProduct = set->string_indices[1],
        .iSerialNumber = set->string_indices[2],
        .bNumConfigurations = 1,
    };
    return usb_build_configuration_descriptors(set);
}

USBDFUDescriptorFunctionState *usb_outlined_load_device_state_x19(void) {
    return bound_state;
}

int usb_function_registration_bounds_panic(void) {
    if (bound_fault != 0) {
        bound_fault->source_token = UINT64_C(0x0a033545e4350115);
        bound_fault->source_line = USB_DFU_DESCRIPTOR_SOURCE_LINE;
        ++bound_fault->fault_count;
    }
    return USB_DFU_DESCRIPTOR_BOUNDS_FAULT;
}

bool usb_function_register_implementation_span(
    USBDFUDescriptorImplementationSpan span
) {
    USBDFUDescriptorFunctionState *const state =
        usb_outlined_load_device_state_x19();
    if (state == 0 || state->implementation_count >=
        USB_DFU_DESCRIPTOR_FUNCTION_CAPACITY) {
        (void)usb_function_registration_bounds_panic();
        return false;
    }
    const uintptr_t slot = (uintptr_t)&state->implementations[
        state->implementation_count];
    if (slot < (uintptr_t)&state->implementations[0]) {
        (void)usb_function_registration_bounds_panic();
        return false;
    }
    state->implementations[state->implementation_count] = span;
    ++state->implementation_count;
    return true;
}

USBDFUDescriptorImplementationBounds
usb_function_outlined_load_implementation_bounds(uint32_t index) {
    USBDFUDescriptorImplementationBounds result = {0};
    USBDFUDescriptorFunctionState *const state =
        usb_outlined_load_device_state_x19();
    if (state == 0 || index >= state->implementation_count ||
        index >= USB_DFU_DESCRIPTOR_FUNCTION_CAPACITY) {
        return result;
    }
    result.span = state->implementations[index];
    result.valid = result.span.lower_bound <= result.span.current &&
        result.span.current < result.span.upper_bound;
    result.current_at_or_above_lower_bound =
        result.span.current >= result.span.lower_bound;
    return result;
}

bool usb_descriptor_outlined_stack_guard_compare_saved_x8(
    uintptr_t saved_guard
) {
    return active_stack_guard == saved_guard;
}

USBDFUDescriptorArguments usb_dfu_outlined_prepare_descriptor_arguments(
    const USBDFUDescriptorTextSpan
        strings[USB_DFU_DESCRIPTOR_STRING_ARGUMENT_COUNT],
    uint8_t security_domain
) {
    USBDFUDescriptorArguments arguments = { .security_domain = security_domain };
    if (strings != 0) {
        for (size_t index = 0;
             index < USB_DFU_DESCRIPTOR_STRING_ARGUMENT_COUNT; ++index) {
            arguments.strings[index] = strings[index];
        }
    }
    return arguments;
}

int usb_dfu_device_initialize(
    USBDFUDescriptorSet *set,
    const USBDFUDescriptorArguments *arguments,
    const USBDFUDescriptorBackend *backend
) {
    if (backend == 0 || !usb_build_dfu_descriptors(set, arguments)) {
        return USB_DFU_DESCRIPTOR_INVALID_ARGUMENT;
    }
    USBDFUDescriptorInitializeCallback initialize =
        backend->use_remote_backend ? backend->remote_initialize :
        backend->local_initialize;
    if (initialize == 0) {
        return USB_DFU_DESCRIPTOR_BACKEND_FAULT;
    }
    return initialize(set, backend->context);
}
