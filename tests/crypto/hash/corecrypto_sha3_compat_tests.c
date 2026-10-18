#include "avp/crypto/hash/corecrypto_sha3_compat.h"

#include <openssl/evp.h>

#include <assert.h>
#include <stddef.h>
#include <string.h>

typedef union {
    uint64_t lanes[25];
    uint8_t bytes[200];
} KeccakState;

static uint64_t next_word(uint64_t *state) {
    uint64_t value = *state;
    value ^= value << 13;
    value ^= value >> 7;
    value ^= value << 17;
    *state = value;
    return value;
}

static void test_zero_state_permutation(void) {
    static const uint64_t expected[25] = {
        UINT64_C(0xf1258f7940e1dde7), UINT64_C(0x84d5ccf933c0478a),
        UINT64_C(0xd598261ea65aa9ee), UINT64_C(0xbd1547306f80494d),
        UINT64_C(0x8b284e056253d057), UINT64_C(0xff97a42d7f8e6fd4),
        UINT64_C(0x90fee5a0a44647c4), UINT64_C(0x8c5bda0cd6192e76),
        UINT64_C(0xad30a6f71b19059c), UINT64_C(0x30935ab7d08ffc64),
        UINT64_C(0xeb5aa93f2317d635), UINT64_C(0xa9a6e6260d712103),
        UINT64_C(0x81a57c16dbcf555f), UINT64_C(0x43b831cd0347c826),
        UINT64_C(0x01f22f1a11a5569f), UINT64_C(0x05e5635a21d9ae61),
        UINT64_C(0x64befef28cc970f2), UINT64_C(0x613670957bc46611),
        UINT64_C(0xb87c5a554fd00ecb), UINT64_C(0x8c3ee88a1ccf32c8),
        UINT64_C(0x940c7922ae3a2614), UINT64_C(0x1841f924a2c509e4),
        UINT64_C(0x16f53526e70465c2), UINT64_C(0x75f644e97f30a13b),
        UINT64_C(0xeaf1ff7b5ceca249),
    };
    uint64_t actual[25] = {0};
    AccelerateCrypto_SHA3_keccak(actual);
    assert(memcmp(actual, expected, sizeof(actual)) == 0);
}

static void sha3_with_compat(
    const uint8_t *input,
    size_t input_size,
    uint8_t *digest,
    size_t digest_size
) {
    KeccakState state = {0};
    const size_t rate = sizeof(state.bytes) - 2 * digest_size;
    while (input_size >= rate) {
        for (size_t index = 0; index < rate; ++index) {
            state.bytes[index] ^= input[index];
        }
        AccelerateCrypto_SHA3_keccak(state.lanes);
        input += rate;
        input_size -= rate;
    }
    for (size_t index = 0; index < input_size; ++index) {
        state.bytes[index] ^= input[index];
    }
    state.bytes[input_size] ^= UINT8_C(0x06);
    state.bytes[rate - 1] ^= UINT8_C(0x80);
    AccelerateCrypto_SHA3_keccak(state.lanes);
    memcpy(digest, state.bytes, digest_size);
}

static void compare_digest(
    const EVP_MD *algorithm,
    size_t digest_size,
    const uint8_t *input,
    size_t input_size
) {
    uint8_t expected[EVP_MAX_MD_SIZE];
    unsigned int expected_size = 0;
    assert(EVP_Digest(input, input_size, expected, &expected_size, algorithm,
                      0) == 1);
    assert(expected_size == digest_size);
    uint8_t actual[EVP_MAX_MD_SIZE];
    sha3_with_compat(input, input_size, actual, digest_size);
    assert(memcmp(actual, expected, digest_size) == 0);
}

static void test_openssl_sha3_oracle(void) {
    static const size_t boundary_lengths[] = {
        0, 1, 2, 3, 71, 72, 103, 104, 135, 136, 143, 144, 145, 255, 512,
    };
    uint8_t input[1024];
    uint64_t random_state = UINT64_C(0x6b656363616b2d66);
    for (size_t index = 0; index < sizeof(input); index += 8) {
        const uint64_t word = next_word(&random_state);
        memcpy(input + index, &word, sizeof(word));
    }

    for (size_t index = 0;
         index < sizeof(boundary_lengths) / sizeof(boundary_lengths[0]);
         ++index) {
        const size_t length = boundary_lengths[index];
        compare_digest(EVP_sha3_224(), 28, input, length);
        compare_digest(EVP_sha3_256(), 32, input, length);
        compare_digest(EVP_sha3_384(), 48, input, length);
        compare_digest(EVP_sha3_512(), 64, input, length);
    }

    for (size_t iteration = 0; iteration < 256; ++iteration) {
        const size_t length = next_word(&random_state) % sizeof(input);
        compare_digest(EVP_sha3_224(), 28, input, length);
        compare_digest(EVP_sha3_256(), 32, input, length);
        compare_digest(EVP_sha3_384(), 48, input, length);
        compare_digest(EVP_sha3_512(), 64, input, length);
    }
}

int main(void) {
    test_zero_state_permutation();
    test_openssl_sha3_oracle();
    return 0;
}
