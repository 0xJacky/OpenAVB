#include "avp/image/img4/img4_x509_compat.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint8_t bytes[8192];
    size_t length;
} Buffer;

static const uint8_t rsa_oid[] = {
    0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01,
};

static const uint8_t img4_extension_oid[] = {
    0x2a, 0x86, 0x48, 0x86, 0xf7, 0x63, 0x64, 0x06, 0x01, 0x0f,
};

_Static_assert(sizeof(Img4CertificateFields) == 0x30,
               "certificate fields ABI size");
_Static_assert(sizeof(Img4TBSCertificateFields) == 0xa0,
               "TBS certificate fields ABI size");

static void append(Buffer *buffer, const void *bytes, size_t length) {
    assert(buffer->length + length <= sizeof(buffer->bytes));
    memcpy(buffer->bytes + buffer->length, bytes, length);
    buffer->length += length;
}

static void append_length(Buffer *buffer, size_t length) {
    if (length < 0x80) {
        const uint8_t byte = (uint8_t)length;
        append(buffer, &byte, 1);
        return;
    }
    assert(length <= UINT16_MAX);
    if (length <= UINT8_MAX) {
        const uint8_t encoded[] = {0x81, (uint8_t)length};
        append(buffer, encoded, sizeof(encoded));
        return;
    }
    const uint8_t encoded[] = {
        0x82, (uint8_t)(length >> 8), (uint8_t)length,
    };
    append(buffer, encoded, sizeof(encoded));
}

static void append_tlv(
    Buffer *buffer,
    uint8_t tag,
    const void *content,
    size_t content_length
) {
    append(buffer, &tag, 1);
    append_length(buffer, content_length);
    append(buffer, content, content_length);
}

static Buffer wrap(uint8_t tag, const Buffer *content) {
    Buffer encoded = {0};
    append_tlv(&encoded, tag, content->bytes, content->length);
    return encoded;
}

static void append_integer(Buffer *buffer, uint8_t value) {
    append_tlv(buffer, 0x02, &value, 1);
}

static Buffer make_algorithm(uint8_t parameters_tag) {
    Buffer content = {0};
    append_tlv(&content, 0x06, rsa_oid, sizeof(rsa_oid));
    if (parameters_tag != 0) {
        append_tlv(&content, parameters_tag, NULL, 0);
    }
    return wrap(0x30, &content);
}

static Buffer make_subject_public_key_info(
    uint8_t parameters_tag,
    uint8_t unused_bits
) {
    const Buffer algorithm = make_algorithm(parameters_tag);

    Buffer rsa_fields = {0};
    const uint8_t modulus[] = {0x00, 0x80, 0x01};
    append_tlv(&rsa_fields, 0x02, modulus, sizeof(modulus));
    const uint8_t exponent[] = {0x01, 0x00, 0x00};
    append_tlv(&rsa_fields, 0x02, exponent, sizeof(exponent));
    const Buffer rsa_key = wrap(0x30, &rsa_fields);

    Buffer bit_string = {0};
    append(&bit_string, &unused_bits, 1);
    append(&bit_string, rsa_key.bytes, rsa_key.length);

    Buffer content = {0};
    append(&content, algorithm.bytes, algorithm.length);
    append_tlv(&content, 0x03, bit_string.bytes, bit_string.length);
    return wrap(0x30, &content);
}

static Buffer make_extensions(
    const uint8_t *oid,
    size_t oid_length,
    uint8_t value_tag
) {
    Buffer extension_fields = {0};
    append_tlv(&extension_fields, 0x06, oid, oid_length);
    Buffer extension_value = {0};
    append_tlv(&extension_value, value_tag, NULL, 0);
    append_tlv(
        &extension_fields,
        0x04,
        extension_value.bytes,
        extension_value.length
    );
    const Buffer extension = wrap(0x30, &extension_fields);

    Buffer sequence_content = {0};
    append(&sequence_content, extension.bytes, extension.length);
    const Buffer sequence = wrap(0x30, &sequence_content);
    return wrap(0xa3, &sequence);
}

static Buffer make_certificate(
    uint8_t parameters_tag,
    uint8_t unused_bits,
    const uint8_t *extension_oid,
    size_t extension_oid_length,
    uint8_t extension_value_tag
) {
    const Buffer algorithm = make_algorithm(0x05);
    const Buffer subject_public_key_info =
        make_subject_public_key_info(parameters_tag, unused_bits);

    Buffer version_content = {0};
    append_integer(&version_content, 2);
    const Buffer version = wrap(0xa0, &version_content);
    const Buffer empty_sequence = {{0x30, 0x00}, 2};

    Buffer tbs_content = {0};
    append(&tbs_content, version.bytes, version.length);
    append_integer(&tbs_content, 1);
    append(&tbs_content, algorithm.bytes, algorithm.length);
    append(&tbs_content, empty_sequence.bytes, empty_sequence.length);
    append(&tbs_content, empty_sequence.bytes, empty_sequence.length);
    append(&tbs_content, empty_sequence.bytes, empty_sequence.length);
    append(
        &tbs_content,
        subject_public_key_info.bytes,
        subject_public_key_info.length
    );
    if (extension_oid != NULL) {
        const Buffer extensions = make_extensions(
            extension_oid,
            extension_oid_length,
            extension_value_tag
        );
        append(&tbs_content, extensions.bytes, extensions.length);
    }
    const Buffer tbs = wrap(0x30, &tbs_content);

    const uint8_t signature[] = {0x00, 0xaa};
    Buffer certificate_content = {0};
    append(&certificate_content, tbs.bytes, tbs.length);
    append(&certificate_content, algorithm.bytes, algorithm.length);
    append_tlv(&certificate_content, 0x03, signature, sizeof(signature));
    return wrap(0x30, &certificate_content);
}

static int parse_one(
    const uint8_t *bytes,
    size_t length,
    Img4CertificateFields *certificate_fields,
    Img4TBSCertificateFields *tbs_fields,
    DERItem *public_key,
    DERItem *extension
) {
    const DERItem certificate = {(uint8_t *)bytes, length};
    return parse_chain(
        &certificate,
        1,
        certificate_fields,
        tbs_fields,
        public_key,
        extension
    );
}

static void test_algorithm_identifier(void) {
    const Buffer algorithm = make_algorithm(0x05);
    DERDecodedInfo decoded;
    const DERItem encoded = {(uint8_t *)algorithm.bytes, algorithm.length};
    assert(DERDecodeItem(&encoded, &decoded) == DR_Success);

    DERItem fields[2];
    assert(parse_algorithm_identifier(&decoded.content, fields) == DR_Success);
    assert(fields[0].length == sizeof(rsa_oid));
    assert(memcmp(fields[0].data, rsa_oid, sizeof(rsa_oid)) == 0);
    assert(fields[1].length == 2 && fields[1].data[0] == 0x05 &&
           fields[1].data[1] == 0x00);

    const Buffer absent_parameters = make_algorithm(0);
    const DERItem absent_encoded = {
        (uint8_t *)absent_parameters.bytes, absent_parameters.length,
    };
    assert(DERDecodeItem(&absent_encoded, &decoded) == DR_Success);
    assert(parse_algorithm_identifier(&decoded.content, fields) == DR_Success);
    assert(fields[1].data == NULL && fields[1].length == 0);

    Buffer malformed = {0};
    append(&malformed, decoded.content.data, decoded.content.length);
    const uint8_t trailing_null[] = {0x05, 0x00};
    append(&malformed, trailing_null, sizeof(trailing_null));
    append(&malformed, trailing_null, sizeof(trailing_null));
    const DERItem malformed_content = {malformed.bytes, malformed.length};
    assert(parse_algorithm_identifier(&malformed_content, fields) !=
           DR_Success);
}

static void test_chain_parse(void) {
    const Buffer certificate = make_certificate(
        0x05,
        0,
        img4_extension_oid,
        sizeof(img4_extension_oid),
        0x31
    );
    Img4CertificateFields certificate_fields;
    Img4TBSCertificateFields tbs_fields;
    DERItem public_key;
    DERItem extension;
    assert(parse_one(certificate.bytes, certificate.length,
                     &certificate_fields, &tbs_fields, &public_key,
                     &extension) == 0);
    assert(certificate_fields.tbs_certificate.data[0] == 0x30);
    assert(tbs_fields.serial_number.length == 1 &&
           tbs_fields.serial_number.data[0] == 1);
    assert(public_key.length > 2 && public_key.data[0] == 0x30);
    assert(extension.length == 2 && extension.data[0] == 0x31 &&
           extension.data[1] == 0x00);

    const Buffer absent_extensions = make_certificate(0x05, 0, NULL, 0, 0);
    assert(parse_one(absent_extensions.bytes, absent_extensions.length,
                     &certificate_fields, &tbs_fields, &public_key,
                     &extension) == 0);
    assert(extension.data == NULL && extension.length == 0);

    const uint8_t unknown_oid[] = {0x2a, 0x03, 0x04};
    const Buffer unknown_extension =
        make_certificate(0x05, 0, unknown_oid, sizeof(unknown_oid), 0x31);
    assert(parse_one(unknown_extension.bytes, unknown_extension.length,
                     &certificate_fields, &tbs_fields, &public_key,
                     &extension) == 0);
    assert(extension.data == NULL && extension.length == 0);

    uint8_t empty_extension_sequence[] = {0x30, 0x00};
    const Img4TBSCertificateFields empty_extensions = {
        .extensions = {empty_extension_sequence,
                       sizeof(empty_extension_sequence)},
    };
    assert(parse_extensions(&empty_extensions, &extension) == -1);

    const Buffer invalid_parameters = make_certificate(
        0x04,
        0,
        img4_extension_oid,
        sizeof(img4_extension_oid),
        0x31
    );
    assert(parse_one(invalid_parameters.bytes, invalid_parameters.length,
                     &certificate_fields, &tbs_fields, &public_key,
                     &extension) == -1);

    const Buffer unused_bits = make_certificate(
        0x05,
        1,
        img4_extension_oid,
        sizeof(img4_extension_oid),
        0x31
    );
    assert(parse_one(unused_bits.bytes, unused_bits.length,
                     &certificate_fields, &tbs_fields, &public_key,
                     &extension) == -1);

    const Buffer wrong_extension_value = make_certificate(
        0x05,
        0,
        img4_extension_oid,
        sizeof(img4_extension_oid),
        0x30
    );
    assert(parse_one(wrong_extension_value.bytes,
                     wrong_extension_value.length, &certificate_fields,
                     &tbs_fields, &public_key, &extension) == -1);
}

static void test_chain_truncation(void) {
    const Buffer certificate = make_certificate(
        0x05,
        0,
        img4_extension_oid,
        sizeof(img4_extension_oid),
        0x31
    );
    for (size_t length = 0; length < certificate.length; ++length) {
        Img4CertificateFields certificate_fields;
        Img4TBSCertificateFields tbs_fields;
        DERItem public_key;
        DERItem extension;
        assert(parse_one(certificate.bytes, length, &certificate_fields,
                         &tbs_fields, &public_key, &extension) == -1);
    }
}

static void test_crack_chain(void) {
    const Buffer first = make_certificate(0x05, 0, NULL, 0, 0);
    const Buffer second = make_certificate(0x05, 0, NULL, 0, 0);
    Buffer encoded = {0};
    append(&encoded, first.bytes, first.length);
    append(&encoded, second.bytes, second.length);

    uint8_t anchor_byte = 0xa5;
    DERItem certificates[3] = {{&anchor_byte, 1}, {0}, {0}};
    const DERItem chain = {encoded.bytes, encoded.length};
    assert(_crack_chain_with_anchor(&chain, certificates, 3) == 0);
    assert(certificates[0].data == &anchor_byte &&
           certificates[0].length == 1);
    assert(certificates[1].data == encoded.bytes &&
           certificates[1].length == first.length);
    assert(certificates[2].data == encoded.bytes + first.length &&
           certificates[2].length == second.length);
    assert(_crack_chain_with_anchor(&chain, certificates, 2) == -1);

    static uint8_t oversized[0x20005];
    oversized[0] = 0x30;
    oversized[1] = 0x83;
    oversized[2] = 0x02;
    oversized[3] = 0x00;
    oversized[4] = 0x00;
    const DERItem oversized_chain = {oversized, sizeof(oversized)};
    assert(_crack_chain_with_anchor(&oversized_chain, certificates, 2) == -1);
}

static void test_openssl_certificate(const char *path) {
    FILE *file = fopen(path, "rb");
    assert(file != NULL);
    assert(fseek(file, 0, SEEK_END) == 0);
    const long signed_length = ftell(file);
    assert(signed_length > 0);
    assert(fseek(file, 0, SEEK_SET) == 0);
    const size_t length = (size_t)signed_length;
    uint8_t *bytes = malloc(length);
    assert(bytes != NULL);
    assert(fread(bytes, 1, length, file) == length);
    assert(fclose(file) == 0);

    Img4CertificateFields certificate_fields;
    Img4TBSCertificateFields tbs_fields;
    DERItem public_key;
    DERItem extension;
    assert(parse_one(bytes, length, &certificate_fields, &tbs_fields,
                     &public_key, &extension) == 0);
    assert(public_key.length > 256 && public_key.data[0] == 0x30);
    assert(extension.length == 2 && extension.data[0] == 0x31);
    free(bytes);
}

int main(int argc, char **argv) {
    test_algorithm_identifier();
    test_chain_parse();
    test_chain_truncation();
    test_crack_chain();
    if (argc == 2) {
        test_openssl_certificate(argv[1]);
    }
    puts("img4_x509_compat_tests: ok");
    return 0;
}
