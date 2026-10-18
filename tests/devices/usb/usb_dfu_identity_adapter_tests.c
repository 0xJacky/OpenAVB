#include "avp/devices/usb/usb_dfu_identity_adapter.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>


static AVPPlatformIdentity identity_fixture(void) {
    AVPPlatformIdentity identity = {
        .ecid = UINT64_C(0x1122334455667788),
        .chip_id = 0x8015,
        .security_domain = 3,
        .chip_revision = 2,
        .chip_fuse_mode = 1,
        .security_epoch = 4,
        .board_id = 6,
        .boot_flags = 9,
    };
    for (size_t index = 0; index < sizeof(identity.ap_nonce); ++index) {
        identity.ap_nonce[index] = (uint8_t)index;
    }
    for (size_t index = 0; index < sizeof(identity.sep_nonce); ++index) {
        identity.sep_nonce[index] = (uint8_t)(0xa0 + index);
    }
    return identity;
}

static void assert_utf16_prefix(
    const uint8_t *descriptor,
    const char *prefix
) {
    assert(descriptor[1] == 3);
    for (size_t index = 0; prefix[index] != '\0'; ++index) {
        assert(descriptor[2 + index * 2] == (uint8_t)prefix[index]);
        assert(descriptor[3 + index * 2] == 0);
    }
}

static void test_serial_and_nonce_builders(void) {
    AVPPlatformIdentity identity = identity_fixture();
    uint8_t serial[384] = {0};
    USBIdentityBuildResult result = usb_build_serial_identity(
        &identity,
        true,
        serial,
        sizeof(serial)
    );
    assert(result.descriptor_length == serial[0]);
    assert(result.output.current == serial);
    assert_utf16_prefix(serial, "SDOM:03 CPID:8015");

    uint8_t nonce[256] = {0};
    result = usb_build_nonce_identity_string(
        &identity,
        nonce,
        sizeof(nonce)
    );
    assert(result.descriptor_length == nonce[0]);
    assert_utf16_prefix(nonce, " NONC:00010203");
}

static void test_identity_span_outliners(void) {
    uint8_t nonce[USB_DFU_NONCE_BUFFER_SIZE] = {0};
    USBIdentityByteSpan span =
        usb_nonce_outlined_prepare_72_byte_output_bounds(nonce);
    assert(span.current == nonce);
    assert(span.lower_bound == nonce);
    assert(span.upper_bound == nonce + sizeof(nonce));
    assert(usb_identity_validate_byte_span(span));
    span.type = 9;
    assert(!usb_identity_validate_byte_span(span));

    uint8_t destination[USB_DFU_IDENTITY_STRING_CAPACITY] = {0};
    span = usb_identity_outlined_set_destination_limit_127(destination);
    assert(span.upper_bound == destination + 127);
    const USBIdentityBoundedStringArguments arguments =
        usb_identity_outlined_prepare_bounded_string_arguments(
            span,
            "hello",
            5
        );
    assert(arguments.destination.current == destination);
    assert(arguments.source.current == arguments.source.lower_bound);
    assert(arguments.source.upper_bound - arguments.source.current == 5);
    assert(arguments.copy_length == 5);

    USBIdentityByteSpan restored =
        usb_identity_outlined_restore_destination_base_and_type(
            destination,
            destination + sizeof(destination),
            USB_DFU_IDENTITY_BYTE_TYPE
        );
    assert(restored.current == destination);
    assert(restored.lower_bound == destination);
    assert(restored.upper_bound == destination + sizeof(destination));

    const USBIdentityByteSpan global =
        usb_identity_outlined_global_string_span(destination);
    assert(global.upper_bound == destination + 127);
    assert(usb_identity_validate_byte_span(global));
}

static void test_identity_constants_and_text(void) {
    const USBIdentityTextSpan manufacturer = usb_get_manufacturer_string();
    assert(strcmp(manufacturer.current, "Apple Inc.") == 0);
    assert(manufacturer.upper_bound - manufacturer.current == 11);

    AVPPlatformIdentity identity = identity_fixture();
    USBIdentityTextSpan product = usb_get_product_string(&identity);
    assert(strcmp(product.current, "Apple Mobile Device (DFU Mode)") == 0);
    memcpy(identity.variant, "Research Device", sizeof("Research Device"));
    product = usb_get_product_string(&identity);
    assert(strcmp(product.current, "Research Device") == 0);
    assert(product.upper_bound - product.current ==
        (ptrdiff_t)sizeof("Research Device"));

    assert(usb_compute_dfu_product_id(0) == 0x1226);
    assert(usb_compute_dfu_product_id(3) == 0x1229);
    assert(usb_compute_dfu_product_id(7) == 0x1229);
    assert(usb_get_apple_vendor_id() == 0x05ac);
}

int main(void) {
    test_serial_and_nonce_builders();
    test_identity_span_outliners();
    test_identity_constants_and_text();
    puts("usb_dfu_identity_adapter_tests: ok");
    return 0;
}
