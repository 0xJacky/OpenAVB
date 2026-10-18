#pragma once

#include "avp/image/img4/img4_context_compat.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BOOT_IMG4_PROPERTY_PUBLIC __attribute__((used, retain))

typedef enum {
    BOOT_IMG4_PROPERTY_EQUAL = 0,
    BOOT_IMG4_PROPERTY_MINIMUM = 1,
} BootIMG4PropertyComparison;

BOOT_IMG4_PROPERTY_PUBLIC int boot_img4_validate_integer_property(
    BootIMG4PropertyComparison comparison,
    const DERItem *property,
    DERTag property_tag,
    uint64_t expected
);
BOOT_IMG4_PROPERTY_PUBLIC int boot_img4_validate_boolean_property(
    const DERItem *property,
    DERTag property_tag,
    bool expected
);
BOOT_IMG4_PROPERTY_PUBLIC int boot_img4_validate_data_property(
    BootIMG4PropertyComparison comparison,
    const DERItem *property,
    DERTag property_tag,
    const uint8_t *expected,
    size_t expected_length
);
BOOT_IMG4_PROPERTY_PUBLIC int boot_img4_validate_true_boolean_property(
    const DERItem *property,
    DERTag property_tag
);
BOOT_IMG4_PROPERTY_PUBLIC uint32_t boot_img4_decode_fourcc_be(uint32_t value);
