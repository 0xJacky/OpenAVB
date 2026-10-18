#include "avp/devices/usb/usb_dfu_identity_adapter.h"

#include "avp/devices/usb/usb_identity.h"


static size_t bounded_text_length(const char *text, size_t capacity) {
    size_t length = 0;
    while (length < capacity && text[length] != '\0') {
        ++length;
    }
    return length;
}

static USBIdentityByteSpan byte_span(uint8_t *bytes, size_t size) {
    return (USBIdentityByteSpan){
        .current = bytes,
        .lower_bound = bytes,
        .upper_bound = bytes + size,
        .type = USB_DFU_IDENTITY_BYTE_TYPE,
    };
}

USBIdentityBuildResult usb_build_serial_identity(
    const AVPPlatformIdentity *identity,
    bool source_image_executed,
    uint8_t *output,
    uint32_t capacity
) {
    return (USBIdentityBuildResult){
        .output = byte_span(output, capacity),
        .descriptor_length = usb_identity_emit_string(
            4,
            output,
            capacity,
            UINT16_MAX,
            identity,
            false,
            source_image_executed
        ),
    };
}

USBIdentityBuildResult usb_build_nonce_identity_string(
    const AVPPlatformIdentity *identity,
    uint8_t *output,
    uint32_t capacity
) {
    return (USBIdentityBuildResult){
        .output = byte_span(output, capacity),
        .descriptor_length = usb_identity_emit_string(
            1,
            output,
            capacity,
            UINT16_MAX,
            identity,
            false,
            false
        ),
    };
}

bool usb_identity_validate_byte_span(USBIdentityByteSpan span) {
    const uintptr_t current = (uintptr_t)span.current;
    const uintptr_t lower = (uintptr_t)span.lower_bound;
    const uintptr_t upper = (uintptr_t)span.upper_bound;
    return span.type == USB_DFU_IDENTITY_BYTE_TYPE &&
        lower <= current && current < upper;
}

USBIdentityByteSpan usb_nonce_outlined_prepare_72_byte_output_bounds(
    uint8_t output[72]
) {
    return byte_span(output, USB_DFU_NONCE_BUFFER_SIZE);
}

USBIdentityBoundedStringArguments
usb_identity_outlined_prepare_bounded_string_arguments(
    USBIdentityByteSpan destination,
    const char *source,
    size_t source_length
) {
    return (USBIdentityBoundedStringArguments){
        .destination = destination,
        .source = {
            .current = source,
            .lower_bound = source,
            .upper_bound = source + source_length,
            .type = USB_DFU_IDENTITY_BYTE_TYPE,
        },
        .copy_length = source_length,
    };
}

USBIdentityByteSpan usb_identity_outlined_set_destination_limit_127(
    uint8_t *destination
) {
    return byte_span(destination, USB_DFU_IDENTITY_STRING_CAPACITY);
}

USBIdentityByteSpan usb_identity_outlined_restore_destination_base_and_type(
    uint8_t *destination,
    uint8_t *upper_bound,
    uintptr_t type
) {
    return (USBIdentityByteSpan){
        .current = destination,
        .lower_bound = destination,
        .upper_bound = upper_bound,
        .type = type,
    };
}

USBIdentityByteSpan usb_identity_outlined_global_string_span(
    uint8_t *global_string
) {
    return byte_span(global_string, USB_DFU_IDENTITY_STRING_CAPACITY);
}

USBIdentityTextSpan usb_get_manufacturer_string(void) {
    static const char manufacturer[] = "Apple Inc.";
    return (USBIdentityTextSpan){
        .current = manufacturer,
        .lower_bound = manufacturer,
        .upper_bound = manufacturer + sizeof(manufacturer),
        .type = USB_DFU_IDENTITY_BYTE_TYPE,
    };
}

USBIdentityTextSpan usb_get_product_string(
    const AVPPlatformIdentity *identity
) {
    static const char dfu_product[] = "Apple Mobile Device (DFU Mode)";
    const size_t variant_length = bounded_text_length(
        identity->variant,
        sizeof(identity->variant)
    );
    const char *product = variant_length == 0 ? dfu_product : identity->variant;
    const size_t product_size = variant_length == 0
        ? sizeof(dfu_product)
        : variant_length + 1;
    return (USBIdentityTextSpan){
        .current = product,
        .lower_bound = product,
        .upper_bound = product + product_size,
        .type = USB_DFU_IDENTITY_BYTE_TYPE,
    };
}

uint16_t usb_compute_dfu_product_id(uint8_t security_domain) {
    return (uint16_t)(USB_DFU_PRODUCT_ID_BASE + (security_domain & 3));
}

uint16_t usb_get_apple_vendor_id(void) {
    return USB_APPLE_VENDOR_ID;
}
