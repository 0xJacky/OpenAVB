#include "avp/image/boot/boot_img4_misc_owner.h"

#if defined(__ELF__)
static bool property_0x6575726d
    __attribute__((section(".runtime_auxiliary.state"), used));
#else
static bool property_0x6575726d;
#endif

void boot_image_mark_property_0x6575726d_true(void) {
    property_0x6575726d = true;
}

bool boot_image_property_0x6575726d_is_true(void) {
    return property_0x6575726d;
}

void boot_image_property_0x6575726d_reset(void) {
    property_0x6575726d = false;
}

int boot_img4_compare_der_integer_tag(
    const DERItem *integer,
    uint32_t expected
) {
    if (integer->length < sizeof(uint32_t)) {
        return -1;
    }
    if (integer->length > sizeof(uint32_t)) {
        return 1;
    }
    uint32_t actual = 0;
    if (DERParseInteger(integer, &actual) != DR_Success) {
        return -2;
    }
    return actual < expected ? -1 : actual > expected ? 1 : 0;
}
