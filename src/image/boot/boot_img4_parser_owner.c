#include "avp/image/boot/boot_img4_parser_owner.h"


enum {
    DER_TAG_IA5_STRING = 0x16,
};

static bool boot_img4_parser_bytes_equal(
    const uint8_t *left,
    const uint8_t *right,
    size_t size
) {
    for (size_t index = 0; index < size; ++index) {
        if (left[index] != right[index]) {
            return false;
        }
    }
    return true;
}

static bool boot_img4_parser_item_equals(
    const DERDecodedInfo *item,
    const char text[4]
) {
    return item->tag == DER_TAG_IA5_STRING && item->content.length == 4 &&
        boot_img4_parser_bytes_equal(
            item->content.data,
            (const uint8_t *)text,
            4
        );
}

int boot_img4_decode_expected_der_tag_partial(
    const DERItem *encoded,
    DERTag expected_tag,
    DERDecodedInfo *decoded,
    DERSize *encoded_length
) {
    if (encoded == 0 || decoded == 0 || encoded_length == 0) {
        return DR_ParamErr;
    }
    const DERReturn result = DERDecodeItemPartialBufferGetLength(
        encoded,
        decoded,
        encoded_length
    );
    if (result != DR_Success) {
        return result;
    }
    return decoded->tag == expected_tag
        ? BOOT_IMG4_PARSER_OK
        : BOOT_IMG4_PARSER_REJECTED;
}

int boot_img4_limit_der_item_to_decoded_content(
    DERItem *item,
    const DERItem *decoded_content
) {
    if (item == 0 || decoded_content == 0 || item->data == 0 ||
        decoded_content->data == 0 || item->length > UINTPTR_MAX -
        (uintptr_t)item->data || decoded_content->length > UINTPTR_MAX -
        (uintptr_t)decoded_content->data) {
        return BOOT_IMG4_PARSER_REJECTED;
    }
    const uintptr_t item_begin = (uintptr_t)item->data;
    const uintptr_t item_end = item_begin + item->length;
    const uintptr_t content_begin = (uintptr_t)decoded_content->data;
    const uintptr_t content_end = content_begin + decoded_content->length;
    const uintptr_t begin = item_begin > content_begin
        ? item_begin
        : content_begin;
    const uintptr_t end = item_end < content_end ? item_end : content_end;
    if (begin >= end) {
        item->data = 0;
        item->length = 0;
        return BOOT_IMG4_PARSER_REJECTED;
    }
    item->data = (uint8_t *)begin;
    item->length = end - begin;
    return BOOT_IMG4_PARSER_OK;
}

int boot_img4_consume_expected_der_string(
    DERItem *cursor,
    const uint8_t *expected,
    DERSize expected_length
) {
    if (cursor == 0 || cursor->data == 0 || expected == 0) {
        return BOOT_IMG4_PARSER_REJECTED;
    }
    DERDecodedInfo decoded;
    DERSize encoded_length = 0;
    const int result = boot_img4_decode_expected_der_tag_partial(
        cursor,
        DER_TAG_IA5_STRING,
        &decoded,
        &encoded_length
    );
    const size_t header_length = result == BOOT_IMG4_PARSER_OK
        ? (size_t)(decoded.content.data - cursor->data)
        : 0;
    if (result != BOOT_IMG4_PARSER_OK ||
        decoded.content.length != expected_length ||
        !boot_img4_parser_bytes_equal(
            decoded.content.data,
            expected,
            expected_length
        ) || encoded_length > SIZE_MAX - header_length ||
        header_length + encoded_length > cursor->length) {
        return BOOT_IMG4_PARSER_REJECTED;
    }
    const size_t consumed = header_length + encoded_length;
    cursor->data += consumed;
    cursor->length -= consumed;
    return BOOT_IMG4_PARSER_OK;
}

int boot_img4_parse_container_header(
    const DERItem *encoded,
    uint32_t *payload_type,
    DERSize *accepted_length
) {
    if (encoded == 0 || payload_type == 0 || accepted_length == 0) {
        return BOOT_IMG4_PARSER_REJECTED;
    }
    *payload_type = 0;
    *accepted_length = 0;

    DERDecodedInfo outer;
    DERSize outer_length = 0;
    if (boot_img4_decode_expected_der_tag_partial(
            encoded,
            ASN1_CONSTR_SEQUENCE,
            &outer,
            &outer_length
        ) != BOOT_IMG4_PARSER_OK) {
        return BOOT_IMG4_PARSER_REJECTED;
    }
    DERSequence outer_sequence;
    if (DERDecodeSeqContentInit(&outer.content, &outer_sequence) !=
        DR_Success) {
        return BOOT_IMG4_PARSER_REJECTED;
    }
    DERDecodedInfo magic;
    DERDecodedInfo payload;
    if (DERDecodeSeqNext(&outer_sequence, &magic) != DR_Success ||
        !boot_img4_parser_item_equals(&magic, "IMG4") ||
        DERDecodeSeqNext(&outer_sequence, &payload) != DR_Success ||
        payload.tag != ASN1_CONSTR_SEQUENCE) {
        return BOOT_IMG4_PARSER_REJECTED;
    }

    DERSequence payload_sequence;
    DERDecodedInfo payload_magic;
    DERDecodedInfo type;
    if (DERDecodeSeqContentInit(&payload.content, &payload_sequence) !=
            DR_Success ||
        DERDecodeSeqNext(&payload_sequence, &payload_magic) != DR_Success ||
        !boot_img4_parser_item_equals(&payload_magic, "IM4P") ||
        DERDecodeSeqNext(&payload_sequence, &type) != DR_Success ||
        type.tag != DER_TAG_IA5_STRING || type.content.length != 4) {
        return BOOT_IMG4_PARSER_REJECTED;
    }

    *payload_type = ((uint32_t)type.content.data[0] << 24) |
        ((uint32_t)type.content.data[1] << 16) |
        ((uint32_t)type.content.data[2] << 8) |
        (uint32_t)type.content.data[3];
    const size_t header_length = (size_t)(outer.content.data - encoded->data);
    if (outer_length > SIZE_MAX - header_length) {
        return BOOT_IMG4_PARSER_REJECTED;
    }
    *accepted_length = header_length + outer_length;
    return BOOT_IMG4_PARSER_OK;
}
