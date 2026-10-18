#include "avp/image/img4/img4_context_compat.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint8_t bytes[2048];
    size_t length;
} Buffer;

static void append(Buffer *buffer, const void *bytes, size_t length) {
    assert(buffer->length + length <= sizeof(buffer->bytes));
    memcpy(buffer->bytes + buffer->length, bytes, length);
    buffer->length += length;
}

static void append_length(Buffer *buffer, size_t length) {
    if (length < 128) {
        const uint8_t value = (uint8_t)length;
        append(buffer, &value, 1);
        return;
    }
    assert(length <= UINT8_MAX);
    const uint8_t value[] = {0x81, (uint8_t)length};
    append(buffer, value, sizeof(value));
}

static void append_tag(
    Buffer *buffer,
    uint8_t tag_class,
    uint32_t tag_number
) {
    if (tag_number < 31) {
        const uint8_t value = tag_class | (uint8_t)tag_number;
        append(buffer, &value, 1);
        return;
    }
    const uint8_t first = tag_class | 0x1f;
    append(buffer, &first, 1);
    uint8_t chunks[5];
    size_t count = 0;
    do {
        chunks[count++] = (uint8_t)(tag_number & 0x7f);
        tag_number >>= 7;
    } while (tag_number != 0);
    while (count != 0) {
        --count;
        uint8_t value = chunks[count];
        if (count != 0) {
            value |= 0x80;
        }
        append(buffer, &value, 1);
    }
}

static Buffer wrap(
    uint8_t tag_class,
    uint32_t tag_number,
    const Buffer *content
) {
    Buffer result = {0};
    append_tag(&result, tag_class, tag_number);
    append_length(&result, content->length);
    append(&result, content->bytes, content->length);
    return result;
}

static void append_value(
    Buffer *buffer,
    uint8_t tag_class,
    uint32_t tag_number,
    const void *data,
    size_t length
) {
    Buffer content = {0};
    append(&content, data, length);
    const Buffer encoded = wrap(tag_class, tag_number, &content);
    append(buffer, encoded.bytes, encoded.length);
}

static void append_fourcc(Buffer *buffer, const char *fourcc) {
    append_value(buffer, 0, 22, fourcc, 4);
}

static void append_integer(Buffer *buffer, uint32_t value) {
    uint8_t storage[5];
    size_t count = 0;
    do {
        storage[4 - count++] = (uint8_t)value;
        value >>= 8;
    } while (value != 0);
    size_t offset = 5 - count;
    if ((storage[offset] & 0x80) != 0) {
        storage[--offset] = 0;
        ++count;
    }
    append_value(buffer, 0, 2, storage + offset, count);
}

static Buffer make_property(
    const char *key,
    uint8_t value_class,
    uint32_t value_tag,
    const void *value,
    size_t value_length
) {
    Buffer fields = {0};
    append_fourcc(&fields, key);
    append_value(&fields, value_class, value_tag, value, value_length);
    const Buffer sequence = wrap(0x20, 16, &fields);
    const uint32_t private_tag = IMG4_FOURCC(key[0], key[1], key[2], key[3]);
    return wrap(0xe0, private_tag, &sequence);
}

static Buffer make_payload(bool include_properties) {
    Buffer compression_fields = {0};
    append_integer(&compression_fields, 1);
    append_integer(&compression_fields, 0x1234);
    const Buffer compression = wrap(0x20, 16, &compression_fields);

    const uint8_t payp_value[] = {0x41, 0x42};
    const Buffer payp_property =
        make_property("CPRO", 0, 4, payp_value, sizeof(payp_value));
    Buffer payp_set_content = {0};
    append(&payp_set_content, payp_property.bytes, payp_property.length);
    const Buffer payp_set = wrap(0x20, 17, &payp_set_content);
    Buffer payp_fields = {0};
    append_fourcc(&payp_fields, "PAYP");
    append(&payp_fields, payp_set.bytes, payp_set.length);
    const Buffer payp = wrap(0x20, 16, &payp_fields);

    Buffer fields = {0};
    append_fourcc(&fields, "IM4P");
    append_fourcc(&fields, "ibot");
    append_fourcc(&fields, "1.0\0");
    const uint8_t payload[] = {1, 2, 3, 4, 5, 6};
    append_value(&fields, 0, 4, payload, sizeof(payload));
    const uint8_t keybag[] = {9, 8, 7};
    append_value(&fields, 0, 4, keybag, sizeof(keybag));
    append(&fields, compression.bytes, compression.length);
    if (include_properties) {
        const Buffer context = wrap(0xa0, 0, &payp);
        append(&fields, context.bytes, context.length);
    }
    return wrap(0x20, 16, &fields);
}

static Buffer make_manifest(uint32_t version, bool matching_body_tag) {
    const uint8_t board[] = {0xaa, 0xbb, 0xcc};
    const Buffer board_property =
        make_property("BORD", 0, 4, board, sizeof(board));
    Buffer manp_value = {0};
    append(&manp_value, board_property.bytes, board_property.length);
    const Buffer manp = make_property(
        "MANP",
        0x20,
        17,
        manp_value.bytes,
        manp_value.length
    );
    Buffer manb_value = {0};
    append(&manb_value, manp.bytes, manp.length);
    uint32_t body_tag = version == 2 ? 16 : 17;
    if (!matching_body_tag) {
        body_tag = body_tag == 16 ? 17 : 16;
    }
    const Buffer manb = make_property(
        "MANB",
        0x20,
        body_tag,
        manb_value.bytes,
        manb_value.length
    );
    Buffer manifest_set_content = {0};
    append(&manifest_set_content, manb.bytes, manb.length);
    const Buffer manifest_set = wrap(0x20, 17, &manifest_set_content);
    Buffer certificates_content = {0};
    const Buffer certificates = wrap(0x20, 16, &certificates_content);

    Buffer fields = {0};
    append_fourcc(&fields, "IM4M");
    append_integer(&fields, version);
    append(&fields, manifest_set.bytes, manifest_set.length);
    const uint8_t signature[] = {0x55, 0xaa};
    append_value(&fields, 0, 4, signature, sizeof(signature));
    append(&fields, certificates.bytes, certificates.length);
    return wrap(0x20, 16, &fields);
}

static Buffer make_restore(void) {
    const uint8_t nonce[] = {1, 3, 3, 7};
    const Buffer property =
        make_property("BNCN", 0, 4, nonce, sizeof(nonce));
    Buffer set_content = {0};
    append(&set_content, property.bytes, property.length);
    const Buffer set = wrap(0x20, 17, &set_content);
    Buffer fields = {0};
    append_fourcc(&fields, "IM4R");
    append(&fields, set.bytes, set.length);
    return wrap(0x20, 16, &fields);
}

static Buffer make_container(
    uint32_t version,
    bool matching_body_tag,
    bool include_payload_properties
) {
    const Buffer payload = make_payload(include_payload_properties);
    const Buffer manifest = make_manifest(version, matching_body_tag);
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

static DERItem property_sequence(const Buffer *property) {
    const DERItem encoded = {(uint8_t *)property->bytes, property->length};
    DERDecodedInfo decoded;
    assert(DERDecodeItem(&encoded, &decoded) == DR_Success);
    return decoded.content;
}

static void test_context_accessors(void) {
    const Buffer encoded = make_container(2, true, true);
    img4_decode_context_t image;
    assert(Img4DecodeInit(encoded.bytes, encoded.length, &image) == DR_Success);

    DERItem payload;
    assert(Img4DecodeGetPayload(&image, &payload) == DR_Success);
    assert(payload.length == 6 && payload.data[5] == 6);
    uint32_t payload_type;
    assert(Img4DecodeGetPayloadType(&image, &payload_type) == DR_Success);
    assert(payload_type == IMG4_FOURCC('i', 'b', 'o', 't'));

    bool exists = false;
    assert(Img4DecodePayloadCompressionInfoExists(&image, &exists) ==
           DR_Success);
    assert(exists);
    uint32_t algorithm;
    uint32_t original_size;
    assert(Img4DecodeGetPayloadCompressionInfo(
               &image, &algorithm, &original_size) == DR_Success);
    assert(algorithm == 1 && original_size == 0x1234);

    assert(Img4DecodeManifestExists(&image, &exists) == DR_Success && exists);
    assert(Img4DecodeSectionExists(&image, IMG4_SECTION_MANIFEST, &exists) ==
               DR_Success &&
           exists);
    assert(Img4DecodeSectionExists(&image, IMG4_SECTION_RESTORE_INFO,
                                   &exists) == DR_Success &&
           exists);

    DERItem object_dictionary;
    DERItem manifest_properties;
    assert(DERImg4DecodeParseManifestProperties(
               &image, &object_dictionary, &manifest_properties) ==
           DR_Success);
    assert(object_dictionary.length > manifest_properties.length);
    Img4DecodedProperty board;
    assert(DERImg4DecodeFindProperty(
               &manifest_properties,
               Img4DecodePrivateTag(IMG4_FOURCC('B', 'O', 'R', 'D')), 4,
               &board) == DR_Success);
    assert(board.value.item.length == 3 && board.value.item.data[2] == 0xcc);

    const uint8_t *data;
    DERSize length;
    assert(Img4DecodeGetDataFromSection(
               &image, IMG4_SECTION_MANIFEST,
               IMG4_FOURCC('B', 'O', 'R', 'D'), &data, &length) == DR_Success);
    assert(length == 3 && data[0] == 0xaa);
    assert(Img4DecodeGetRestoreInfoData(
               &image, IMG4_FOURCC('B', 'N', 'C', 'N'), &data,
               &length) == DR_Success);
    assert(length == 4 && data[3] == 7);

    assert(Img4DecodePayloadPropertiesExists(&image, &exists) == DR_Success &&
           exists);
    DERItem payload_properties;
    assert(Img4DecodeGetPayloadProperties(&image, &payload_properties) ==
           DR_Success);
    Img4DecodedPayloadProperties decoded_payload_properties;
    assert(DERImg4DecodePayloadProperties(&payload_properties,
                                          &decoded_payload_properties) ==
           DR_Success);
}

static void test_property_types(void) {
    const uint8_t integer_value[] = {0x01, 0x23, 0x45, 0x67, 0x89};
    const Buffer integer_property = make_property(
        "CEPO",
        0,
        2,
        integer_value,
        sizeof(integer_value)
    );
    const DERItem integer_sequence = property_sequence(&integer_property);
    uint64_t integer = 0;
    assert(Img4DecodeGetPropertyInteger64(
               &integer_sequence, IMG4_FOURCC('C', 'E', 'P', 'O'),
               &integer) == DR_Success);
    assert(integer == UINT64_C(0x0123456789));

    const uint8_t true_value = 0xff;
    const Buffer boolean_property =
        make_property("EKEY", 0, 1, &true_value, 1);
    const DERItem boolean_sequence = property_sequence(&boolean_property);
    bool boolean = false;
    assert(Img4DecodeGetPropertyBoolean(
               &boolean_sequence, IMG4_FOURCC('E', 'K', 'E', 'Y'),
               &boolean) == DR_Success);
    assert(boolean);

    const uint8_t bytes[] = {5, 4, 3, 2, 1};
    const Buffer data_property =
        make_property("DATA", 0, 4, bytes, sizeof(bytes));
    const DERItem data_sequence = property_sequence(&data_property);
    const uint8_t *decoded_data;
    uint32_t decoded_length;
    assert(Img4DecodeGetPropertyData(
               &data_sequence, IMG4_FOURCC('D', 'A', 'T', 'A'),
               &decoded_data, &decoded_length) == DR_Success);
    assert(decoded_length == 5 && decoded_data[4] == 1);
}

static void test_versions_and_failures(void) {
    const Buffer legacy = make_container(1, true, false);
    img4_decode_context_t image;
    assert(Img4DecodeInit(legacy.bytes, legacy.length, &image) == DR_Success);
    DERItem properties;
    assert(DERImg4DecodeParseManifestProperties(&image, NULL, &properties) ==
           DR_Success);
    bool exists = true;
    assert(Img4DecodePayloadPropertiesExists(&image, &exists) == DR_Success);
    assert(!exists);
    assert(Img4DecodeGetPayloadProperties(&image, &properties) == DR_ParamErr);

    const Buffer mismatched = make_container(2, false, true);
    assert(Img4DecodeInit(mismatched.bytes, mismatched.length, &image) ==
           DR_Success);
    assert(DERImg4DecodeParseManifestProperties(&image, NULL, &properties) ==
           DR_UnexpectedTag);

    const Buffer canonical = make_container(2, true, true);
    for (size_t length = 0; length < canonical.length; ++length) {
        assert(Img4DecodeInit(canonical.bytes, length, &image) != DR_Success);
    }
    assert(Img4DecodeSectionExists(&image, 2, &exists) == DR_ParamErr);
    assert(Img4DecodeInit(NULL, canonical.length, &image) == DR_ParamErr);
}

int main(void) {
    test_context_accessors();
    test_property_types();
    test_versions_and_failures();
    puts("IMG4 context compatibility tests passed");
}

