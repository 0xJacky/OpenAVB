#include "avp/image/boot/boot_img4_property_validators.h"

#include <assert.h>
#include <stdio.h>

static DERReturn decode_result;
static uint64_t integer_value;
static bool boolean_value;
static const uint8_t *data_value;
static uint32_t data_length;

DERReturn Img4DecodeGetPropertyInteger64(
    const DERItem *property,
    DERTag property_tag,
    uint64_t *value
) {
    (void)property;
    (void)property_tag;
    *value = integer_value;
    return decode_result;
}

DERReturn Img4DecodeGetPropertyBoolean(
    const DERItem *property,
    DERTag property_tag,
    bool *value
) {
    (void)property;
    (void)property_tag;
    *value = boolean_value;
    return decode_result;
}

DERReturn Img4DecodeGetPropertyData(
    const DERItem *property,
    DERTag property_tag,
    const uint8_t **data,
    uint32_t *length
) {
    (void)property;
    (void)property_tag;
    *data = data_value;
    *length = data_length;
    return decode_result;
}

int main(void) {
    assert(boot_img4_decode_fourcc_be(UINT32_C(0x494d3450)) ==
           UINT32_C(0x50344d49));
    const DERItem property = {0};
    decode_result = DR_Success;
    integer_value = 7;
    assert(boot_img4_validate_integer_property(
               BOOT_IMG4_PROPERTY_EQUAL, &property, 1, 7) == 0);
    assert(boot_img4_validate_integer_property(
               BOOT_IMG4_PROPERTY_MINIMUM, &property, 1, 6) == 0);
    assert(boot_img4_validate_integer_property(
               BOOT_IMG4_PROPERTY_MINIMUM, &property, 1, 8) == -1);

    boolean_value = true;
    assert(boot_img4_validate_true_boolean_property(&property, 2) == 0);
    assert(boot_img4_validate_boolean_property(&property, 2, false) == -1);

    const uint8_t actual[] = {2, 4, 6};
    const uint8_t lower[] = {2, 4, 5};
    data_value = actual;
    data_length = sizeof(actual);
    assert(boot_img4_validate_data_property(
               BOOT_IMG4_PROPERTY_EQUAL, &property, 3, actual,
               sizeof(actual)) == 0);
    assert(boot_img4_validate_data_property(
               BOOT_IMG4_PROPERTY_MINIMUM, &property, 3, lower,
               sizeof(lower)) == 0);
    assert(boot_img4_validate_data_property(
               BOOT_IMG4_PROPERTY_EQUAL, &property, 3, lower,
               sizeof(lower)) == -1);

    decode_result = DR_DecodeError;
    assert(boot_img4_validate_integer_property(
               BOOT_IMG4_PROPERTY_EQUAL, &property, 1, 7) == -1);
    assert(boot_img4_validate_boolean_property(&property, 2, true) == -1);
    assert(boot_img4_validate_data_property(
               BOOT_IMG4_PROPERTY_EQUAL, &property, 3, actual,
               sizeof(actual)) == -1);
    puts("boot IMG4 property validator tests passed");
    return 0;
}
