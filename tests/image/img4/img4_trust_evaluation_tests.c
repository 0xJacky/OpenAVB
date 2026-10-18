#include "avp/image/img4/img4_trust_evaluation.h"

#include <assert.h>
#include <openssl/evp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


typedef struct {
    uint8_t bytes[4096];
    size_t length;
} Buffer;

static DERItem oracle_root;
static unsigned property_events;
static unsigned crypto_events;

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
    return wrap(
        0xe0,
        IMG4_FOURCC(key[0], key[1], key[2], key[3]),
        &sequence
    );
}

static Buffer make_container(void) {
    Buffer payload_fields = {0};
    append_fourcc(&payload_fields, "IM4P");
    append_fourcc(&payload_fields, "ibot");
    append_fourcc(&payload_fields, "1.0\0");
    const uint8_t payload_bytes[] = {1, 2, 3, 4};
    append_value(
        &payload_fields,
        0,
        4,
        payload_bytes,
        sizeof(payload_bytes)
    );
    const Buffer payload = wrap(0x20, 16, &payload_fields);

    const uint8_t manifest_value[] = {0xaa};
    const Buffer manifest_leaf = make_property(
        "MANX",
        0,
        4,
        manifest_value,
        sizeof(manifest_value)
    );
    Buffer manifest_set_content = {0};
    append(
        &manifest_set_content,
        manifest_leaf.bytes,
        manifest_leaf.length
    );
    const Buffer manifest_properties = make_property(
        "MANP",
        0x20,
        17,
        manifest_set_content.bytes,
        manifest_set_content.length
    );

    const uint8_t object_value[] = {0xbb, 0xcc};
    const Buffer object_leaf = make_property(
        "OBJP",
        0,
        4,
        object_value,
        sizeof(object_value)
    );
    Buffer object_set_content = {0};
    append(&object_set_content, object_leaf.bytes, object_leaf.length);
    const Buffer object_properties = make_property(
        "ibot",
        0x20,
        17,
        object_set_content.bytes,
        object_set_content.length
    );

    Buffer body_content = {0};
    append(
        &body_content,
        manifest_properties.bytes,
        manifest_properties.length
    );
    append(
        &body_content,
        object_properties.bytes,
        object_properties.length
    );
    const Buffer body = make_property(
        "MANB",
        0x20,
        16,
        body_content.bytes,
        body_content.length
    );
    Buffer properties_content = {0};
    append(&properties_content, body.bytes, body.length);
    const Buffer properties = wrap(0x20, 17, &properties_content);

    Buffer manifest_fields = {0};
    append_fourcc(&manifest_fields, "IM4M");
    const uint8_t version = 2;
    append_value(&manifest_fields, 0, 2, &version, 1);
    append(&manifest_fields, properties.bytes, properties.length);
    const uint8_t signature[] = {0x55, 0xaa};
    append_value(&manifest_fields, 0, 4, signature, sizeof(signature));
    const Buffer empty = {0};
    const Buffer certificates = wrap(0x20, 16, &empty);
    append(&manifest_fields, certificates.bytes, certificates.length);
    const Buffer manifest = wrap(0x20, 16, &manifest_fields);

    Buffer container_fields = {0};
    append_fourcc(&container_fields, "IMG4");
    append(&container_fields, payload.bytes, payload.length);
    const Buffer manifest_context = wrap(0xa0, 0, &manifest);
    append(
        &container_fields,
        manifest_context.bytes,
        manifest_context.length
    );
    return wrap(0x20, 16, &container_fields);
}

static uint8_t *read_file(const char *path, size_t *length) {
    FILE *file = fopen(path, "rb");
    assert(file != NULL);
    assert(fseek(file, 0, SEEK_END) == 0);
    const long size = ftell(file);
    assert(size > 0 && fseek(file, 0, SEEK_SET) == 0);
    uint8_t *bytes = malloc((size_t)size);
    assert(bytes != NULL);
    assert(fread(bytes, 1, (size_t)size, file) == (size_t)size);
    assert(fclose(file) == 0);
    *length = (size_t)size;
    return bytes;
}

static int sha384_digest(
    const void *data,
    size_t data_length,
    void *digest,
    size_t digest_length,
    const Img4DecodeImplementation *implementation
) {
    (void)implementation;
    unsigned int actual_length = 0;
    assert(digest_length == 48);
    return EVP_Digest(data, data_length, digest, &actual_length,
                      EVP_sha384(), NULL) == 1 && actual_length == 48 ? 0 : -1;
}

static int rsa_verify(
    const uint8_t *public_key,
    unsigned int public_key_length,
    const uint8_t *signature,
    unsigned int signature_length,
    const uint8_t *digest,
    unsigned int digest_length,
    const Img4DecodeImplementation *implementation,
    void *context
) {
    (void)context;
    return verify_signature_rsa(
        public_key,
        public_key_length,
        signature,
        signature_length,
        digest,
        digest_length,
        implementation
    );
}

static int oracle_crack(
    const DERItem *chain,
    DERItem *certificates,
    unsigned int certificate_count
) {
    assert(certificate_count == 2);
    certificates[0] = oracle_root;
    return _crack_chain_with_anchor(chain, certificates, certificate_count);
}

static int unused_chain(
    const uint8_t *chain,
    unsigned int chain_length,
    const uint8_t **key,
    DERSize *key_length,
    const uint8_t **extension,
    DERSize *extension_length,
    const Img4DecodeImplementation *implementation,
    void *context
) {
    (void)chain;
    (void)chain_length;
    (void)extension;
    (void)extension_length;
    (void)implementation;
    (void)context;
    ++crypto_events;
    *key = (const uint8_t *)"key";
    *key_length = 3;
    return 0;
}

static int unused_signature(const uint8_t *public_key,
                            unsigned int public_key_length,
                            const uint8_t *signature,
                            unsigned int signature_length,
                            const uint8_t *digest,
                            unsigned int digest_length,
                            const Img4DecodeImplementation *implementation,
                            void *context) {
    (void)public_key;
    (void)public_key_length;
    (void)signature;
    (void)signature_length;
    (void)digest;
    (void)digest_length;
    (void)implementation;
    (void)context;
    ++crypto_events;
    return 0;
}

static int certificate_properties(
    img4_decode_context_t *image,
    void *context
) {
    (void)image;
    (void)context;
    ++crypto_events;
    return 0;
}

static const uint8_t *trust_computed_hash(
    const img4_decode_context_t *image,
    void *context
) {
    (void)context;
    return &image->manifest.reserved[0x10];
}

static DERReturn evaluate_property(
    DERTag property_tag,
    const Img4PropertyValue *value,
    bool object_property,
    void *context
) {
    (void)context;
    assert(value->tag == 4);
    if (property_events == 0) {
        assert(property_tag == Img4DecodePrivateTag(
                                   IMG4_FOURCC('M', 'A', 'N', 'X')));
        assert(!object_property && value->length == 1 &&
               value->data[0] == 0xaa);
    } else {
        assert(property_tag == Img4DecodePrivateTag(
                                   IMG4_FOURCC('O', 'B', 'J', 'P')));
        assert(object_property && value->length == 2 &&
               value->data[1] == 0xcc);
    }
    ++property_events;
    return DR_Success;
}

static Img4DecodeImplementation make_implementation(
    Img4ComputeDigestCallback digest,
    Img4VerifyChainCallback chain,
    Img4VerifySignatureCallback signature
) {
    static const uint8_t digest_oid[] = {
        0x06, 0x09, 0x60, 0x86, 0x48, 0x01,
        0x65, 0x03, 0x04, 0x02, 0x02,
    };
    static const uint8_t signature_oid_bytes[] = {
        0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0c,
    };
    static const DERItem signature_oid = {
        (DERByte *)signature_oid_bytes, sizeof(signature_oid_bytes),
    };
    static const Img4DigestDescriptor descriptor = {
        .output_size = 48,
        .oid = digest_oid,
    };
    static const Img4DigestDescriptor *descriptor_slot = &descriptor;
    static const Img4DigestConfiguration configuration = {
        .descriptor = &descriptor_slot,
    };
    return (Img4DecodeImplementation){
        .compute_digest = digest,
        .reserved_08 = (const void *)chain,
        .verify_signature = signature,
        .reserved_18 = (const void *)certificate_properties,
        .digest_configuration = &configuration,
        .signature_algorithm_oid = &signature_oid,
    };
}

static void test_trust_orchestration(void) {
    const Buffer encoded = make_container();
    img4_decode_context_t image;
    assert(Img4DecodeInit(encoded.bytes, encoded.length, &image) == DR_Success);
    Img4DecodeImplementation implementation = make_implementation(
        sha384_digest,
        unused_chain,
        unused_signature
    );
    const Img4PropertyCallbacks callbacks = {
        .evaluate_property = evaluate_property,
        .get_trusted_manifest_hash = trust_computed_hash,
    };

    property_events = 0;
    crypto_events = 0;
    assert(Img4DecodePerformTrustEvaluationWithCallbacks(
               IMG4_FOURCC('i', 'b', 'o', 't'), &image, &callbacks,
               &implementation, NULL) == DR_Success);
    assert(property_events == 2 && crypto_events == 0);
    assert(image.state[0] == 1 && image.state[1] == 1);

    assert(Img4DecodeInit(encoded.bytes, encoded.length, &image) == DR_Success);
    property_events = 0;
    assert(_Img4DecodePerformTrustEvaluationWithCallbacksInternal(
               IMG4_FOURCC('i', 'b', 'o', 't'), &image, &callbacks,
               &implementation, true, NULL) == DR_Success);
    assert(property_events == 1);

    Img4DigestDescriptor oversized = {.output_size = 49};
    const Img4DigestDescriptor *oversized_slot = &oversized;
    Img4DigestConfiguration oversized_configuration = {
        .descriptor = &oversized_slot,
    };
    implementation.digest_configuration = &oversized_configuration;
    assert(Img4DecodePerformTrustEvaluationWithCallbacks(
               0, &image, &callbacks, &implementation, NULL) ==
           DR_BufOverflow);
}

static void test_certificate_oracle(
    const char *root_path,
    const char *leaf_path
) {
    size_t root_length;
    size_t leaf_length;
    uint8_t *root = read_file(root_path, &root_length);
    uint8_t *leaf = read_file(leaf_path, &leaf_length);
    oracle_root = (DERItem){root, root_length};
    Img4DecodeImplementation implementation = make_implementation(
        sha384_digest,
        NULL,
        rsa_verify
    );
    const uint8_t *key;
    const uint8_t *extension;
    DERSize key_length;
    DERSize extension_length;
    assert(verify_chain_img4_v2_with_crack_callback(
               leaf, (unsigned int)leaf_length, &key, &key_length,
               &extension, &extension_length, &implementation,
               oracle_crack) == 0);
    assert(key != NULL && key_length > 512);
    assert(extension != NULL && extension_length == 2 &&
           extension[0] == 0x31 && extension[1] == 0x00);

    leaf[leaf_length - 1] ^= 1;
    assert(verify_chain_img4_v2_with_crack_callback(
               leaf, (unsigned int)leaf_length, &key, &key_length,
               &extension, &extension_length, &implementation,
               oracle_crack) == -1);
    leaf[leaf_length - 1] ^= 1;
    for (size_t prefix = 0; prefix < leaf_length; ++prefix) {
        assert(verify_chain_img4_v2_with_crack_callback(
                   leaf, (unsigned int)prefix, &key, &key_length,
                   &extension, &extension_length, &implementation,
                   oracle_crack) == -1);
    }
    free(leaf);
    free(root);
}

static void test_builtin_anchor(void) {
    static const uint8_t expected_sha256[32] = {
        0xf4, 0xf2, 0xcb, 0xd3, 0xfa, 0x65, 0x16, 0xa2,
        0xb7, 0x0f, 0xb1, 0xd7, 0xe8, 0xc8, 0x8e, 0x2a,
        0x52, 0x6d, 0x61, 0x2d, 0xeb, 0x14, 0xe3, 0xfd,
        0x65, 0x64, 0x75, 0xa4, 0xfc, 0xae, 0xce, 0x2b,
    };
    const DERItem *anchor = img4_rsa4k_sha384_trust_anchor();
    assert(anchor->data != NULL && anchor->length == 0x572);
    uint8_t digest[32];
    unsigned int digest_length = 0;
    assert(EVP_Digest(anchor->data, anchor->length, digest, &digest_length,
                      EVP_sha256(), NULL) == 1);
    assert(digest_length == sizeof(digest) &&
           memcmp(digest, expected_sha256, sizeof(digest)) == 0);
    DERDecodedInfo decoded;
    assert(DERDecodeItem(anchor, &decoded) == DR_Success);
    assert(decoded.tag == ASN1_CONSTR_SEQUENCE);
    DERItem certificates[2];
    assert(crack_chain_rsa4k_sha384(NULL, certificates, 2) == -1);
}

int main(int argc, char **argv) {
    test_builtin_anchor();
    test_trust_orchestration();
    if (argc != 1) {
        assert(argc == 3);
        test_certificate_oracle(argv[1], argv[2]);
    }
    return 0;
}
