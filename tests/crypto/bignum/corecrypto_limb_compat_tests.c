#include "avp/crypto/bignum/corecrypto_limb_compat.h"

#include <openssl/bn.h>

#include <assert.h>
#include <string.h>


static BIGNUM *bn_from_limbs(const AVPCCUnit *limbs, size_t count) {
    BIGNUM *value = BN_lebin2bn(
        (const unsigned char *)limbs,
        (int)(count * sizeof(*limbs)),
        0
    );
    assert(value != 0);
    return value;
}

static void test_shape_and_comparison(void) {
    AVPCCUnit value[4];
    ccn_seti(4, value, UINT64_C(0x123456789abcdef0));
    assert(value[0] == UINT64_C(0x123456789abcdef0));
    assert(value[1] == 0 && value[2] == 0 && value[3] == 0);

    value[2] = 7;
    assert(ccn_n(4, value) == 3);
    assert(ccn_n_noasm(4, value) == 3);
    assert(ccn_cmp_asm(4, value, value) == 0);
    AVPCCUnit larger[4];
    ccn_set(4, larger, value);
    larger[3] = 1;
    assert(ccn_cmp_internal(4, larger, value) == 1);
    assert(ccn_cmp_asm(4, value, larger) == -1);

    AVPCCUnit overlap[] = { 1, 2, 3, 4, 5 };
    ccn_set(4, overlap + 1, overlap);
    const AVPCCUnit backward_expected[] = { 1, 1, 2, 3, 4 };
    assert(memcmp(overlap, backward_expected, sizeof(overlap)) == 0);
    ccn_set(4, overlap, overlap + 1);
    const AVPCCUnit forward_expected[] = { 1, 2, 3, 4, 4 };
    assert(memcmp(overlap, forward_expected, sizeof(overlap)) == 0);

    ccn_zero(4, value);
    assert(ccn_n(4, value) == 0);
}

static void test_bit_lengths_against_openssl(void) {
    static const AVPCCUnit vectors[][4] = {
        { 0, 0, 0, 0 },
        { 1, 0, 0, 0 },
        { UINT64_MAX, 0, 0, 0 },
        { 0, UINT64_C(1) << 17, 0, 0 },
        { UINT64_C(0x1020304050607080), 0, 9, UINT64_C(1) << 63 },
    };

    for (size_t index = 0; index < sizeof(vectors) / sizeof(vectors[0]); ++index) {
        BIGNUM *oracle = bn_from_limbs(vectors[index], 4);
        const size_t bit_length = (size_t)BN_num_bits(oracle);
        assert(ccn_bitlen_public_value(4, vectors[index]) == bit_length);
        assert(ccn_bitlen_internal(4, vectors[index]) == bit_length);
        assert(ccn_write_uint_size_public_value(4, vectors[index]) ==
            (bit_length + 7) / 8);
        BN_free(oracle);
    }
}

static void test_read_uint_against_openssl(void) {
    static const uint8_t encoded[] = {
        0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
        0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10,
        0x55,
    };
    AVPCCUnit actual[3];
    assert(ccn_read_uint_public_value(
        3,
        actual,
        sizeof(encoded),
        encoded
    ) == 0);

    BIGNUM *oracle = BN_bin2bn(encoded, (int)sizeof(encoded), 0);
    assert(oracle != 0);
    uint8_t expected[sizeof(actual)];
    assert(BN_bn2lebinpad(oracle, expected, sizeof(expected)) ==
        (int)sizeof(expected));
    assert(memcmp(actual, expected, sizeof(actual)) == 0);
    BN_free(oracle);

    static const uint8_t permitted_padding[25] = { 0 };
    assert(ccn_read_uint_internal(
        3,
        actual,
        sizeof(permitted_padding),
        permitted_padding
    ) == 0);
    uint8_t overflow[25] = { 1 };
    assert(ccn_read_uint_public_value(
        3,
        actual,
        sizeof(overflow),
        overflow
    ) == AVP_CCN_READ_UINT_OVERFLOW);
}

static void test_negation_against_openssl(void) {
    const AVPCCUnit input[2] = {
        UINT64_C(0x0123456789abcdef),
        UINT64_C(0xfedcba9876543210),
    };
    AVPCCUnit actual[2];
    assert(ccn_neg(2, actual, input) == 0);

    BIGNUM *oracle = bn_from_limbs(input, 2);
    BIGNUM *modulus = BN_new();
    BIGNUM *expected_bn = BN_new();
    assert(modulus != 0 && expected_bn != 0);
    assert(BN_one(modulus));
    assert(BN_lshift(modulus, modulus, 128));
    assert(BN_sub(expected_bn, modulus, oracle));
    uint8_t expected[sizeof(actual)];
    assert(BN_bn2lebinpad(expected_bn, expected, sizeof(expected)) ==
        (int)sizeof(expected));
    assert(memcmp(actual, expected, sizeof(actual)) == 0);
    BN_free(expected_bn);
    BN_free(modulus);
    BN_free(oracle);

    const AVPCCUnit zero[2] = { 0, 0 };
    assert(ccn_neg(2, actual, zero) == 1);
    assert(actual[0] == 0 && actual[1] == 0);
}

static void test_swap_and_trailing_zeros(void) {
    AVPCCUnit value[3] = {
        UINT64_C(0x0011223344556677),
        UINT64_C(0x8899aabbccddeeff),
        UINT64_C(0x0123456789abcdef),
    };
    ccn_swap(3, value);
    assert(value[0] == UINT64_C(0xefcdab8967452301));
    assert(value[1] == UINT64_C(0xffeeddccbbaa9988));
    assert(value[2] == UINT64_C(0x7766554433221100));

    const AVPCCUnit trailing[] = { 0, UINT64_C(1) << 13, 0 };
    assert(ccn_trailing_zeros(3, trailing) == 77);
    const AVPCCUnit zero[] = { 0, 0, 0 };
    assert(ccn_trailing_zeros(3, zero) == 0);
}

static uint64_t next_word(uint64_t *state) {
    uint64_t value = *state;
    value ^= value << 13;
    value ^= value >> 7;
    value ^= value << 17;
    *state = value;
    return value;
}

static void test_randomized_openssl_oracle(void) {
    uint64_t random_state = UINT64_C(0x6f70656e626f6f74);
    for (size_t iteration = 0; iteration < 512; ++iteration) {
        AVPCCUnit left[4];
        AVPCCUnit right[4];
        for (size_t index = 0; index < 4; ++index) {
            left[index] = next_word(&random_state);
            right[index] = next_word(&random_state);
        }
        if ((iteration & 7) == 0) {
            left[iteration & 3] = 0;
        }

        BIGNUM *left_bn = bn_from_limbs(left, 4);
        BIGNUM *right_bn = bn_from_limbs(right, 4);
        const int oracle_compare = BN_cmp(left_bn, right_bn);
        const int expected_compare = (oracle_compare > 0) - (oracle_compare < 0);
        assert(ccn_cmp_asm(4, left, right) == expected_compare);
        assert(ccn_bitlen_public_value(4, left) ==
            (size_t)BN_num_bits(left_bn));

        size_t expected_trailing = 0;
        if (!BN_is_zero(left_bn)) {
            while (!BN_is_bit_set(left_bn, (int)expected_trailing)) {
                ++expected_trailing;
            }
        }
        assert(ccn_trailing_zeros(4, left) == expected_trailing);
        BN_free(right_bn);
        BN_free(left_bn);
    }
}

int main(void) {
    test_shape_and_comparison();
    test_bit_lengths_against_openssl();
    test_read_uint_against_openssl();
    test_negation_against_openssl();
    test_swap_and_trailing_zeros();
    test_randomized_openssl_oracle();
    return 0;
}
