#include "avp/crypto/hash/corecrypto_sha512_compat.h"

#define OPENSSL_SUPPRESS_DEPRECATED
#include <openssl/sha.h>

#include <assert.h>
#include <string.h>

typedef void (*CompressFunction)(
    uint64_t state[8],
    size_t block_count,
    const void *blocks
);

static const uint64_t sha512_initial_state[8] = {
    UINT64_C(0x6a09e667f3bcc908), UINT64_C(0xbb67ae8584caa73b),
    UINT64_C(0x3c6ef372fe94f82b), UINT64_C(0xa54ff53a5f1d36f1),
    UINT64_C(0x510e527fade682d1), UINT64_C(0x9b05688c2b3e6c1f),
    UINT64_C(0x1f83d9abfb41bd6b), UINT64_C(0x5be0cd19137e2179),
};

static uint64_t next_word(uint64_t *state) {
    uint64_t value = *state;
    value ^= value << 13;
    value ^= value >> 7;
    value ^= value << 17;
    *state = value;
    return value;
}

static void openssl_compress(
    uint64_t state[8],
    size_t block_count,
    const uint8_t *blocks
) {
    SHA512_CTX context;
    assert(SHA512_Init(&context));
    for (size_t index = 0; index < 8; ++index) {
        context.h[index] = state[index];
    }
    for (size_t block = 0; block < block_count; ++block) {
        SHA512_Transform(&context, blocks + block * AVP_SHA512_BLOCK_SIZE);
    }
    for (size_t index = 0; index < 8; ++index) {
        state[index] = context.h[index];
    }
}

static void test_compression_oracle(void) {
    static const CompressFunction functions[] = {
        ccsha384_vng_arm_hw_compress,
        ccsha512_ltc_compress,
        AccelerateCrypto_SHA512_compress_hwassist,
    };
    uint64_t random_state = UINT64_C(0x7368613531322d63);
    uint8_t blocks[4 * AVP_SHA512_BLOCK_SIZE];
    for (size_t iteration = 0; iteration < 256; ++iteration) {
        for (size_t index = 0; index < sizeof(blocks); index += 8) {
            const uint64_t word = next_word(&random_state);
            memcpy(blocks + index, &word, sizeof(word));
        }
        uint64_t initial[8];
        for (size_t index = 0; index < 8; ++index) {
            initial[index] = iteration == 0 ? sha512_initial_state[index]
                                            : next_word(&random_state);
        }
        const size_t block_count = iteration & 3;
        uint64_t expected[8];
        memcpy(expected, initial, sizeof(expected));
        openssl_compress(expected, block_count, blocks);

        for (size_t function = 0;
             function < sizeof(functions) / sizeof(functions[0]); ++function) {
            uint64_t actual[8];
            memcpy(actual, initial, sizeof(actual));
            functions[function](actual, block_count, blocks);
            assert(memcmp(actual, expected, sizeof(actual)) == 0);
        }
    }
}

static void digest_with_compat(
    const AVPCCSHA512Info *info,
    const uint8_t *input,
    size_t input_size,
    uint8_t *digest
) {
    AVPCCSHA512Context context;
    avp_ccsha512_init(&context, info);
    size_t offset = 0;
    while (offset < input_size) {
        size_t chunk = (offset * 17 + 23) % 97 + 1;
        if (chunk > input_size - offset) {
            chunk = input_size - offset;
        }
        assert(avp_ccsha512_update(&context, input + offset, chunk) == 0);
        offset += chunk;
    }
    if (input_size == 0) {
        assert(avp_ccsha512_update(&context, 0, 0) == 0);
    }
    AVPCCSHA512Context original;
    memcpy(&original, &context, sizeof(original));
    assert(ccsha512_final(info, &context, digest) == 0);
    assert(memcmp(&context, &original, sizeof(context)) == 0);
}

static void test_nist_abc_vectors(void) {
    static const uint8_t sha384_abc[AVP_SHA384_DIGEST_SIZE] = {
        0xcb, 0x00, 0x75, 0x3f, 0x45, 0xa3, 0x5e, 0x8b, 0xb5, 0xa0, 0x3d, 0x69,
        0x9a, 0xc6, 0x50, 0x07, 0x27, 0x2c, 0x32, 0xab, 0x0e, 0xde, 0xd1, 0x63,
        0x1a, 0x8b, 0x60, 0x5a, 0x43, 0xff, 0x5b, 0xed, 0x80, 0x86, 0x07, 0x2b,
        0xa1, 0xe7, 0xcc, 0x23, 0x58, 0xba, 0xec, 0xa1, 0x34, 0xc8, 0x25, 0xa7,
    };
    static const uint8_t sha512_abc[AVP_SHA512_DIGEST_SIZE] = {
        0xdd, 0xaf, 0x35, 0xa1, 0x93, 0x61, 0x7a, 0xba, 0xcc, 0x41, 0x73,
        0x49, 0xae, 0x20, 0x41, 0x31, 0x12, 0xe6, 0xfa, 0x4e, 0x89, 0xa9,
        0x7e, 0xa2, 0x0a, 0x9e, 0xee, 0xe6, 0x4b, 0x55, 0xd3, 0x9a, 0x21,
        0x92, 0x99, 0x2a, 0x27, 0x4f, 0xc1, 0xa8, 0x36, 0xba, 0x3c, 0x23,
        0xa3, 0xfe, 0xeb, 0xbd, 0x45, 0x4d, 0x44, 0x23, 0x64, 0x3c, 0xe8,
        0x0e, 0x2a, 0x9a, 0xc9, 0x4f, 0xa5, 0x4c, 0xa4, 0x9f,
    };
    static const uint8_t abc[] = {'a', 'b', 'c'};
    uint8_t actual[AVP_SHA512_DIGEST_SIZE];
    digest_with_compat(&avp_ccsha384_info, abc, sizeof(abc), actual);
    assert(memcmp(actual, sha384_abc, sizeof(sha384_abc)) == 0);
    digest_with_compat(&avp_ccsha512_info, abc, sizeof(abc), actual);
    assert(memcmp(actual, sha512_abc, sizeof(sha512_abc)) == 0);
}

static void test_finalization_oracle(void) {
    static const size_t lengths[] = {
        0, 1, 3, 111, 112, 113, 127, 128, 129, 255, 256, 1024,
    };
    uint8_t input[1024];
    uint64_t random_state = UINT64_C(0x66696e616c2d3531);
    for (size_t index = 0; index < sizeof(input); index += 8) {
        const uint64_t word = next_word(&random_state);
        memcpy(input + index, &word, sizeof(word));
    }

    for (size_t index = 0; index < sizeof(lengths) / sizeof(lengths[0]);
         ++index) {
        uint8_t expected[AVP_SHA512_DIGEST_SIZE];
        uint8_t actual[AVP_SHA512_DIGEST_SIZE];
        assert(SHA384(input, lengths[index], expected) == expected);
        digest_with_compat(&avp_ccsha384_info, input, lengths[index], actual);
        assert(memcmp(actual, expected, AVP_SHA384_DIGEST_SIZE) == 0);

        assert(SHA512(input, lengths[index], expected) == expected);
        digest_with_compat(&avp_ccsha512_info, input, lengths[index], actual);
        assert(memcmp(actual, expected, AVP_SHA512_DIGEST_SIZE) == 0);
    }
}

int main(void) {
    test_compression_oracle();
    test_nist_abc_vectors();
    test_finalization_oracle();
    return 0;
}
