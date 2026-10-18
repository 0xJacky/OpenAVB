#pragma once

#include "avp/crypto/bignum/corecrypto_limb_compat.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


enum {
    AVP_CCRSA_MAX_N = 64,
    AVP_CCERR_OK = 0,
    AVP_CCRSA_INVALID_INPUT = -23,
    AVP_CCRSA_KEY_ERROR = -28,
    AVP_CCERR_INVALID_SIGNATURE = -146,
    AVP_CCRSA_SIG_LEN_STRICT = 0,
    AVP_CCRSA_SIG_LEN_ALLOW_SHORT = 0x7dcdc05e,
};

typedef struct {
    AVPCCSize n;
    size_t bit_length;
    const void *functions;
    AVPCCUnit storage[];
} AVPCCRSAPublicKey;

#define AVP_CCRSA_PUBLIC __attribute__((used, retain))
#define AVP_CCRSA_PUBLIC_CONTEXT_UNITS(n) (3u * (n) + 4u)

extern const uint8_t avp_ccrsa_fault_canary[16];

AVP_CCRSA_PUBLIC int ccrsa_make_pub(
    AVPCCRSAPublicKey *key,
    size_t exponent_size,
    const uint8_t *exponent,
    size_t modulus_size,
    const uint8_t *modulus
);

AVP_CCRSA_PUBLIC int ccrsa_pub_crypt_ws(
    void *workspace,
    AVPCCRSAPublicKey *key,
    AVPCCUnit *result,
    const AVPCCUnit *source
);

AVP_CCRSA_PUBLIC int ccrsa_emsa_pkcs1v15_verify_canary_out(
    size_t encoded_message_length,
    const uint8_t *encoded_message,
    size_t digest_length,
    const uint8_t *digest,
    const uint8_t *oid,
    uint8_t fault_canary_out[16]
);

AVP_CCRSA_PUBLIC int ccrsa_verify_pkcs1v15_internal_ws(
    void *workspace,
    AVPCCRSAPublicKey *key,
    const uint8_t *oid,
    size_t digest_length,
    const uint8_t *digest,
    size_t signature_length,
    const uint8_t *signature,
    int signature_length_validation,
    uint8_t fault_canary_out[16]
);

AVP_CCRSA_PUBLIC int ccrsa_verify_pkcs1v15(
    AVPCCRSAPublicKey *key,
    const uint8_t *oid,
    size_t digest_length,
    const uint8_t *digest,
    size_t signature_length,
    const uint8_t *signature,
    bool *valid
);

AVP_CCRSA_PUBLIC int ccrsa_verify_pkcs1v15_digest(
    AVPCCRSAPublicKey *key,
    const uint8_t *oid,
    size_t digest_length,
    const uint8_t *digest,
    size_t signature_length,
    const uint8_t *signature,
    uint8_t fault_canary_out[16]
);
