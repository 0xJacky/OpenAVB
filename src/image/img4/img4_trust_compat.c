/*
 * RSA key and signature handling on top of the libDER-compatible decoder
 * (apple_libder.h); the RSAPublicKey schema is the one RFC 8017 dictates.
 * RSA verification is delegated to the public-key compatibility owner,
 * whose encoding contract follows RFC 8017.
 */
#include "avp/image/img4/img4_trust_compat.h"

#include <stddef.h>
#include <string.h>


enum {
    ASN1_INTEGER = 2,
    ASN1_NULL = 5,
    IMG4_RSA3_BITS = 3072,
    IMG4_RSA4_BITS = 4096,
    IMG4_SHA384_SIZE = 48,
};

typedef struct {
    DERItem modulus;
    DERItem exponent;
} Img4RSAPublicKeyFields;

static const DERItemSpec rsa_public_key_specs[] = {
    {offsetof(Img4RSAPublicKeyFields, modulus), ASN1_INTEGER,
     DER_DEC_NO_OPTS},
    {offsetof(Img4RSAPublicKeyFields, exponent), ASN1_INTEGER,
     DER_DEC_NO_OPTS},
};

static int parameters_are_null_or_absent(const DERItem *parameters) {
    return parameters->length == 0 ||
           (parameters->length == 2 && parameters->data[0] == ASN1_NULL &&
            parameters->data[1] == 0);
}

static int bytes_equal(
    const uint8_t *left,
    const uint8_t *right,
    size_t count
) {
    uint8_t difference = 0;
    for (size_t index = 0; index < count; ++index) {
        difference |= left[index] ^ right[index];
    }
    return difference == 0;
}

static const Img4DigestDescriptor *digest_descriptor(
    const Img4DecodeImplementation *implementation) {
    if (implementation == NULL || implementation->digest_configuration == NULL ||
        implementation->digest_configuration->descriptor == NULL) {
        return NULL;
    }
    return *implementation->digest_configuration->descriptor;
}

int IMG4_TRUST_PUBLIC img4_make_rsa_public_key_checked(
    AVPCCRSAPublicKey *key,
    const void *key_upper_bound,
    size_t modulus_length,
    const uint8_t *modulus,
    size_t exponent_length,
    const uint8_t *exponent
) {
    if (key == NULL || key_upper_bound == NULL || modulus == NULL ||
        exponent == NULL || modulus_length == 0 || exponent_length == 0) {
        return -1;
    }
    const uintptr_t key_address = (uintptr_t)key;
    const uintptr_t upper = (uintptr_t)key_upper_bound;
    if (upper < key_address || upper - key_address < sizeof(*key) + 8u) {
        return -1;
    }
    while (modulus_length != 0 && *modulus == 0) {
        ++modulus;
        --modulus_length;
    }
    if (modulus_length == 0) {
        return -1;
    }
    const size_t required_n = (modulus_length + 7u) / 8u;
    if (required_n == 0 || required_n > key->n ||
        required_n > AVP_CCRSA_MAX_N) {
        return -1;
    }
    const size_t required_units = AVP_CCRSA_PUBLIC_CONTEXT_UNITS(required_n);
    if (required_units > (upper - key_address) / sizeof(uint64_t)) {
        return -1;
    }
    key->n = required_n;
    return ccrsa_make_pub(
        key,
        exponent_length,
        exponent,
        modulus_length,
        modulus
    );
}

int IMG4_TRUST_PUBLIC verify_pkcs1_sig(
    const DERItem *public_key,
    const uint8_t *digest_oid,
    const DERItem *digest,
    const DERItem *signature,
    uint64_t rsa_key_bits
) {
    if (public_key == NULL || digest_oid == NULL || digest == NULL ||
        signature == NULL || public_key->data == NULL ||
        digest->data == NULL || signature->data == NULL ||
        (rsa_key_bits != IMG4_RSA3_BITS && rsa_key_bits != IMG4_RSA4_BITS)) {
        return -1;
    }

    Img4RSAPublicKeyFields fields;
    if (DERParseSequence(
            public_key,
            (DERShort)(sizeof(rsa_public_key_specs) /
                       sizeof(rsa_public_key_specs[0])),
            rsa_public_key_specs, &fields, sizeof(fields)) != DR_Success) {
        return -1;
    }

    uint64_t context[AVP_CCRSA_PUBLIC_CONTEXT_UNITS(AVP_CCRSA_MAX_N)] = {0};
    AVPCCRSAPublicKey *key = (AVPCCRSAPublicKey *)context;
    key->n = (size_t)(rsa_key_bits / 64u);
    if (img4_make_rsa_public_key_checked(
            key, context + AVP_CCRSA_PUBLIC_CONTEXT_UNITS(AVP_CCRSA_MAX_N),
            fields.modulus.length, fields.modulus.data,
            fields.exponent.length, fields.exponent.data) != AVP_CCERR_OK) {
        return -1;
    }
    uint8_t canary[16];
    if (ccrsa_verify_pkcs1v15_digest(
            key, digest_oid, digest->length, digest->data, signature->length,
            signature->data, canary) != AVP_CCERR_OK ||
        !bytes_equal(canary, avp_ccrsa_fault_canary, sizeof(canary))) {
        return -1;
    }
    return 0;
}

int IMG4_TRUST_PUBLIC verify_signature_rsa(
    const uint8_t *public_key,
    unsigned int public_key_length,
    const uint8_t *signature,
    unsigned int signature_length,
    const uint8_t *digest,
    unsigned int digest_length,
    const Img4DecodeImplementation *implementation
) {
    const Img4DigestDescriptor *descriptor = digest_descriptor(implementation);
    if (public_key == NULL || public_key_length == 0 || signature == NULL ||
        signature_length == 0 || digest == NULL || digest_length == 0 ||
        descriptor == NULL || descriptor->output_size != digest_length ||
        descriptor->oid == NULL) {
        return -1;
    }
    DERItem public_key_item = {(DERByte *)public_key, public_key_length};
    DERItem signature_item = {(DERByte *)signature, signature_length};
    DERItem digest_item = {(DERByte *)digest, digest_length};
    return verify_pkcs1_sig(
        &public_key_item,
        descriptor->oid,
        &digest_item,
        &signature_item,
        IMG4_RSA4_BITS
    );
}

int IMG4_TRUST_PUBLIC verify_chain_signatures(
    const DERItem *issuer_public_key,
    const Img4CertificateFields *leaf_certificate,
    const Img4DecodeImplementation *implementation
) {
    const Img4DigestDescriptor *descriptor = digest_descriptor(implementation);
    if (issuer_public_key == NULL || leaf_certificate == NULL ||
        implementation == NULL || implementation->compute_digest == NULL ||
        implementation->verify_signature == NULL ||
        implementation->signature_algorithm_oid == NULL ||
        descriptor == NULL || descriptor->output_size == 0 ||
        descriptor->output_size > IMG4_SHA384_SIZE) {
        return -1;
    }

    DERItem algorithm[2];
    if (parse_algorithm_identifier(&leaf_certificate->signature_algorithm,
                                   algorithm) != DR_Success ||
        !DEROidCompare(&algorithm[0], implementation->signature_algorithm_oid) ||
        !parameters_are_null_or_absent(&algorithm[1])) {
        return -1;
    }

    uint8_t digest[IMG4_SHA384_SIZE] = {0};
    if (implementation->compute_digest(
            leaf_certificate->tbs_certificate.data,
            leaf_certificate->tbs_certificate.length, digest,
            descriptor->output_size, implementation) != 0) {
        return -1;
    }

    DERItem signature;
    DERByte unused_bits;
    if (DERParseBitString(&leaf_certificate->signature_value, &signature,
                          &unused_bits) != DR_Success ||
        unused_bits != 0 || issuer_public_key->length > UINT32_MAX ||
        signature.length > UINT32_MAX) {
        return -1;
    }
    return implementation->verify_signature(
        issuer_public_key->data,
        (unsigned int)issuer_public_key->length,
        signature.data,
        (unsigned int)signature.length,
        digest,
        (unsigned int)descriptor->output_size,
        implementation,
        NULL
    );
}
