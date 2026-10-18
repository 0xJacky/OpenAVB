/*
 * Certificate decoding uses the libDER-compatible decoder through
 * img4_x509_compat.c. Chain ordering and signature policy follow the
 * two-certificate libimg4 interface the ROM's callers expect.
 */
#include "avp/image/img4/img4_trust_evaluation.h"

#include <stddef.h>
#include <stdint.h>


static const uint8_t rsa4k_sha384_anchor_bytes[] =
#include "img4_trust_rsa4k_anchor.inc"
;

static const DERItem rsa4k_sha384_anchor = {
    (DERByte *)rsa4k_sha384_anchor_bytes,
    sizeof(rsa4k_sha384_anchor_bytes) - 1u,
};

const DERItem *IMG4_TRUST_EVALUATION_PUBLIC
img4_rsa4k_sha384_trust_anchor(void) {
    return &rsa4k_sha384_anchor;
}

static int items_equal(const DERItem *left, const DERItem *right) {
    if (left->length != right->length) {
        return 0;
    }
    uint8_t difference = 0;
    for (size_t index = 0; index < left->length; ++index) {
        difference |= left->data[index] ^ right->data[index];
    }
    return difference == 0;
}

int IMG4_TRUST_EVALUATION_PUBLIC crack_chain_rsa4k_sha384(
    const DERItem *chain,
    DERItem *certificates,
    unsigned int certificate_count
) {
    if (chain == NULL || certificates == NULL || certificate_count != 2u) {
        return -1;
    }
    certificates[0] = rsa4k_sha384_anchor;
    return _crack_chain_with_anchor(
        chain,
        certificates,
        certificate_count
    ) == 0 ? 0 : -1;
}

int IMG4_TRUST_EVALUATION_PUBLIC verify_chain_img4_v2_with_crack_callback(
    const uint8_t *chain,
    unsigned int chain_length,
    const uint8_t **leaf_public_key,
    DERSize *leaf_public_key_length,
    const uint8_t **extension,
    DERSize *extension_length,
    const Img4DecodeImplementation *implementation,
    Img4CrackChainCallback crack_callback
) {
    if (chain == NULL || chain_length == 0 || leaf_public_key == NULL ||
        leaf_public_key_length == NULL || implementation == NULL ||
        crack_callback == NULL) {
        return -1;
    }

    DERItem encoded_chain = {(DERByte *)chain, chain_length};
    DERItem certificates[2] = {{0}};
    if (crack_callback(&encoded_chain, certificates, 2) != 0) {
        return -1;
    }

    Img4CertificateFields certificate_fields[2];
    Img4TBSCertificateFields tbs_fields[2];
    DERItem public_keys[2];
    DERItem extensions[2];
    if (parse_chain(certificates, 2, certificate_fields, tbs_fields,
                    public_keys, extensions) != 0 ||
        !items_equal(&tbs_fields[0].subject, &tbs_fields[1].issuer) ||
        verify_chain_signatures(&public_keys[0], &certificate_fields[1],
                                implementation) != 0) {
        return -1;
    }

    *leaf_public_key = public_keys[1].data;
    *leaf_public_key_length = public_keys[1].length;
    if (extension != NULL && extension_length != NULL) {
        *extension = extensions[1].data;
        *extension_length = extensions[1].length;
    }
    return 0;
}

int IMG4_TRUST_EVALUATION_PUBLIC verify_chain_img4_v2(
    const uint8_t *chain,
    unsigned int chain_length,
    const uint8_t **leaf_public_key,
    DERSize *leaf_public_key_length,
    const uint8_t **extension,
    DERSize *extension_length,
    const Img4DecodeImplementation *implementation
) {
    return verify_chain_img4_v2_with_crack_callback(
        chain,
        chain_length,
        leaf_public_key,
        leaf_public_key_length,
        extension,
        extension_length,
        implementation,
        crack_chain_rsa4k_sha384
    );
}
