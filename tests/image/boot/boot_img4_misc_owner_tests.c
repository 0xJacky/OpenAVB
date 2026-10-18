#include "avp/image/boot/boot_img4_misc_owner.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    boot_image_property_0x6575726d_reset();
    assert(!boot_image_property_0x6575726d_is_true());
    boot_image_mark_property_0x6575726d_true();
    assert(boot_image_property_0x6575726d_is_true());

    uint8_t equal_bytes[] = {0x01, 0x02, 0x03, 0x04};
    DERItem integer = {.data = equal_bytes, .length = sizeof(equal_bytes)};
    assert(boot_img4_compare_der_integer_tag(&integer, 0x01020304) == 0);
    assert(boot_img4_compare_der_integer_tag(&integer, 0x01020305) == -1);
    assert(boot_img4_compare_der_integer_tag(&integer, 0x01020303) == 1);

    integer.length = 3;
    assert(boot_img4_compare_der_integer_tag(&integer, 0) == -1);
    integer.length = 5;
    assert(boot_img4_compare_der_integer_tag(&integer, 0) == 1);
    uint8_t negative[] = {0x80, 0x00, 0x00, 0x00};
    integer = (DERItem){.data = negative, .length = sizeof(negative)};
    assert(boot_img4_compare_der_integer_tag(&integer, 0) == -2);

    puts("boot IMG4 misc owner tests passed");
    return 0;
}
