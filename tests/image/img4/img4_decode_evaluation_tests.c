#include "avp/crypto/support/corecrypto_mbedtls.h"
#include "avp/image/img4/img4_decode_evaluation.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>


typedef struct {
    uint8_t bytes[1024];
    size_t length;
} Buffer;

static uint64_t active_guard = UINT64_C(0x1122334455667788);
static unsigned callback_count;
static bool callback_object;

uint64_t runtime_stack_guard(void) {
    return active_guard;
}

static void append(Buffer *buffer, const void *bytes, size_t length) {
    assert(buffer->length + length <= sizeof(buffer->bytes));
    memcpy(buffer->bytes + buffer->length, bytes, length);
    buffer->length += length;
}

static void append_length(Buffer *buffer, size_t length) {
    assert(length < 128);
    const uint8_t byte = (uint8_t)length;
    append(buffer, &byte, 1);
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
    while (count != 0) {
        --count;
        uint8_t byte = encoded[count];
        if (count != 0) {
            byte |= 0x80;
        }
        append(buffer, &byte, 1);
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

static Buffer make_property(
    const char key[4],
    uint8_t value_class,
    uint32_t value_tag,
    const void *value,
    size_t value_length
) {
    Buffer fields = {0};
    const uint8_t ia5 = 22;
    append(&fields, &ia5, 1);
    append_length(&fields, 4);
    append(&fields, key, 4);
    append_tag(&fields, value_class, value_tag);
    append_length(&fields, value_length);
    append(&fields, value, value_length);
    const Buffer sequence = wrap(0x20, 16, &fields);
    return wrap(
        0xe0,
        IMG4_FOURCC(key[0], key[1], key[2], key[3]),
        &sequence
    );
}

static DERReturn record_property(
    DERTag tag,
    const Img4PropertyValue *value,
    bool object_property,
    void *context
) {
    assert(context == &callback_count);
    assert(tag == Img4DecodePrivateTag(IMG4_FOURCC('B', 'O', 'R', 'D')));
    assert(value->tag == 4 && value->length == 2);
    assert(value->data[0] == 0x12 && value->data[1] == 0x34);
    ++callback_count;
    callback_object = object_property;
    return DR_Success;
}

static int mock_digest(
    const void *data,
    size_t data_length,
    void *digest,
    size_t digest_length,
    const Img4DecodeImplementation *implementation
) {
    (void)implementation;
    assert(data_length == 3 && memcmp(data, "abc", 3) == 0);
    memset(digest, 0x5a, digest_length);
    return DR_Success;
}

static Img4DecodeImplementation implementation_with(
    Img4ComputeDigestCallback callback
) {
    static const Img4DigestDescriptor descriptor = {.output_size = 48};
    static const Img4DigestDescriptor *slot = &descriptor;
    static const Img4DigestConfiguration configuration = {
        .descriptor = &slot,
    };
    return (Img4DecodeImplementation){
        .compute_digest = callback,
        .digest_configuration = &configuration,
    };
}

static void test_hashes(void) {
    static const uint8_t sha384_abc[48] = {
        0xcb, 0x00, 0x75, 0x3f, 0x45, 0xa3, 0x5e, 0x8b,
        0xb5, 0xa0, 0x3d, 0x69, 0x9a, 0xc6, 0x50, 0x07,
        0x27, 0x2c, 0x32, 0xab, 0x0e, 0xde, 0xd1, 0x63,
        0x1a, 0x8b, 0x60, 0x5a, 0x43, 0xff, 0x5b, 0xed,
        0x80, 0x86, 0x07, 0x2b, 0xa1, 0xe7, 0xcc, 0x23,
        0x58, 0xba, 0xec, 0xa1, 0x34, 0xc8, 0x25, 0xa7,
    };
    Img4DecodeImplementation implementation =
        implementation_with(Img4DecodeComputeDigest);
    uint8_t digest[48];
    assert(Img4DecodeComputeDigest("abc", 3, digest, sizeof(digest),
                                   &implementation) == DR_Success);
    assert(memcmp(digest, sha384_abc, sizeof(digest)) == 0);

    implementation.compute_digest = mock_digest;
    img4_decode_context_t image = {0};
    image.encoded_payload = (DERItem){(DERByte *)"abc", 3};
    image.payload.payload = image.encoded_payload;
    image.encoded_manifest = image.encoded_payload;
    assert(Img4DecodeCopyPayloadHashWithCallback(
               &image, digest, sizeof(digest), &implementation) == DR_Success);
    for (size_t index = 0; index < sizeof(digest); ++index) {
        assert(digest[index] == 0x5a);
        image.payload_hash[index] = 0xa5;
        image.manifest.reserved[0x10 + index] = 0x3c;
    }
    image.state[0] = 1;
    image.state[1] = 1;
    assert(Img4DecodeCopyPayloadHashWithCallback(
               &image, digest, sizeof(digest), &implementation) == DR_Success);
    assert(digest[0] == 0xa5 && digest[47] == 0xa5);
    assert(Img4DecodeCopyManifestHashWithCallback(
               &image, digest, sizeof(digest), &implementation) == DR_Success);
    assert(digest[0] == 0x3c && digest[47] == 0x3c);
    assert(Img4DecodeCopyManifestHashWithCallback(
               &image, digest, 47, &implementation) == DR_BufOverflow);
}

static void test_dictionary_and_certificate_constraints(void) {
    const uint8_t value[] = {0x12, 0x34};
    const Buffer leaf = make_property("BORD", 0, 4, value, sizeof(value));
    DERItem dictionary = {(DERByte *)leaf.bytes, leaf.length};
    callback_count = 0;
    assert(Img4DecodeEvaluateDictionaryProperties(
               &dictionary, 1, record_property, &callback_count) == DR_Success);
    assert(callback_count == 1 && callback_object);
    for (size_t prefix = 1; prefix < leaf.length; ++prefix) {
        dictionary.length = prefix;
        assert(Img4DecodeEvaluateDictionaryProperties(
                   &dictionary, 0, record_property, &callback_count) !=
               DR_Success);
    }
    dictionary.length = leaf.length;

    const Buffer required_manifest = make_property(
        "MANP",
        0x20,
        17,
        leaf.bytes,
        leaf.length
    );
    Buffer extension_content = {0};
    append(
        &extension_content,
        required_manifest.bytes,
        required_manifest.length
    );
    const Buffer extension = wrap(0x20, 17, &extension_content);
    img4_decode_context_t image = {0};
    *(DERItem *)&image.reserved_28[0x10] = dictionary;
    *(DERItem *)&image.manifest.reserved[0] =
        (DERItem){(DERByte *)extension.bytes, extension.length};
    assert(Img4DecodeEvaluateCertificateProperties(&image) == DR_Success);

    const Buffer optional = make_property("MISS", 0xa0, 0, NULL, 0);
    const Buffer excluded = make_property("NOPE", 0xa0, 1, NULL, 0);
    Buffer policy_content = {0};
    append(&policy_content, optional.bytes, optional.length);
    append(&policy_content, excluded.bytes, excluded.length);
    const Buffer policy_manifest = make_property(
        "MANP",
        0x20,
        17,
        policy_content.bytes,
        policy_content.length
    );
    const uint8_t extension_value = 7;
    const Buffer manifest_extension = make_property(
        "manx",
        0,
        4,
        &extension_value,
        sizeof(extension_value)
    );
    Buffer policy_extension_content = {0};
    append(
        &policy_extension_content,
        policy_manifest.bytes,
        policy_manifest.length
    );
    append(
        &policy_extension_content,
        manifest_extension.bytes,
        manifest_extension.length
    );
    const Buffer policy_extension = wrap(
        0x20,
        17,
        &policy_extension_content
    );
    *(DERItem *)&image.manifest.reserved[0] =
        (DERItem){(DERByte *)policy_extension.bytes,
                  policy_extension.length};
    DERItem returned_extension = {0};
    assert(Img4DecodeEvaluateCertificatePropertiesInternal(
               &image, &returned_extension) == DR_Success);
    assert(returned_extension.data != NULL && returned_extension.length != 0);

    uint8_t corrupted[1024];
    memcpy(corrupted, extension.bytes, extension.length);
    corrupted[extension.length - 1] ^= 1;
    *(DERItem *)&image.manifest.reserved[0] =
        (DERItem){corrupted, extension.length};
    assert(Img4DecodeEvaluateCertificateProperties(&image) == (DERReturn)-1);
    for (size_t prefix = 1; prefix < extension.length; ++prefix) {
        *(DERItem *)&image.manifest.reserved[0] =
            (DERItem){(DERByte *)extension.bytes, prefix};
        assert(Img4DecodeEvaluateCertificateProperties(&image) != DR_Success);
    }
}

static void test_outliners(void) {
    DERItem item = {(DERByte *)"x", 1};
    assert(img4_decode_stack_guard_load_to_x8() == active_guard);
    assert(img4_decode_stack_guard_compare_saved_x8(active_guard));
    assert(img4_decode_stack_guard_compare_saved_x8_clone(active_guard));
    const Img4StackGuardPrologueState state =
        img4_decode_stack_guard_preserve_x0_load_to_x9(&item);
    assert(state.item == &item && state.guard == active_guard);
    assert(img4_decode_property_outlined_bind_output_and_save_stack_guard(
               &item) == &item);
    uint8_t scratch[48];
    memset(scratch, 0xa5, sizeof(scratch));
    img4_decode_property_outlined_zero_decode_scratch(scratch);
    for (size_t index = 0; index < sizeof(scratch); ++index) {
        assert(scratch[index] == 0);
    }
}

int main(void) {
    test_hashes();
    test_dictionary_and_certificate_constraints();
    test_outliners();
    return 0;
}
