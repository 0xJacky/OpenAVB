#pragma once

#include "avp/image/der/apple_libder.h"

#include <stddef.h>
#include <stdint.h>


#define BOOT_IMG4_PARSER_PUBLIC __attribute__((used, retain))

enum {
    BOOT_IMG4_PARSER_OK = 0,
    BOOT_IMG4_PARSER_REJECTED = -1,
};

BOOT_IMG4_PARSER_PUBLIC int boot_img4_parse_container_header(
    const DERItem *encoded,
    uint32_t *payload_type,
    DERSize *accepted_length
);
BOOT_IMG4_PARSER_PUBLIC int boot_img4_decode_expected_der_tag_partial(
    const DERItem *encoded,
    DERTag expected_tag,
    DERDecodedInfo *decoded,
    DERSize *encoded_length
);
BOOT_IMG4_PARSER_PUBLIC int boot_img4_limit_der_item_to_decoded_content(
    DERItem *item,
    const DERItem *decoded_content
);
BOOT_IMG4_PARSER_PUBLIC int boot_img4_consume_expected_der_string(
    DERItem *cursor,
    const uint8_t *expected,
    DERSize expected_length
);
