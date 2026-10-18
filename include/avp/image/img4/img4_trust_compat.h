#pragma once

#include "avp/crypto/rsa/corecrypto_rsa_public_compat.h"
#include "avp/image/img4/img4_x509_compat.h"

#include <stddef.h>
#include <stdint.h>


typedef struct Img4DecodeImplementation Img4DecodeImplementation;

typedef int (*Img4ComputeDigestCallback)(
    const void *data,
    size_t data_length,
    void *digest,
    size_t digest_length,
    const Img4DecodeImplementation *implementation
);

typedef int (*Img4VerifySignatureCallback)(
    const uint8_t *public_key,
    unsigned int public_key_length,
    const uint8_t *signature,
    unsigned int signature_length,
    const uint8_t *digest,
    unsigned int digest_length,
    const Img4DecodeImplementation *implementation,
    void *context
);

typedef struct {
    size_t output_size;
    uint8_t reserved[0x18];
    const uint8_t *oid;
} Img4DigestDescriptor;

typedef struct {
    const void *reserved[2];
    const Img4DigestDescriptor *const *descriptor;
} Img4DigestConfiguration;

struct Img4DecodeImplementation {
    Img4ComputeDigestCallback compute_digest;
    const void *reserved_08;
    Img4VerifySignatureCallback verify_signature;
    const void *reserved_18;
    const Img4DigestConfiguration *digest_configuration;
    const DERItem *signature_algorithm_oid;
};

#define IMG4_TRUST_PUBLIC __attribute__((used, retain))

IMG4_TRUST_PUBLIC int img4_make_rsa_public_key_checked(
    AVPCCRSAPublicKey *key,
    const void *key_upper_bound,
    size_t modulus_length,
    const uint8_t *modulus,
    size_t exponent_length,
    const uint8_t *exponent
);

IMG4_TRUST_PUBLIC int verify_pkcs1_sig(
    const DERItem *public_key,
    const uint8_t *digest_oid,
    const DERItem *digest,
    const DERItem *signature,
    uint64_t rsa_key_bits
);

IMG4_TRUST_PUBLIC int verify_signature_rsa(
    const uint8_t *public_key,
    unsigned int public_key_length,
    const uint8_t *signature,
    unsigned int signature_length,
    const uint8_t *digest,
    unsigned int digest_length,
    const Img4DecodeImplementation *implementation
);

IMG4_TRUST_PUBLIC int verify_chain_signatures(
    const DERItem *issuer_public_key,
    const Img4CertificateFields *leaf_certificate,
    const Img4DecodeImplementation *implementation
);
