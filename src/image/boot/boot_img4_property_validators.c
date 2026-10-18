#include "avp/image/boot/boot_img4_property_validators.h"

#include <limits.h>

static int compare_bytes(
    const uint8_t *left,
    const uint8_t *right,
    size_t length
) {
    for (size_t index = 0; index < length; ++index) {
        if (left[index] < right[index]) {
            return -1;
        }
        if (left[index] > right[index]) {
            return 1;
        }
    }
    return 0;
}

int boot_img4_validate_integer_property(
    BootIMG4PropertyComparison comparison,
    const DERItem *property,
    DERTag property_tag,
    uint64_t expected
) {
    uint64_t actual = 0;
    if (Img4DecodeGetPropertyInteger64(property, property_tag, &actual) !=
        DR_Success) {
        return -1;
    }
    if (comparison == BOOT_IMG4_PROPERTY_EQUAL) {
        return actual == expected ? 0 : -1;
    }
    if (comparison == BOOT_IMG4_PROPERTY_MINIMUM) {
        return actual >= expected ? 0 : -1;
    }
    return -1;
}

int boot_img4_validate_boolean_property(
    const DERItem *property,
    DERTag property_tag,
    bool expected
) {
    bool actual = false;
    if (Img4DecodeGetPropertyBoolean(property, property_tag, &actual) !=
        DR_Success) {
        return -1;
    }
    return actual == expected ? 0 : -1;
}

int boot_img4_validate_data_property(
    BootIMG4PropertyComparison comparison,
    const DERItem *property,
    DERTag property_tag,
    const uint8_t *expected,
    size_t expected_length
) {
    if (expected_length > UINT32_MAX ||
        (expected == NULL && expected_length != 0)) {
        return -1;
    }
    const uint8_t *actual = NULL;
    uint32_t actual_length = 0;
    if (Img4DecodeGetPropertyData(
            property, property_tag, &actual, &actual_length
        ) != DR_Success || actual_length != expected_length) {
        return -1;
    }
    const int order = compare_bytes(actual, expected, expected_length);
    if (comparison == BOOT_IMG4_PROPERTY_EQUAL) {
        return order == 0 ? 0 : -1;
    }
    if (comparison == BOOT_IMG4_PROPERTY_MINIMUM) {
        return order >= 0 ? 0 : -1;
    }
    return -1;
}

int boot_img4_validate_true_boolean_property(
    const DERItem *property,
    DERTag property_tag
) {
    return boot_img4_validate_boolean_property(property, property_tag, true);
}

uint32_t boot_img4_decode_fourcc_be(uint32_t value) {
    return ((value & UINT32_C(0x000000ff)) << 24) |
           ((value & UINT32_C(0x0000ff00)) << 8) |
           ((value & UINT32_C(0x00ff0000)) >> 8) |
           ((value & UINT32_C(0xff000000)) >> 24);
}
