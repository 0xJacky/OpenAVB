#include "avp/crypto/rsa/corecrypto_rsa_public_compat.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static const uint8_t sha384_oid[] = {
    0x06, 0x09, 0x60, 0x86, 0x48, 0x01,
    0x65, 0x03, 0x04, 0x02, 0x02,
};

static void test_public_operation(void) {
    uint64_t context[AVP_CCRSA_PUBLIC_CONTEXT_UNITS(1)] = {0};
    AVPCCRSAPublicKey *key = (AVPCCRSAPublicKey *)context;
    key->n = 1;
    const uint8_t modulus[] = {0x0c, 0xa1};
    const uint8_t exponent[] = {0x11};
    assert(ccrsa_make_pub(key, sizeof(exponent), exponent, sizeof(modulus),
                          modulus) == AVP_CCERR_OK);
    const uint64_t source = 65;
    uint64_t result = 0;
    assert(ccrsa_pub_crypt_ws(NULL, key, &result, &source) == AVP_CCERR_OK);
    assert(result == 2790);
    const uint64_t out_of_range = 3233;
    assert(ccrsa_pub_crypt_ws(NULL, key, &result, &out_of_range) ==
           AVP_CCRSA_KEY_ERROR);
}

static void make_encoded_message(uint8_t encoded[128], const uint8_t digest[48]) {
    const size_t digest_info_size = 2 + (2 + sizeof(sha384_oid) + 2) + 2 + 48;
    const size_t separator = 128 - digest_info_size - 1;
    encoded[0] = 0;
    encoded[1] = 1;
    memset(encoded + 2, 0xff, separator - 2);
    encoded[separator] = 0;
    size_t cursor = separator + 1;
    encoded[cursor++] = 0x30;
    encoded[cursor++] = (uint8_t)(digest_info_size - 2);
    encoded[cursor++] = 0x30;
    encoded[cursor++] = (uint8_t)(sizeof(sha384_oid) + 2);
    memcpy(encoded + cursor, sha384_oid, sizeof(sha384_oid));
    cursor += sizeof(sha384_oid);
    encoded[cursor++] = 5;
    encoded[cursor++] = 0;
    encoded[cursor++] = 4;
    encoded[cursor++] = 48;
    memcpy(encoded + cursor, digest, 48);
    cursor += 48;
    assert(cursor == 128);
}

static void test_emsa(void) {
    uint8_t digest[48];
    for (size_t index = 0; index < sizeof(digest); ++index) {
        digest[index] = (uint8_t)(index * 3 + 1);
    }
    uint8_t encoded[128];
    uint8_t canary[16];
    make_encoded_message(encoded, digest);
    assert(ccrsa_emsa_pkcs1v15_verify_canary_out(
               sizeof(encoded), encoded, sizeof(digest), digest, sha384_oid,
               canary) == AVP_CCERR_OK);
    assert(memcmp(canary, avp_ccrsa_fault_canary, sizeof(canary)) == 0);
    for (size_t prefix = 0; prefix < sizeof(encoded); ++prefix) {
        assert(ccrsa_emsa_pkcs1v15_verify_canary_out(
                   prefix, encoded, sizeof(digest), digest, sha384_oid,
                   canary) != AVP_CCERR_OK);
    }
    encoded[17] ^= 1;
    assert(ccrsa_emsa_pkcs1v15_verify_canary_out(
               sizeof(encoded), encoded, sizeof(digest), digest, sha384_oid,
               canary) == AVP_CCERR_INVALID_SIGNATURE);
    assert(memcmp(canary, (uint8_t[16]){0}, sizeof(canary)) == 0);
}

static uint8_t *read_file(const char *path, size_t *size) {
    FILE *file = fopen(path, "rb");
    assert(file != NULL);
    assert(fseek(file, 0, SEEK_END) == 0);
    const long length = ftell(file);
    assert(length > 0);
    assert(fseek(file, 0, SEEK_SET) == 0);
    uint8_t *bytes = malloc((size_t)length);
    assert(bytes != NULL);
    assert(fread(bytes, 1, (size_t)length, file) == (size_t)length);
    assert(fclose(file) == 0);
    *size = (size_t)length;
    return bytes;
}

static void test_oracle(int argc, char **argv) {
    assert(argc == 5);
    size_t modulus_size, exponent_size, digest_size, signature_size;
    uint8_t *modulus = read_file(argv[1], &modulus_size);
    uint8_t *exponent = read_file(argv[2], &exponent_size);
    uint8_t *digest = read_file(argv[3], &digest_size);
    uint8_t *signature = read_file(argv[4], &signature_size);
    assert(digest_size == 48);
    const size_t n = (modulus_size + 7) / 8;
    assert(n <= AVP_CCRSA_MAX_N);
    uint64_t context[AVP_CCRSA_PUBLIC_CONTEXT_UNITS(AVP_CCRSA_MAX_N)] = {0};
    AVPCCRSAPublicKey *key = (AVPCCRSAPublicKey *)context;
    key->n = n;
    assert(ccrsa_make_pub(key, exponent_size, exponent, modulus_size, modulus) ==
           AVP_CCERR_OK);
    uint8_t canary[16];
    assert(ccrsa_verify_pkcs1v15_digest(
               key, sha384_oid, digest_size, digest, signature_size, signature,
               canary) == AVP_CCERR_OK);
    assert(memcmp(canary, avp_ccrsa_fault_canary, sizeof(canary)) == 0);
    signature[signature_size / 2] ^= 0x80;
    assert(ccrsa_verify_pkcs1v15_digest(
               key, sha384_oid, digest_size, digest, signature_size, signature,
               canary) == AVP_CCERR_INVALID_SIGNATURE);
    bool valid = true;
    assert(ccrsa_verify_pkcs1v15(
               key, sha384_oid, digest_size, digest, signature_size, signature,
               &valid) == AVP_CCERR_OK);
    assert(!valid);
    free(signature);
    free(digest);
    free(exponent);
    free(modulus);
}

static void test_key_validation(void) {
    uint64_t context[AVP_CCRSA_PUBLIC_CONTEXT_UNITS(1)] = {0};
    AVPCCRSAPublicKey *key = (AVPCCRSAPublicKey *)context;
    const uint8_t exponent[] = {3};
    const uint8_t even_modulus[] = {12};
    key->n = 1;
    assert(ccrsa_make_pub(key, 1, exponent, 1, even_modulus) ==
           AVP_CCRSA_KEY_ERROR);
    key->n = 0;
    assert(ccrsa_make_pub(key, 1, exponent, 1, even_modulus) ==
           AVP_CCRSA_INVALID_INPUT);
    const uint8_t small_modulus[] = {3};
    key->n = 1;
    assert(ccrsa_make_pub(key, 1, exponent, 1, small_modulus) ==
           AVP_CCRSA_KEY_ERROR);
}

int main(int argc, char **argv) {
    test_public_operation();
    test_emsa();
    test_key_validation();
    if (argc != 1) {
        test_oracle(argc, argv);
    }
    return 0;
}
