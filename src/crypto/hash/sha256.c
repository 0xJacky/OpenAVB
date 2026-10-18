#include "avp/crypto/hash/sha256.h"


static const uint32_t round_constants[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

static uint32_t rotate_right(uint32_t value, uint32_t count) {
    return (value >> count) | (value << (32 - count));
}

static uint32_t read_be32(const uint8_t *bytes) {
    return (uint32_t)bytes[0] << 24 |
        (uint32_t)bytes[1] << 16 |
        (uint32_t)bytes[2] << 8 |
        bytes[3];
}

static void compress(uint32_t state[8], const uint8_t block[64]) {
    uint32_t words[64];
    for (uint32_t index = 0; index < 16; ++index) {
        words[index] = read_be32(block + index * 4);
    }
    for (uint32_t index = 16; index < 64; ++index) {
        const uint32_t left = words[index - 15];
        const uint32_t right = words[index - 2];
        const uint32_t sigma0 = rotate_right(left, 7) ^
            rotate_right(left, 18) ^ (left >> 3);
        const uint32_t sigma1 = rotate_right(right, 17) ^
            rotate_right(right, 19) ^ (right >> 10);
        words[index] = words[index - 16] + sigma0 +
            words[index - 7] + sigma1;
    }

    uint32_t a = state[0];
    uint32_t b = state[1];
    uint32_t c = state[2];
    uint32_t d = state[3];
    uint32_t e = state[4];
    uint32_t f = state[5];
    uint32_t g = state[6];
    uint32_t h = state[7];
    for (uint32_t index = 0; index < 64; ++index) {
        const uint32_t sum1 = rotate_right(e, 6) ^
            rotate_right(e, 11) ^ rotate_right(e, 25);
        const uint32_t choice = (e & f) ^ (~e & g);
        const uint32_t temporary1 = h + sum1 + choice +
            round_constants[index] + words[index];
        const uint32_t sum0 = rotate_right(a, 2) ^
            rotate_right(a, 13) ^ rotate_right(a, 22);
        const uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        const uint32_t temporary2 = sum0 + majority;
        h = g;
        g = f;
        f = e;
        e = d + temporary1;
        d = c;
        c = b;
        b = a;
        a = temporary1 + temporary2;
    }
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

void sha256(const uint8_t *data, size_t size, uint8_t digest[32]) {
    uint32_t state[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19,
    };
    const size_t full_blocks = size / 64;
    for (size_t index = 0; index < full_blocks; ++index) {
        compress(state, data + index * 64);
    }

    uint8_t final_blocks[128] = {0};
    const size_t remainder = size % 64;
    for (size_t index = 0; index < remainder; ++index) {
        final_blocks[index] = data[full_blocks * 64 + index];
    }
    final_blocks[remainder] = 0x80;
    const size_t final_size = remainder < 56 ? 64 : 128;
    const uint64_t bit_size = (uint64_t)size * 8;
    for (uint32_t index = 0; index < 8; ++index) {
        final_blocks[final_size - 1 - index] =
            (uint8_t)(bit_size >> (index * 8));
    }
    compress(state, final_blocks);
    if (final_size == 128) {
        compress(state, final_blocks + 64);
    }
    for (uint32_t index = 0; index < 8; ++index) {
        digest[index * 4] = (uint8_t)(state[index] >> 24);
        digest[index * 4 + 1] = (uint8_t)(state[index] >> 16);
        digest[index * 4 + 2] = (uint8_t)(state[index] >> 8);
        digest[index * 4 + 3] = (uint8_t)state[index];
    }
}
