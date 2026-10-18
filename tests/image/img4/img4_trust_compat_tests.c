#include "avp/image/img4/img4_trust_compat.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static const uint8_t sha384_oid[] = {
    0x06, 0x09, 0x60, 0x86, 0x48, 0x01,
    0x65, 0x03, 0x04, 0x02, 0x02,
};
static const uint8_t sha384_with_rsa_oid_bytes[] = {
    0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0c,
};
static const DERItem sha384_with_rsa_oid = {
    (DERByte *)sha384_with_rsa_oid_bytes,
    sizeof(sha384_with_rsa_oid_bytes),
};

static int mock_digest(
    const void *data,
    size_t data_length,
    void *digest,
    size_t digest_length,
    const Img4DecodeImplementation *implementation
) {
    (void)implementation;
    assert(data_length == 3);
    assert(memcmp(data, "tbs", 3) == 0);
    memset(digest, 0xa5, digest_length);
    return 0;
}

static int mock_verify(const uint8_t *public_key,
                       unsigned int public_key_length,
                       const uint8_t *signature,
                       unsigned int signature_length,
                       const uint8_t *digest,
                       unsigned int digest_length,
                       const Img4DecodeImplementation *implementation,
                       void *context) {
    (void)implementation;
    assert(context == NULL);
    assert(public_key_length == 3 && memcmp(public_key, "key", 3) == 0);
    assert(signature_length == 3 && memcmp(signature, "sig", 3) == 0);
    assert(digest_length == 48);
    for (size_t index = 0; index < digest_length; ++index) {
        assert(digest[index] == 0xa5);
    }
    return 0;
}

static Img4DecodeImplementation make_implementation(void) {
    static const Img4DigestDescriptor descriptor = {
        .output_size = 48,
        .oid = sha384_oid,
    };
    static const Img4DigestDescriptor *descriptor_slot = &descriptor;
    static const Img4DigestConfiguration configuration = {
        .descriptor = &descriptor_slot,
    };
    return (Img4DecodeImplementation){
        .compute_digest = mock_digest,
        .verify_signature = mock_verify,
        .digest_configuration = &configuration,
        .signature_algorithm_oid = &sha384_with_rsa_oid,
    };
}

static void test_checked_key_import(void) {
    uint64_t context[AVP_CCRSA_PUBLIC_CONTEXT_UNITS(4)] = {0};
    AVPCCRSAPublicKey *key = (AVPCCRSAPublicKey *)context;
    key->n = 4;
    const uint8_t modulus[] = {0, 0, 0x0c, 0xa1};
    const uint8_t exponent[] = {0x11};
    assert(img4_make_rsa_public_key_checked(
               key, context + AVP_CCRSA_PUBLIC_CONTEXT_UNITS(4),
               sizeof(modulus), modulus, sizeof(exponent), exponent) == 0);
    assert(key->n == 1 && key->bit_length == 12);

    key->n = 4;
    assert(img4_make_rsa_public_key_checked(
               key, context + 4, sizeof(modulus), modulus, sizeof(exponent),
               exponent) == -1);
}

static void test_chain_signature_adapter(void) {
    uint8_t algorithm[] = {
        0x06, 0x09, 0x2a, 0x86, 0x48, 0x86,
        0xf7, 0x0d, 0x01, 0x01, 0x0c, 0x05, 0x00,
    };
    uint8_t signature_bits[] = {0, 's', 'i', 'g'};
    Img4CertificateFields leaf = {
        .tbs_certificate = {(DERByte *)"tbs", 3},
        .signature_algorithm = {algorithm, sizeof(algorithm)},
        .signature_value = {signature_bits, sizeof(signature_bits)},
    };
    DERItem issuer_key = {(DERByte *)"key", 3};
    Img4DecodeImplementation implementation = make_implementation();
    assert(verify_chain_signatures(&issuer_key, &leaf, &implementation) == 0);

    for (size_t prefix = 0; prefix < sizeof(algorithm) - 2; ++prefix) {
        leaf.signature_algorithm.length = prefix;
        assert(verify_chain_signatures(&issuer_key, &leaf, &implementation) ==
               -1);
    }
    leaf.signature_algorithm.length = sizeof(algorithm) - 2;
    assert(verify_chain_signatures(&issuer_key, &leaf, &implementation) == 0);
    leaf.signature_algorithm.length = sizeof(algorithm);
    signature_bits[0] = 1;
    assert(verify_chain_signatures(&issuer_key, &leaf, &implementation) == -1);
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
    assert(argc == 4);
    size_t public_key_size, digest_size, signature_size;
    uint8_t *public_key = read_file(argv[1], &public_key_size);
    uint8_t *digest = read_file(argv[2], &digest_size);
    uint8_t *signature = read_file(argv[3], &signature_size);
    DERItem public_key_item = {public_key, public_key_size};
    DERItem digest_item = {digest, digest_size};
    DERItem signature_item = {signature, signature_size};
    assert(verify_pkcs1_sig(&public_key_item, sha384_oid, &digest_item,
                            &signature_item, 4096) == 0);
    signature[signature_size - 1] ^= 1;
    assert(verify_pkcs1_sig(&public_key_item, sha384_oid, &digest_item,
                            &signature_item, 4096) == -1);
    for (size_t prefix = 0; prefix < public_key_size; ++prefix) {
        public_key_item.length = prefix;
        assert(verify_pkcs1_sig(&public_key_item, sha384_oid, &digest_item,
                                &signature_item, 4096) == -1);
    }
    free(signature);
    free(digest);
    free(public_key);
}

int main(int argc, char **argv) {
    test_checked_key_import();
    test_chain_signature_adapter();
    if (argc != 1) {
        test_oracle(argc, argv);
    }
    return 0;
}
