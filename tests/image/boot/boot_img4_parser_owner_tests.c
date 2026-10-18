#include "avp/image/boot/boot_img4_parser_owner.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>


static void test_partial_decode_and_cursor(void) {
    uint8_t encoded[] = {0x16, 0x04, 'I', 'M', 'G', '4', 0xaa};
    DERItem item = {encoded, sizeof(encoded)};
    DERDecodedInfo decoded;
    DERSize encoded_length = 0;
    assert(boot_img4_decode_expected_der_tag_partial(
        &item,
        0x16,
        &decoded,
        &encoded_length
    ) == 0);
    assert(encoded_length == 4);
    assert(decoded.content.length == 4);
    assert(boot_img4_decode_expected_der_tag_partial(
        &item,
        0x04,
        &decoded,
        &encoded_length
    ) == -1);

    assert(boot_img4_consume_expected_der_string(
        &item,
        (const uint8_t *)"IMG4",
        4
    ) == 0);
    assert(item.data == encoded + 6);
    assert(item.length == 1);
}

static void test_intersection(void) {
    uint8_t bytes[32];
    DERItem item = {bytes, sizeof(bytes)};
    DERItem content = {bytes + 4, 8};
    assert(boot_img4_limit_der_item_to_decoded_content(&item, &content) == 0);
    assert(item.data == bytes + 4);
    assert(item.length == 8);

    item = (DERItem){bytes, 4};
    content = (DERItem){bytes + 8, 4};
    assert(boot_img4_limit_der_item_to_decoded_content(&item, &content) == -1);
    assert(item.data == 0 && item.length == 0);
}

static void test_container_header(void) {
    const uint8_t encoded[] = {
        0x30, 0x10,
        0x16, 0x04, 'I', 'M', 'G', '4',
        0x30, 0x08,
        0x16, 0x04, 'I', 'M', '4', 'P',
        0x16, 0x00,
    };
    uint8_t image[sizeof(encoded) + 4];
    memcpy(image, encoded, sizeof(encoded));
    image[sizeof(encoded) - 2] = 0x16;
    image[sizeof(encoded) - 1] = 0x04;
    image[sizeof(encoded)] = 'i';
    image[sizeof(encoded) + 1] = 'b';
    image[sizeof(encoded) + 2] = 's';
    image[sizeof(encoded) + 3] = 's';
    image[1] = 0x14;
    image[9] = 0x0c;

    DERItem item = {image, sizeof(image)};
    uint32_t type = 0;
    DERSize accepted = 0;
    assert(boot_img4_parse_container_header(&item, &type, &accepted) == 0);
    assert(type == UINT32_C(0x69627373));
    assert(accepted == sizeof(image));

    image[4] = 'X';
    assert(boot_img4_parse_container_header(&item, &type, &accepted) == -1);
}

int main(void) {
    test_partial_decode_and_cursor();
    test_intersection();
    test_container_header();
}
