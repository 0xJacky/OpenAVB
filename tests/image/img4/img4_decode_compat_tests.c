#include "avp/image/img4/img4_decode_compat.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint8_t bytes[2048];
    size_t length;
} Buffer;

_Static_assert(sizeof(Img4DecodedContainer) == 0x40,
               "IMG4 container ABI size");
_Static_assert(sizeof(Img4DecodedPayload) == 0x60,
               "IMG4 payload ABI size");
_Static_assert(sizeof(Img4DecodedPayloadWithProperties) == 0xa0,
               "IMG4 extended payload ABI size");
_Static_assert(sizeof(Img4DecodedManifest) == 0xc0,
               "IMG4 manifest ABI size");
_Static_assert(sizeof(Img4DecodedRestoreInfo) == 0x20,
               "IMG4 restore ABI size");
_Static_assert(sizeof(Img4Property) == 0x18, "IMG4 property ABI size");
_Static_assert(sizeof(Img4DecodedProperty) == 0x30,
               "IMG4 decoded property ABI size");

static void append(Buffer *buffer, const void *bytes, size_t length) {
    assert(buffer->length + length <= sizeof(buffer->bytes));
    memcpy(buffer->bytes + buffer->length, bytes, length);
    buffer->length += length;
}

static uint8_t *find_bytes(
    Buffer *buffer,
    const void *needle,
    size_t needle_length
) {
    assert(needle_length <= buffer->length);
    for (size_t offset = 0; offset <= buffer->length - needle_length;
         ++offset) {
        if (memcmp(buffer->bytes + offset, needle, needle_length) == 0) {
            return buffer->bytes + offset;
        }
    }
    return NULL;
}

static void append_length(Buffer *buffer, size_t length) {
    if (length < 128) {
        const uint8_t byte = (uint8_t)length;
        append(buffer, &byte, 1);
    } else {
        assert(length <= UINT8_MAX);
        const uint8_t bytes[] = {0x81, (uint8_t)length};
        append(buffer, bytes, sizeof(bytes));
    }
}

static void append_tag(
    Buffer *buffer,
    uint8_t tag_class,
    uint32_t tag_number
) {
    if (tag_number < 31) {
        const uint8_t byte = tag_class | (uint8_t)tag_number;
        append(buffer, &byte, 1);
        return;
    }
    const uint8_t first = tag_class | 0x1f;
    append(buffer, &first, 1);
    uint8_t encoded[5];
    size_t count = 0;
    do {
        encoded[count++] = (uint8_t)(tag_number & 0x7f);
        tag_number >>= 7;
    } while (tag_number != 0);
    while (count > 0) {
        --count;
        uint8_t byte = encoded[count];
        if (count != 0) {
            byte |= 0x80;
        }
        append(buffer, &byte, 1);
    }
}

static void append_tlv(
    Buffer *buffer,
    uint8_t tag_class,
    uint32_t tag_number,
    const Buffer *content
) {
    append_tag(buffer, tag_class, tag_number);
    append_length(buffer, content->length);
    append(buffer, content->bytes, content->length);
}

static void append_bytes(
    Buffer *buffer,
    uint8_t tag,
    const void *bytes,
    size_t length
) {
    Buffer content = {0};
    append(&content, bytes, length);
    append_tlv(buffer, 0, tag, &content);
}

static void append_fourcc(Buffer *buffer, const char *fourcc) {
    append_bytes(buffer, 22, fourcc, 4);
}

static void append_integer(Buffer *buffer, uint32_t value) {
    Buffer content = {0};
    if (value == 0) {
        const uint8_t zero = 0;
        append(&content, &zero, 1);
    } else {
        uint8_t bytes[5];
        size_t count = 0;
        while (value != 0) {
            bytes[4 - count++] = (uint8_t)value;
            value >>= 8;
        }
        size_t start = 5 - count;
        if ((bytes[start] & 0x80) != 0) {
            bytes[--start] = 0;
            ++count;
        }
        append(&content, bytes + start, count);
    }
    append_tlv(buffer, 0, 2, &content);
}

static Buffer wrap(uint8_t tag_class, uint32_t tag, const Buffer *content) {
    Buffer encoded = {0};
    append_tlv(&encoded, tag_class, tag, content);
    return encoded;
}

static Buffer make_property(
    const char *key,
    uint8_t value_tag_class,
    uint32_t value_tag,
    const void *value,
    size_t value_length
) {
    Buffer fields = {0};
    append_fourcc(&fields, key);
    Buffer value_content = {0};
    append(&value_content, value, value_length);
    append_tlv(&fields, value_tag_class, value_tag, &value_content);
    const Buffer sequence = wrap(0x20, 16, &fields);
    const uint32_t tag = ((uint32_t)(uint8_t)key[0] << 24) |
                         ((uint32_t)(uint8_t)key[1] << 16) |
                         ((uint32_t)(uint8_t)key[2] << 8) |
                         (uint8_t)key[3];
    return wrap(0xe0, tag, &sequence);
}

static Buffer make_payload(void) {
    Buffer compression_fields = {0};
    append_integer(&compression_fields, 1);
    append_integer(&compression_fields, 0x1234);
    const Buffer compression = wrap(0x20, 16, &compression_fields);

    const uint8_t property_value[] = {0xaa, 0xbb};
    const Buffer property = make_property(
        "BORD",
        0,
        4,
        property_value,
        sizeof(property_value)
    );
    Buffer property_set_content = {0};
    append(&property_set_content, property.bytes, property.length);
    const Buffer property_set = wrap(0x20, 17, &property_set_content);
    Buffer payp_fields = {0};
    append_fourcc(&payp_fields, "PAYP");
    append(&payp_fields, property_set.bytes, property_set.length);
    const Buffer payp = wrap(0x20, 16, &payp_fields);

    Buffer fields = {0};
    append_fourcc(&fields, "IM4P");
    append_fourcc(&fields, "ibot");
    append_fourcc(&fields, "1.0\0");
    const uint8_t payload_bytes[] = {1, 2, 3, 4, 5};
    append_bytes(&fields, 4, payload_bytes, sizeof(payload_bytes));
    const uint8_t keybag[] = {9, 8, 7};
    append_bytes(&fields, 4, keybag, sizeof(keybag));
    append(&fields, compression.bytes, compression.length);
    const Buffer properties_context = wrap(0xa0, 0, &payp);
    append(&fields, properties_context.bytes, properties_context.length);
    return wrap(0x20, 16, &fields);
}

static Buffer make_manifest(uint32_t version) {
    const Buffer body = make_property("MANB", 0x20, 17, NULL, 0);
    Buffer properties_content = {0};
    append(&properties_content, body.bytes, body.length);
    const Buffer properties = wrap(0x20, 17, &properties_content);
    Buffer certificates_content = {0};
    const Buffer certificates = wrap(0x20, 16, &certificates_content);

    Buffer fields = {0};
    append_fourcc(&fields, "IM4M");
    append_integer(&fields, version);
    append(&fields, properties.bytes, properties.length);
    const uint8_t signature[] = {0x55, 0xaa};
    append_bytes(&fields, 4, signature, sizeof(signature));
    append(&fields, certificates.bytes, certificates.length);
    return wrap(0x20, 16, &fields);
}

static Buffer make_restore(void) {
    const uint8_t nonce[] = {1, 3, 3, 7};
    const Buffer property = make_property("BNCN", 0, 4, nonce, sizeof(nonce));
    Buffer set_content = {0};
    append(&set_content, property.bytes, property.length);
    const Buffer set = wrap(0x20, 17, &set_content);
    Buffer fields = {0};
    append_fourcc(&fields, "IM4R");
    append(&fields, set.bytes, set.length);
    return wrap(0x20, 16, &fields);
}

static Buffer make_container(void) {
    const Buffer payload = make_payload();
    const Buffer manifest = make_manifest(2);
    const Buffer restore = make_restore();
    Buffer fields = {0};
    append_fourcc(&fields, "IMG4");
    append(&fields, payload.bytes, payload.length);
    const Buffer manifest_context = wrap(0xa0, 0, &manifest);
    append(&fields, manifest_context.bytes, manifest_context.length);
    const Buffer restore_context = wrap(0xa0, 1, &restore);
    append(&fields, restore_context.bytes, restore_context.length);
    return wrap(0x20, 16, &fields);
}

static void test_full_decode(void) {
    const Buffer encoded = make_container();
    const DERItem item = {(uint8_t *)encoded.bytes, encoded.length};
    Img4DecodedContainer container;
    assert(DERImg4Decode(&item, &container) == DR_Success);

    Img4DecodedPayload payload;
    assert(DERImg4DecodePayload(&container.payload, &payload) == DR_Success);
    assert(payload.payload.length == 5 && payload.payload.data[4] == 5);
    Img4DecodedPayloadWithProperties extended_payload;
    assert(DERImg4DecodePayloadWithProperties(&container.payload,
                                              &extended_payload) ==
           DR_Success);
    DERItem compression[2];
    assert(DERImg4DecodePayloadCompression(&payload.compression,
                                           compression) == DR_Success);
    uint32_t algorithm;
    uint32_t uncompressed_size;
    assert(DERParseInteger(&compression[0], &algorithm) == DR_Success);
    assert(DERParseInteger(&compression[1], &uncompressed_size) == DR_Success);
    assert(algorithm == 1 && uncompressed_size == 0x1234);

    Img4DecodedPayloadProperties payload_properties;
    assert(DERImg4DecodePayloadProperties(&extended_payload.properties,
                                          &payload_properties) == DR_Success);
    Img4DecodedProperty board;
    DERDecodedInfo set;
    assert(DERDecodeItem(&payload_properties.properties, &set) == DR_Success);
    const DERTag board_tag =
        Img4DecodePrivateTag(IMG4_FOURCC('B', 'O', 'R', 'D'));
    DERItem board_sequence;
    assert(DERImg4DecodeContentFindItemWithTag(&set.content, board_tag,
                                              &board_sequence) == DR_Success);
    Img4DecodedProperty direct_board;
    assert(DERImg4DecodeProperty(&board_sequence, board_tag, &direct_board) ==
           DR_Success);
    assert(direct_board.key.tag == board_tag &&
           direct_board.value.tag == 4);
    Img4DecodedProperty typed_board;
    assert(DERImg4DecodePropertyWithItem(&board_sequence, board_tag, 4,
                                        &typed_board) == DR_Success);
    assert(typed_board.value.item.length == 2);
    DERSequence payload_property_sequence;
    DERDecodeSeqContentInit(&set.content, &payload_property_sequence);
    assert(DERImg4DecodeFindPropertyInSequence(
               &payload_property_sequence, board_tag, 4, &board) == DR_Success);
    assert(board.value.item.length == 2 && board.value.item.data[0] == 0xaa);
    assert(board.value.tag == 4);

    Img4DecodedManifest manifest;
    assert(DERImg4DecodeManifest(&container.manifest, &manifest) == DR_Success);
    static const DERItemSpec manifest_test_specs[] = {
        {offsetof(Img4DecodedManifest, magic), 22, DER_DEC_NO_OPTS},
        {offsetof(Img4DecodedManifest, version), 2, DER_DEC_NO_OPTS},
        {offsetof(Img4DecodedManifest, properties), ASN1_CONSTR_SET,
         DER_DEC_SAVE_DER},
        {offsetof(Img4DecodedManifest, signature), 4, DER_DEC_NO_OPTS},
        {offsetof(Img4DecodedManifest, certificates), ASN1_CONSTR_SEQUENCE,
         DER_DEC_SAVE_DER},
    };
    Img4DecodedManifest common_manifest;
    assert(DERImg4DecodeManifestCommon(
               &container.manifest,
               sizeof(manifest_test_specs) / sizeof(manifest_test_specs[0]),
               manifest_test_specs, (DERItem *)&common_manifest,
               IMG4_MANIFEST_MAGIC) == DR_Success);
    Img4DecodedProperty body;
    DERDecodedInfo manifest_set;
    assert(DERDecodeItem(&manifest.properties, &manifest_set) == DR_Success);
    DERSequence manifest_property_sequence;
    DERDecodeSeqContentInit(&manifest_set.content, &manifest_property_sequence);
    assert(DERImg4DecodeFindPropertyInSequence(
               &manifest_property_sequence,
               Img4DecodePrivateTag(IMG4_MANIFEST_BODY_MAGIC),
               ASN1_CONSTR_SET, &body) == DR_Success);
    assert(body.value.tag == ASN1_CONSTR_SET && body.value.item.length == 0);

    Img4DecodedRestoreInfo restore;
    assert(DERImg4DecodeRestoreInfo(&container.restore_info, &restore) ==
           DR_Success);
}

static void test_strict_failures(void) {
    Buffer encoded = make_container();
    for (size_t length = 0; length < encoded.length; ++length) {
        const DERItem truncated = {encoded.bytes, length};
        Img4DecodedContainer container;
        assert(DERImg4Decode(&truncated, &container) != DR_Success);
    }

    Buffer trailing = encoded;
    trailing.bytes[trailing.length++] = 0;
    const DERItem with_trailing = {trailing.bytes, trailing.length};
    Img4DecodedContainer container;
    assert(DERImg4Decode(&with_trailing, &container) == DR_BufOverflow);

    Buffer bad_magic = encoded;
    uint8_t *magic = find_bytes(&bad_magic, "IMG4", 4);
    assert(magic != NULL);
    magic[0] = 'X';
    const DERItem bad_magic_item = {bad_magic.bytes, bad_magic.length};
    assert(DERImg4Decode(&bad_magic_item, &container) == DR_UnexpectedTag);

    const Buffer future_manifest = make_manifest(3);
    const DERItem future_item = {(uint8_t *)future_manifest.bytes,
                                 future_manifest.length};
    Img4DecodedManifest manifest;
    assert(DERImg4DecodeManifest(&future_item, &manifest) == DR_UnexpectedTag);

    const DERItem absent = {NULL, 0};
    Img4DecodedPayloadProperties absent_payload_properties;
    assert(DERImg4DecodePayloadProperties(&absent,
                                          &absent_payload_properties) ==
           DR_Success);
    assert(absent_payload_properties.magic.data == NULL);
    assert(DERImg4DecodeManifest(&absent, &manifest) == DR_Success);
    Img4DecodedRestoreInfo restore;
    assert(DERImg4DecodeRestoreInfo(NULL, NULL) == DR_Success);
    assert(DERImg4DecodeRestoreInfo(&absent, &restore) == DR_Success);
    assert(restore.magic.data == NULL);
    uint8_t present_byte = 0;
    const DERItem present_empty = {&present_byte, 0};
    assert(DERImg4DecodePayloadProperties(&present_empty,
                                          &absent_payload_properties) !=
           DR_Success);
    assert(DERImg4DecodeManifest(&present_empty, &manifest) != DR_Success);
    assert(DERImg4DecodeRestoreInfo(&present_empty, &restore) != DR_Success);

    Buffer compression_fields = {0};
    append_integer(&compression_fields, 2);
    append_integer(&compression_fields, 1);
    const Buffer compression = wrap(0x20, 16, &compression_fields);
    const DERItem compression_item = {(uint8_t *)compression.bytes,
                                      compression.length};
    DERItem decoded[2];
    assert(DERImg4DecodePayloadCompression(&compression_item, decoded) ==
           DR_ParamErr);
}

int main(void) {
    test_full_decode();
    test_strict_failures();
    puts("IMG4 decode compatibility tests passed");
}
