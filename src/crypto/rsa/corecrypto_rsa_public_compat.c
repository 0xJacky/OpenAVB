/*
 * Fixed-width public-key arithmetic follows LibTomMath's binary modular
 * exponentiation model. PKCS#1 v1.5 encoding follows RFC 8017 section 9.2.
 */
#include "avp/crypto/rsa/corecrypto_rsa_public_compat.h"

#include <string.h>


const uint8_t avp_ccrsa_fault_canary[16] = {
    0xea, 0xc5, 0x4a, 0x7c, 0x9f, 0x28, 0xdf, 0x10,
    0xb6, 0xe9, 0x3e, 0xb9, 0x1c, 0xd3, 0x3a, 0xc5,
};

static AVPCCUnit *rsa_modulus(AVPCCRSAPublicKey *key) {
    return key->storage;
}

static AVPCCUnit *rsa_exponent(AVPCCRSAPublicKey *key) {
    return key->storage + 2 * key->n + 1;
}

static int limbs_compare(
    size_t count,
    const AVPCCUnit *left,
    const AVPCCUnit *right
) {
    for (size_t index = count; index != 0; --index) {
        if (left[index - 1] > right[index - 1]) {
            return 1;
        }
        if (left[index - 1] < right[index - 1]) {
            return -1;
        }
    }
    return 0;
}

static AVPCCUnit limbs_sub(
    size_t count,
    AVPCCUnit *result,
    const AVPCCUnit *left,
    const AVPCCUnit *right
) {
    AVPCCUnit borrow = 0;
    for (size_t index = 0; index < count; ++index) {
        const AVPCCUnit rhs = right[index] + borrow;
        const AVPCCUnit rhs_overflow = rhs < right[index];
        const AVPCCUnit next = left[index] - rhs;
        borrow = rhs_overflow | (left[index] < rhs);
        result[index] = next;
    }
    return borrow;
}

static void limbs_add(
    size_t count,
    AVPCCUnit *result,
    const AVPCCUnit *left,
    const AVPCCUnit *right
) {
    AVPCCUnit carry = 0;
    for (size_t index = 0; index < count; ++index) {
        const AVPCCUnit partial = left[index] + carry;
        const AVPCCUnit carry_a = partial < left[index];
        const AVPCCUnit sum = partial + right[index];
        const AVPCCUnit carry_b = sum < partial;
        result[index] = sum;
        carry = carry_a | carry_b;
    }
}

static void add_mod(
    size_t count,
    AVPCCUnit *result,
    const AVPCCUnit *left,
    const AVPCCUnit *right,
    const AVPCCUnit *modulus
) {
    AVPCCUnit threshold[AVP_CCRSA_MAX_N];
    (void)limbs_sub(count, threshold, modulus, right);
    if (limbs_compare(count, left, threshold) >= 0) {
        (void)limbs_sub(count, result, left, threshold);
    } else {
        limbs_add(count, result, left, right);
    }
}

static void mul_mod(
    size_t count,
    AVPCCUnit *result,
    const AVPCCUnit *left,
    const AVPCCUnit *right,
    const AVPCCUnit *modulus,
    size_t modulus_bits
) {
    AVPCCUnit accumulator[AVP_CCRSA_MAX_N] = {0};
    AVPCCUnit doubled[AVP_CCRSA_MAX_N];
    memcpy(doubled, left, count * sizeof(*doubled));
    for (size_t bit = 0; bit < modulus_bits; ++bit) {
        if (((right[bit / 64] >> (bit & 63)) & 1u) != 0) {
            add_mod(count, accumulator, accumulator, doubled, modulus);
        }
        add_mod(count, doubled, doubled, doubled, modulus);
    }
    memcpy(result, accumulator, count * sizeof(*result));
}

static size_t limbs_bit_length(size_t count, const AVPCCUnit *value) {
    while (count != 0 && value[count - 1] == 0) {
        --count;
    }
    return count == 0 ? 0
                      : (count - 1) * 64u +
                            (64u - (size_t)__builtin_clzll(value[count - 1]));
}

static void pow_mod(
    size_t count,
    AVPCCUnit *result,
    const AVPCCUnit *base,
    const AVPCCUnit *exponent,
    const AVPCCUnit *modulus,
    size_t modulus_bits
) {
    AVPCCUnit accumulator[AVP_CCRSA_MAX_N] = {1};
    AVPCCUnit power[AVP_CCRSA_MAX_N];
    AVPCCUnit temporary[AVP_CCRSA_MAX_N];
    memcpy(power, base, count * sizeof(*power));
    const size_t exponent_bits = limbs_bit_length(count, exponent);
    for (size_t bit = 0; bit < exponent_bits; ++bit) {
        if (((exponent[bit / 64] >> (bit & 63)) & 1u) != 0) {
            mul_mod(
                count,
                temporary,
                accumulator,
                power,
                modulus,
                modulus_bits
            );
            memcpy(accumulator, temporary, count * sizeof(*accumulator));
        }
        if (bit + 1 != exponent_bits) {
            mul_mod(count, temporary, power, power, modulus, modulus_bits);
            memcpy(power, temporary, count * sizeof(*power));
        }
    }
    memcpy(result, accumulator, count * sizeof(*result));
}

static int import_uint(
    size_t count,
    AVPCCUnit *result,
    size_t input_size,
    const uint8_t *input
) {
    if (input == NULL || input_size == 0 || input_size > count * 8u) {
        return AVP_CCRSA_INVALID_INPUT;
    }
    memset(result, 0, count * sizeof(*result));
    for (size_t byte = 0; byte < input_size; ++byte) {
        const size_t source = input_size - byte - 1;
        result[byte / 8] |= (uint64_t)input[source] << ((byte & 7) * 8);
    }
    return AVP_CCERR_OK;
}

static void export_uint(
    size_t count,
    const AVPCCUnit *value,
    size_t output_size,
    uint8_t *output
) {
    memset(output, 0, output_size);
    for (size_t byte = 0; byte < output_size && byte < count * 8u; ++byte) {
        output[output_size - byte - 1] =
            (uint8_t)(value[byte / 8] >> ((byte & 7) * 8));
    }
}

int AVP_CCRSA_PUBLIC ccrsa_make_pub(
    AVPCCRSAPublicKey *key,
    size_t exponent_size,
    const uint8_t *exponent,
    size_t modulus_size,
    const uint8_t *modulus
) {
    if (key == NULL || key->n == 0 || key->n > AVP_CCRSA_MAX_N ||
        modulus == NULL || exponent == NULL || modulus_size == 0 ||
        exponent_size == 0 || modulus_size > key->n * 8u ||
        exponent_size > key->n * 8u) {
        return AVP_CCRSA_INVALID_INPUT;
    }

    AVPCCUnit *m = rsa_modulus(key);
    AVPCCUnit *e = rsa_exponent(key);
    (void)import_uint(key->n, m, modulus_size, modulus);
    (void)import_uint(key->n, e, exponent_size, exponent);
    if ((m[0] & 1u) == 0 || (e[0] & 1u) == 0 ||
        limbs_bit_length(key->n, m) < 2 ||
        limbs_bit_length(key->n, e) < 2 ||
        limbs_compare(key->n, e, m) >= 0) {
        return AVP_CCRSA_KEY_ERROR;
    }
    key->bit_length = limbs_bit_length(key->n, m);
    key->functions = NULL;
    key->storage[key->n] = 0;
    memset(key->storage + key->n + 1, 0, key->n * sizeof(AVPCCUnit));
    return AVP_CCERR_OK;
}

int AVP_CCRSA_PUBLIC ccrsa_pub_crypt_ws(
    void *workspace,
    AVPCCRSAPublicKey *key,
    AVPCCUnit *result,
    const AVPCCUnit *source
) {
    (void)workspace;
    if (key == NULL || result == NULL || source == NULL || key->n == 0 ||
        key->n > AVP_CCRSA_MAX_N || key->bit_length == 0 ||
        limbs_compare(key->n, source, rsa_modulus(key)) >= 0) {
        return AVP_CCRSA_KEY_ERROR;
    }
    pow_mod(
        key->n,
        result,
        source,
        rsa_exponent(key),
        rsa_modulus(key),
        key->bit_length
    );
    return AVP_CCERR_OK;
}

static size_t der_item_size(const uint8_t *der) {
    if (der == NULL || der[0] != 0x06 || der[1] >= 0x80) {
        return 0;
    }
    return (size_t)der[1] + 2u;
}

int AVP_CCRSA_PUBLIC ccrsa_emsa_pkcs1v15_verify_canary_out(
    size_t encoded_message_length,
    const uint8_t *encoded_message,
    size_t digest_length,
    const uint8_t *digest,
    const uint8_t *oid,
    uint8_t fault_canary_out[16]
) {
    if (fault_canary_out != NULL) {
        memset(fault_canary_out, 0, 16);
    }
    if (encoded_message == NULL || digest == NULL || digest_length == 0 ||
        fault_canary_out == NULL) {
        return AVP_CCRSA_INVALID_INPUT;
    }

    const size_t oid_size = oid == NULL ? 0 : der_item_size(oid);
    if (oid != NULL && oid_size == 0) {
        return AVP_CCRSA_INVALID_INPUT;
    }
    const size_t algorithm_size = oid == NULL ? 0 : 2u + oid_size + 2u;
    const size_t digest_info_size = oid == NULL
                                        ? digest_length
                                        : 2u + algorithm_size + 2u +
                                              digest_length;
    if (digest_length >= 0x80 || algorithm_size >= 0x80 ||
        digest_info_size >= 0x80 ||
        encoded_message_length < digest_info_size + 11u) {
        return AVP_CCRSA_INVALID_INPUT;
    }

    const size_t separator = encoded_message_length - digest_info_size - 1u;
    unsigned mismatch = encoded_message[0] | (encoded_message[1] ^ 0x01u);
    mismatch |= separator < 10u;
    for (size_t index = 2; index < separator; ++index) {
        mismatch |= encoded_message[index] ^ 0xffu;
    }
    mismatch |= encoded_message[separator];

    size_t cursor = separator + 1u;
    if (oid != NULL) {
        mismatch |= encoded_message[cursor++] ^ 0x30u;
        mismatch |= encoded_message[cursor++] ^ (uint8_t)(digest_info_size - 2u);
        mismatch |= encoded_message[cursor++] ^ 0x30u;
        mismatch |= encoded_message[cursor++] ^ (uint8_t)(algorithm_size - 2u);
        for (size_t index = 0; index < oid_size; ++index) {
            mismatch |= encoded_message[cursor++] ^ oid[index];
        }
        mismatch |= encoded_message[cursor++] ^ 0x05u;
        mismatch |= encoded_message[cursor++];
        mismatch |= encoded_message[cursor++] ^ 0x04u;
        mismatch |= encoded_message[cursor++] ^ (uint8_t)digest_length;
    }
    for (size_t index = 0; index < digest_length; ++index) {
        mismatch |= encoded_message[cursor++] ^ digest[index];
    }
    if (mismatch != 0 || cursor != encoded_message_length) {
        return AVP_CCERR_INVALID_SIGNATURE;
    }
    memcpy(fault_canary_out, avp_ccrsa_fault_canary, 16);
    return AVP_CCERR_OK;
}

int AVP_CCRSA_PUBLIC ccrsa_verify_pkcs1v15_internal_ws(
    void *workspace,
    AVPCCRSAPublicKey *key,
    const uint8_t *oid,
    size_t digest_length,
    const uint8_t *digest,
    size_t signature_length,
    const uint8_t *signature,
    int signature_length_validation,
    uint8_t fault_canary_out[16]
) {
    if (key == NULL || digest == NULL || signature == NULL ||
        fault_canary_out == NULL || key->n == 0 || key->n > AVP_CCRSA_MAX_N) {
        return AVP_CCRSA_INVALID_INPUT;
    }
    const size_t modulus_size = (key->bit_length + 7u) / 8u;
    const bool strict = signature_length_validation == AVP_CCRSA_SIG_LEN_STRICT;
    if ((strict && signature_length != modulus_size) ||
        (!strict && signature_length_validation !=
                        AVP_CCRSA_SIG_LEN_ALLOW_SHORT) ||
        signature_length == 0 || signature_length > modulus_size) {
        return AVP_CCRSA_INVALID_INPUT;
    }

    AVPCCUnit signature_value[AVP_CCRSA_MAX_N];
    AVPCCUnit encoded_value[AVP_CCRSA_MAX_N];
    uint8_t encoded_message[AVP_CCRSA_MAX_N * 8u];
    if (import_uint(key->n, signature_value, signature_length, signature) !=
        AVP_CCERR_OK) {
        return AVP_CCRSA_INVALID_INPUT;
    }
    if (limbs_compare(key->n, signature_value, rsa_modulus(key)) >= 0) {
        memset(fault_canary_out, 0, 16);
        return AVP_CCERR_INVALID_SIGNATURE;
    }
    const int crypt_result = ccrsa_pub_crypt_ws(
        workspace,
        key,
        encoded_value,
        signature_value
    );
    if (crypt_result != AVP_CCERR_OK) {
        memset(fault_canary_out, 0, 16);
        return crypt_result;
    }
    export_uint(key->n, encoded_value, modulus_size, encoded_message);
    return ccrsa_emsa_pkcs1v15_verify_canary_out(
        modulus_size,
        encoded_message,
        digest_length,
        digest,
        oid,
        fault_canary_out
    );
}

int AVP_CCRSA_PUBLIC ccrsa_verify_pkcs1v15(
    AVPCCRSAPublicKey *key,
    const uint8_t *oid,
    size_t digest_length,
    const uint8_t *digest,
    size_t signature_length,
    const uint8_t *signature,
    bool *valid
) {
    if (valid == NULL) {
        return AVP_CCRSA_INVALID_INPUT;
    }
    uint8_t canary[16];
    const int result = ccrsa_verify_pkcs1v15_internal_ws(
        NULL,
        key,
        oid,
        digest_length,
        digest,
        signature_length,
        signature,
        AVP_CCRSA_SIG_LEN_STRICT,
        canary
    );
    if (result == AVP_CCERR_OK || result == AVP_CCERR_INVALID_SIGNATURE) {
        *valid = result == AVP_CCERR_OK;
        return AVP_CCERR_OK;
    }
    *valid = false;
    return result;
}

int AVP_CCRSA_PUBLIC ccrsa_verify_pkcs1v15_digest(
    AVPCCRSAPublicKey *key,
    const uint8_t *oid,
    size_t digest_length,
    const uint8_t *digest,
    size_t signature_length,
    const uint8_t *signature,
    uint8_t fault_canary_out[16]
) {
    return ccrsa_verify_pkcs1v15_internal_ws(
        NULL,
        key,
        oid,
        digest_length,
        digest,
        signature_length,
        signature,
        AVP_CCRSA_SIG_LEN_STRICT,
        fault_canary_out
    );
}
