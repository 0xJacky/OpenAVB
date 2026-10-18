#pragma once

#include "avp/platform/platform.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


#define USB_DFU_IDENTITY_PUBLIC __attribute__((used, retain))

enum {
    USB_DFU_IDENTITY_BYTE_TYPE = 1,
    USB_DFU_NONCE_BUFFER_SIZE = 72,
    USB_DFU_IDENTITY_STRING_CAPACITY = 127,
    USB_DFU_PRODUCT_ID_BASE = 0x1226,
    USB_APPLE_VENDOR_ID = 0x05ac,
};

typedef struct {
    uint8_t *current;
    uint8_t *lower_bound;
    uint8_t *upper_bound;
    uintptr_t type;
} USBIdentityByteSpan;

_Static_assert(sizeof(USBIdentityByteSpan) == 0x20, "USB identity span size");

typedef struct {
    const char *current;
    const char *lower_bound;
    const char *upper_bound;
    uintptr_t type;
} USBIdentityTextSpan;

typedef struct {
    USBIdentityByteSpan destination;
    USBIdentityTextSpan source;
    size_t copy_length;
} USBIdentityBoundedStringArguments;

typedef struct {
    USBIdentityByteSpan output;
    uint32_t descriptor_length;
} USBIdentityBuildResult;

USB_DFU_IDENTITY_PUBLIC USBIdentityBuildResult usb_build_serial_identity(
    const AVPPlatformIdentity *identity,
    bool source_image_executed,
    uint8_t *output,
    uint32_t capacity
);

USB_DFU_IDENTITY_PUBLIC USBIdentityBuildResult
usb_build_nonce_identity_string(
    const AVPPlatformIdentity *identity,
    uint8_t *output,
    uint32_t capacity
);

USB_DFU_IDENTITY_PUBLIC bool usb_identity_validate_byte_span(
    USBIdentityByteSpan span
);

USB_DFU_IDENTITY_PUBLIC USBIdentityByteSpan
usb_nonce_outlined_prepare_72_byte_output_bounds(uint8_t output[72]);

USB_DFU_IDENTITY_PUBLIC USBIdentityBoundedStringArguments
usb_identity_outlined_prepare_bounded_string_arguments(
    USBIdentityByteSpan destination,
    const char *source,
    size_t source_length
);

USB_DFU_IDENTITY_PUBLIC USBIdentityByteSpan
usb_identity_outlined_set_destination_limit_127(uint8_t *destination);

USB_DFU_IDENTITY_PUBLIC USBIdentityByteSpan
usb_identity_outlined_restore_destination_base_and_type(
    uint8_t *destination,
    uint8_t *upper_bound,
    uintptr_t type
);

USB_DFU_IDENTITY_PUBLIC USBIdentityByteSpan
usb_identity_outlined_global_string_span(uint8_t *global_string);

USB_DFU_IDENTITY_PUBLIC USBIdentityTextSpan usb_get_manufacturer_string(void);

USB_DFU_IDENTITY_PUBLIC USBIdentityTextSpan usb_get_product_string(
    const AVPPlatformIdentity *identity
);

USB_DFU_IDENTITY_PUBLIC uint16_t usb_compute_dfu_product_id(
    uint8_t security_domain
);

USB_DFU_IDENTITY_PUBLIC uint16_t usb_get_apple_vendor_id(void);
