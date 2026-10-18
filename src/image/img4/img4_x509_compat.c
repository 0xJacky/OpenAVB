/*
 * X.509 certificate parsing on top of the libDER-compatible decoder
 * (apple_libder.h). The item-spec tables follow the ASN.1 schema of
 * RFC 5280; IMG4-specific chain splitting and manifest-extension selection
 * follow the ROM interface.
 */
#include "avp/image/img4/img4_x509_compat.h"

#include <stddef.h>

enum {
    ASN1_BOOLEAN = 1,
    ASN1_INTEGER = 2,
    ASN1_BIT_STRING = 3,
    ASN1_OCTET_STRING = 4,
    ASN1_NULL = 5,
    ASN1_OBJECT_ID = 6,
};

#define ASN1_CONTEXT(number) ((DERTag)((UINT64_C(4) << 61) | (number)))
#define ASN1_CONSTR_CONTEXT(number)                                         \
    ((DERTag)((UINT64_C(5) << 61) | (number)))

typedef struct {
    DERItem algorithm;
    DERItem subject_public_key;
} SubjectPublicKeyInfo;

typedef struct {
    DERItem identifier;
    DERItem critical;
    DERItem value;
} CertificateExtension;

static const DERByte rsa_encryption_oid_bytes[] = {
    0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01,
};

static const DERByte img4_manifest_extension_oid_bytes[] = {
    0x2a, 0x86, 0x48, 0x86, 0xf7, 0x63, 0x64, 0x06, 0x01, 0x0f,
};

static const DERItem rsa_encryption_oid = {
    (DERByte *)rsa_encryption_oid_bytes,
    sizeof(rsa_encryption_oid_bytes),
};

static const DERItem img4_manifest_extension_oid = {
    (DERByte *)img4_manifest_extension_oid_bytes,
    sizeof(img4_manifest_extension_oid_bytes),
};

static const DERItemSpec certificate_specs[] = {
    {offsetof(Img4CertificateFields, tbs_certificate), ASN1_CONSTR_SEQUENCE,
     DER_DEC_SAVE_DER},
    {offsetof(Img4CertificateFields, signature_algorithm),
     ASN1_CONSTR_SEQUENCE, DER_DEC_NO_OPTS},
    {offsetof(Img4CertificateFields, signature_value), ASN1_BIT_STRING,
     DER_DEC_NO_OPTS},
};

static const DERItemSpec tbs_certificate_specs[] = {
    {offsetof(Img4TBSCertificateFields, version), ASN1_CONSTR_CONTEXT(0),
     DER_DEC_OPTIONAL},
    {offsetof(Img4TBSCertificateFields, serial_number), ASN1_INTEGER,
     DER_DEC_NO_OPTS},
    {offsetof(Img4TBSCertificateFields, signature), ASN1_CONSTR_SEQUENCE,
     DER_DEC_NO_OPTS},
    {offsetof(Img4TBSCertificateFields, issuer), ASN1_CONSTR_SEQUENCE,
     DER_DEC_NO_OPTS},
    {offsetof(Img4TBSCertificateFields, validity), ASN1_CONSTR_SEQUENCE,
     DER_DEC_NO_OPTS},
    {offsetof(Img4TBSCertificateFields, subject), ASN1_CONSTR_SEQUENCE,
     DER_DEC_NO_OPTS},
    {offsetof(Img4TBSCertificateFields, subject_public_key_info),
     ASN1_CONSTR_SEQUENCE, DER_DEC_NO_OPTS},
    {offsetof(Img4TBSCertificateFields, issuer_unique_id), ASN1_CONTEXT(1),
     DER_DEC_OPTIONAL},
    {offsetof(Img4TBSCertificateFields, subject_unique_id), ASN1_CONTEXT(2),
     DER_DEC_OPTIONAL},
    {offsetof(Img4TBSCertificateFields, extensions), ASN1_CONSTR_CONTEXT(3),
     DER_DEC_OPTIONAL},
};

static const DERItemSpec subject_public_key_info_specs[] = {
    {offsetof(SubjectPublicKeyInfo, algorithm), ASN1_CONSTR_SEQUENCE,
     DER_DEC_NO_OPTS},
    {offsetof(SubjectPublicKeyInfo, subject_public_key), ASN1_BIT_STRING,
     DER_DEC_NO_OPTS},
};

static const DERItemSpec algorithm_identifier_specs[] = {
    {0, ASN1_OBJECT_ID, DER_DEC_NO_OPTS},
    {sizeof(DERItem), 0,
     DER_DEC_ASN_ANY | DER_DEC_OPTIONAL | DER_DEC_SAVE_DER},
};

static const DERItemSpec extension_specs[] = {
    {offsetof(CertificateExtension, identifier), ASN1_OBJECT_ID,
     DER_DEC_NO_OPTS},
    {offsetof(CertificateExtension, critical), ASN1_BOOLEAN,
     DER_DEC_OPTIONAL},
    {offsetof(CertificateExtension, value), ASN1_OCTET_STRING,
     DER_DEC_NO_OPTS},
};

static int complete_decoded_item(
    const DERItem *encoded,
    DERDecodedInfo *decoded
) {
    if (DERDecodeItem(encoded, decoded) != DR_Success) {
        return -1;
    }
    const DERByte *end = encoded->data + encoded->length;
    return decoded->content.data + decoded->content.length == end ? 0 : -1;
}

static int algorithm_parameters_are_null_or_absent(const DERItem algorithm[2]) {
    if (algorithm[1].length == 0) {
        return 1;
    }
    return algorithm[1].length == 2 && algorithm[1].data[0] == ASN1_NULL &&
           algorithm[1].data[1] == 0;
}

DERReturn IMG4_X509_PUBLIC parse_algorithm_identifier(
    const DERItem *content,
    DERItem algorithm[2]
) {
    if (content == NULL || algorithm == NULL) {
        return DR_ParamErr;
    }
    return DERParseSequenceContent(
        content,
        (DERShort)(sizeof(algorithm_identifier_specs) /
                   sizeof(algorithm_identifier_specs[0])),
        algorithm_identifier_specs,
        algorithm,
        sizeof(DERItem) * 2
    );
}

int IMG4_X509_PUBLIC _crack_chain_with_anchor(
    const DERItem *encoded_chain,
    DERItem *certificate_chain,
    unsigned int certificate_count
) {
    if (encoded_chain == NULL || certificate_chain == NULL ||
        encoded_chain->data == NULL || encoded_chain->length == 0 ||
        certificate_count < 2 || certificate_count > UINT16_MAX) {
        return -1;
    }

    DERByte *cursor = encoded_chain->data;
    DERSize remaining = encoded_chain->length;
    unsigned int index = 1;
    while (remaining != 0) {
        DERItem suffix = {cursor, remaining};
        DERDecodedInfo decoded;
        if (DERDecodeItem(&suffix, &decoded) != DR_Success) {
            return -1;
        }
        const DERByte *item_end = decoded.content.data + decoded.content.length;
        if (item_end < cursor) {
            return -1;
        }
        const DERSize encoded_length = (DERSize)(item_end - cursor);
        if (encoded_length == 0 || encoded_length > 0x1ffff ||
            encoded_length > remaining || index >= certificate_count) {
            return -1;
        }
        certificate_chain[index].data = cursor;
        certificate_chain[index].length = encoded_length;
        cursor += encoded_length;
        remaining -= encoded_length;
        ++index;
    }
    return index == certificate_count ? 0 : -1;
}

int IMG4_X509_PUBLIC parse_extensions(
    const Img4TBSCertificateFields *certificate,
    DERItem *extension_value
) {
    if (certificate == NULL || extension_value == NULL) {
        return -1;
    }
    *extension_value = (DERItem){0};
    if (certificate->extensions.length == 0) {
        return 0;
    }

    DERDecodedInfo outer;
    if (complete_decoded_item(&certificate->extensions, &outer) != 0 ||
        outer.tag != ASN1_CONSTR_SEQUENCE) {
        return -1;
    }

    DERSequence sequence;
    DERDecodeSeqContentInit(&outer.content, &sequence);
    int saw_extension = 0;
    for (;;) {
        DERDecodedInfo decoded;
        const DERReturn next = DERDecodeSeqNext(&sequence, &decoded);
        if (next == DR_EndOfSequence) {
            return saw_extension ? 0 : -1;
        }
        if (next != DR_Success || decoded.tag != ASN1_CONSTR_SEQUENCE) {
            return -1;
        }

        CertificateExtension extension;
        if (DERParseSequenceContent(
                &decoded.content,
                (DERShort)(sizeof(extension_specs) / sizeof(extension_specs[0])),
                extension_specs, &extension, sizeof(extension)) != DR_Success) {
            return -1;
        }
        saw_extension = 1;
        if (!DEROidCompare(&extension.identifier,
                           &img4_manifest_extension_oid)) {
            continue;
        }

        DERDecodedInfo value;
        if (complete_decoded_item(&extension.value, &value) != 0 ||
            value.tag != ASN1_CONSTR_SET) {
            return -1;
        }
        *extension_value = extension.value;
    }
}

int IMG4_X509_PUBLIC parse_chain(
    const DERItem *certificates,
    unsigned int certificate_count,
    Img4CertificateFields *certificate_fields,
    Img4TBSCertificateFields *tbs_fields,
    DERItem *subject_public_keys,
    DERItem *extensions
) {
    if (certificates == NULL || certificate_count == 0 ||
        certificate_fields == NULL || tbs_fields == NULL ||
        subject_public_keys == NULL || extensions == NULL) {
        return -1;
    }

    for (unsigned int index = 0; index < certificate_count; ++index) {
        if (DERParseSequence(
                &certificates[index],
                (DERShort)(sizeof(certificate_specs) /
                           sizeof(certificate_specs[0])),
                certificate_specs, &certificate_fields[index],
                sizeof(certificate_fields[index])) != DR_Success) {
            return -1;
        }
        if (DERParseSequence(
                &certificate_fields[index].tbs_certificate,
                (DERShort)(sizeof(tbs_certificate_specs) /
                           sizeof(tbs_certificate_specs[0])),
                tbs_certificate_specs, &tbs_fields[index],
                sizeof(tbs_fields[index])) != DR_Success) {
            return -1;
        }

        SubjectPublicKeyInfo subject_public_key_info;
        if (DERParseSequenceContent(
                &tbs_fields[index].subject_public_key_info,
                (DERShort)(sizeof(subject_public_key_info_specs) /
                           sizeof(subject_public_key_info_specs[0])),
                subject_public_key_info_specs, &subject_public_key_info,
                sizeof(subject_public_key_info)) != DR_Success) {
            return -1;
        }

        DERItem algorithm[2];
        if (parse_algorithm_identifier(&subject_public_key_info.algorithm,
                                       algorithm) != DR_Success ||
            !DEROidCompare(&algorithm[0], &rsa_encryption_oid) ||
            !algorithm_parameters_are_null_or_absent(algorithm)) {
            return -1;
        }

        DERByte unused_bits;
        if (DERParseBitString(&subject_public_key_info.subject_public_key,
                              &subject_public_keys[index],
                              &unused_bits) != DR_Success ||
            unused_bits != 0) {
            return -1;
        }
        if (parse_extensions(&tbs_fields[index], &extensions[index]) != 0) {
            return -1;
        }
    }
    return 0;
}
