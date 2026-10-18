#pragma once

#include "avp/image/der/apple_libder.h"

#include <stdbool.h>
#include <stdint.h>

#define BOOT_IMG4_MISC_PUBLIC __attribute__((used, retain))

BOOT_IMG4_MISC_PUBLIC void boot_image_mark_property_0x6575726d_true(void);
bool boot_image_property_0x6575726d_is_true(void);
void boot_image_property_0x6575726d_reset(void);
BOOT_IMG4_MISC_PUBLIC int boot_img4_compare_der_integer_tag(
    const DERItem *integer,
    uint32_t expected
);
