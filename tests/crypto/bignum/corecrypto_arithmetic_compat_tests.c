#include "avp/crypto/bignum/corecrypto_arithmetic_compat.h"

#include <assert.h>
#include <openssl/bn.h>
#include <stdint.h>
#include <string.h>


static uint64_t random_state = UINT64_C(0x63636e6172697468);

static uint64_t next_word(void) {
    random_state ^= random_state << 13;
    random_state ^= random_state >> 7;
    random_state ^= random_state << 17;
    return random_state;
}

static __uint128_t joined(const uint64_t value[2]) {
    return ((__uint128_t)value[1] << 64) | value[0];
}

static void assert_product_matches_openssl(
    const uint64_t left[2],
    const uint64_t right[2],
    const uint64_t result[4]
) {
    BIGNUM *left_number = BN_lebin2bn(
        (const unsigned char *)left,
        (int)sizeof(uint64_t[2]),
        NULL
    );
    BIGNUM *right_number = BN_lebin2bn(
        (const unsigned char *)right,
        (int)sizeof(uint64_t[2]),
        NULL
    );
    BIGNUM *product = BN_new();
    BN_CTX *context = BN_CTX_new();
    unsigned char expected[sizeof(uint64_t[4])] = {0};
    assert(left_number != NULL);
    assert(right_number != NULL);
    assert(product != NULL);
    assert(context != NULL);
    assert(BN_mul(product, left_number, right_number, context) == 1);
    assert(BN_bn2lebinpad(product, expected, (int)sizeof(expected)) ==
           (int)sizeof(expected));
    assert(memcmp(result, expected, sizeof(expected)) == 0);
    BN_CTX_free(context);
    BN_free(product);
    BN_free(right_number);
    BN_free(left_number);
}

static void test_two_limb_oracle(void) {
    for (size_t iteration = 0; iteration < 4096; ++iteration) {
        uint64_t left[2] = {next_word(), next_word()};
        uint64_t right[2] = {next_word(), next_word()};
        uint64_t result[4] = {0};
        const __uint128_t left_value = joined(left);
        const __uint128_t right_value = joined(right);

        ccn_mul(2, result, left, right);
        assert_product_matches_openssl(left, right, result);
        memset(result, 0, sizeof(result));
        ccn_mul_ws(0, 2, result, left, right);
        assert_product_matches_openssl(left, right, result);

        const uint64_t add_carry = ccn_add_asm(2, result, left, right);
        const __uint128_t sum = left_value + right_value;
        assert(joined(result) == sum);
        assert(add_carry == (sum < left_value));
        assert(ccn_add_ws(0, 2, result, left, right) == add_carry);

        const uint64_t borrow = ccn_sub_asm(2, result, left, right);
        assert(joined(result) == left_value - right_value);
        assert(borrow == (left_value < right_value));
        assert(ccn_sub_ws(0, 2, result, left, right) == borrow);

        const uint64_t addend = next_word();
        assert(ccn_add1_ws(0, 2, result, left, addend) ==
               (left_value + addend < left_value));
        assert(joined(result) == left_value + addend);
        assert(ccn_sub1(2, result, left, addend) == (left_value < addend));
        assert(joined(result) == left_value - addend);

        const uint64_t multiplier = next_word();
        const __uint128_t low_product = (__uint128_t)left[0] * multiplier;
        const __uint128_t high_product =
            (__uint128_t)left[1] * multiplier + (uint64_t)(low_product >> 64);
        const uint64_t carry = ccn_mul1_asm(2, result, left, multiplier);
        assert(result[0] == (uint64_t)low_product);
        assert(result[1] == (uint64_t)high_product);
        assert(carry == (uint64_t)(high_product >> 64));
        assert(ccn_mul1(2, result, left, multiplier) == carry);

        uint64_t accumulated[2] = {right[0], right[1]};
        const __uint128_t expected_accumulated =
            right_value + left_value * multiplier;
        (void)ccn_addmul1_asm(2, accumulated, left, multiplier);
        assert(joined(accumulated) == expected_accumulated);
        accumulated[0] = right[0];
        accumulated[1] = right[1];
        (void)ccn_addmul1(2, accumulated, left, multiplier);
        assert(joined(accumulated) == expected_accumulated);

        const size_t shift = 1 + next_word() % 63;
        ccn_shift_right(2, result, left, shift);
        assert(joined(result) == left_value >> shift);
        ccn_shift_left(2, result, left, shift);
        assert(joined(result) == left_value << shift);
    }
}

static void test_multiplication_and_conditional_operations(void) {
    const uint64_t left[] = {UINT64_MAX, 2, 3};
    const uint64_t right[] = {5, 7, 11};
    uint64_t result[6];
    ccn_mul(3, result, left, right);
    const uint64_t expected[] = {
        UINT64_C(0xfffffffffffffffb), UINT64_C(7), UINT64_C(25),
        UINT64_C(54), UINT64_C(33), UINT64_C(0),
    };
    assert(memcmp(result, expected, sizeof(expected)) == 0);
    memset(result, 0, sizeof(result));
    ccn_mul_ws(0, 3, result, left, right);
    assert(memcmp(result, expected, sizeof(expected)) == 0);

    ccn_mux(3, 1, result, left, right);
    assert(memcmp(result, left, sizeof(left)) == 0);
    ccn_mux(3, 0, result, left, right);
    assert(memcmp(result, right, sizeof(right)) == 0);
    assert(ccn_cond_add(3, 0, result, left, right) == 0);
    assert(memcmp(result, left, sizeof(left)) == 0);
    (void)ccn_cond_add(3, 1, result, left, right);
    uint64_t ordinary[3];
    (void)ccn_add_asm(3, ordinary, left, right);
    assert(memcmp(result, ordinary, sizeof(ordinary)) == 0);
    assert(ccn_cond_sub(3, 0, result, left, right) == 0);
    assert(memcmp(result, left, sizeof(left)) == 0);
    (void)ccn_cond_sub(3, 1, result, left, right);
    (void)ccn_sub_asm(3, ordinary, left, right);
    assert(memcmp(result, ordinary, sizeof(ordinary)) == 0);
    ccn_cond_neg(3, 0, result, left);
    assert(memcmp(result, left, sizeof(left)) == 0);
    ccn_cond_neg(3, 1, result, left);
    uint64_t zero[3] = {0};
    (void)ccn_sub_asm(3, ordinary, zero, left);
    assert(memcmp(result, ordinary, sizeof(ordinary)) == 0);
}

static void test_extended_limb_operations(void) {
    ccn_mux_seed_mask_state(UINT64_C(0x0123456789abcdef));
    uint64_t expected_mask = UINT64_C(0x0123456789abcdef);
    expected_mask ^= expected_mask << 13;
    expected_mask ^= expected_mask >> 7;
    expected_mask ^= expected_mask << 17;
    assert(ccn_mux_next_mask() == expected_mask);

    const uint64_t source[] = {
        UINT64_C(0x0123456789abcdef),
        UINT64_C(0xfedcba9876543210),
        UINT64_C(0x1122334455667788),
        UINT64_C(0x99aabbccddeeff00),
    };
    uint64_t result[4] = {0};
    ccn_shift_right_multi(4, result, source, 68);
    assert(result[0] == ((source[1] >> 4) | (source[2] << 60)));
    assert(result[1] == ((source[2] >> 4) | (source[3] << 60)));
    assert(result[2] == source[3] >> 4);
    assert(result[3] == 0);
    ccn_shift_right_multi(4, result, source, 256);
    assert(result[0] == 0 && result[1] == 0 && result[2] == 0 &&
           result[3] == 0);

    const uint64_t right[] = {UINT64_MAX, UINT64_C(4)};
    assert(ccn_subn(4, result, source, 2, right) == 0);
    uint64_t extended_right[] = {right[0], right[1], 0, 0};
    uint64_t expected[4];
    assert(ccn_sub_asm(4, expected, source, extended_right) == 0);
    assert(memcmp(result, expected, sizeof(expected)) == 0);

    assert(ccn_add1_asm(4, result, source, UINT64_MAX) == 0);
    assert(ccn_add1_ws(0, 4, expected, source, UINT64_MAX) == 0);
    assert(memcmp(result, expected, sizeof(expected)) == 0);
    assert(ccn_add1_asm(0, result, source, UINT64_C(77)) == 77);
}

int main(void) {
    test_two_limb_oracle();
    test_multiplication_and_conditional_operations();
    test_extended_limb_operations();
    return 0;
}
