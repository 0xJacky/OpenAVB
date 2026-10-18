#include "avp/crypto/hash/corecrypto_sha3_compat.h"

#include <stddef.h>

static const uint64_t keccak_round_constants[24] = {
    UINT64_C(0x0000000000000001), UINT64_C(0x0000000000008082),
    UINT64_C(0x800000000000808a), UINT64_C(0x8000000080008000),
    UINT64_C(0x000000000000808b), UINT64_C(0x0000000080000001),
    UINT64_C(0x8000000080008081), UINT64_C(0x8000000000008009),
    UINT64_C(0x000000000000008a), UINT64_C(0x0000000000000088),
    UINT64_C(0x0000000080008009), UINT64_C(0x000000008000000a),
    UINT64_C(0x000000008000808b), UINT64_C(0x800000000000008b),
    UINT64_C(0x8000000000008089), UINT64_C(0x8000000000008003),
    UINT64_C(0x8000000000008002), UINT64_C(0x8000000000000080),
    UINT64_C(0x000000000000800a), UINT64_C(0x800000008000000a),
    UINT64_C(0x8000000080008081), UINT64_C(0x8000000000008080),
    UINT64_C(0x0000000080000001), UINT64_C(0x8000000080008008),
};

static const uint8_t keccak_rotation_offsets[24] = {
    1,  3,  6,  10, 15, 21, 28, 36, 45, 55, 2,  14,
    27, 41, 56, 8,  25, 43, 62, 18, 39, 61, 20, 44,
};

static const uint8_t keccak_permutation_indices[24] = {
    10, 7,  11, 17, 18, 3, 5,  16, 8,  21, 24, 4,
    15, 23, 19, 13, 12, 2, 20, 14, 22, 9,  6,  1,
};

static uint64_t rotate_left(uint64_t value, unsigned shift) {
    return (value << shift) | (value >> (64 - shift));
}

/* AVPBooter-compatible state lanes match tiny_sha3's x + 5*y ordering. */
void AVP_CCSHA3_PUBLIC AccelerateCrypto_SHA3_keccak(uint64_t state[25]) {
    uint64_t column[5];
    for (size_t round = 0; round < 24; ++round) {
        for (size_t x = 0; x < 5; ++x) {
            column[x] = state[x] ^ state[x + 5] ^ state[x + 10] ^
                        state[x + 15] ^ state[x + 20];
        }
        for (size_t x = 0; x < 5; ++x) {
            const uint64_t theta =
                column[(x + 4) % 5] ^ rotate_left(column[(x + 1) % 5], 1);
            for (size_t y = 0; y < 25; y += 5) {
                state[y + x] ^= theta;
            }
        }

        uint64_t carried = state[1];
        for (size_t index = 0; index < 24; ++index) {
            const size_t destination = keccak_permutation_indices[index];
            const uint64_t displaced = state[destination];
            state[destination] =
                rotate_left(carried, keccak_rotation_offsets[index]);
            carried = displaced;
        }

        for (size_t row = 0; row < 25; row += 5) {
            for (size_t x = 0; x < 5; ++x) {
                column[x] = state[row + x];
            }
            for (size_t x = 0; x < 5; ++x) {
                state[row + x] ^= (~column[(x + 1) % 5]) & column[(x + 2) % 5];
            }
        }
        state[0] ^= keccak_round_constants[round];
    }
}
