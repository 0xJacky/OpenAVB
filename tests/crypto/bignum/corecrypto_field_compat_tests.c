#include "avp/crypto/bignum/corecrypto_field_compat.h"

#include <assert.h>
#include <openssl/bn.h>
#include <stdint.h>
#include <string.h>


enum {
    TEST_N = 2,
};

typedef struct {
    AVPCCSize n;
    AVPCCUnit bit_length;
    const AVPCCZPFunctions *functions;
    AVPCCUnit storage[TEST_N * 2 + 1];
} TestFieldContext;

static BIGNUM *bn_from_limbs(size_t count, const uint64_t *limbs) {
    return BN_lebin2bn(
        (const unsigned char *)limbs,
        (int)(count * sizeof(*limbs)),
        NULL
    );
}

static void assert_bn_limbs(
    const BIGNUM *expected,
    size_t count,
    const uint64_t *actual
) {
    uint64_t encoded[AVP_CC_FIELD_MAX_N] = {0};
    assert(BN_bn2lebinpad(expected, (unsigned char *)encoded,
                         (int)(count * sizeof(*encoded))) >= 0);
    assert(memcmp(encoded, actual, count * sizeof(*actual)) == 0);
}

static uint64_t random_state = UINT64_C(0x9e3779b97f4a7c15);

static uint64_t next_random(void) {
    random_state ^= random_state >> 12;
    random_state ^= random_state << 25;
    random_state ^= random_state >> 27;
    return random_state * UINT64_C(0x2545f4914f6cdd1d);
}

static void test_openssl_oracle_rounds(void) {
    TestFieldContext context = {
        .n = TEST_N,
        .storage = {UINT64_MAX, UINT64_C(0x7fffffffffffffff)},
    };
    AVPCCZP *zp = (AVPCCZP *)&context;
    assert(cczp_init_ws(NULL, zp) == 0);
    BN_CTX *bn_context = BN_CTX_new();
    BIGNUM *p = bn_from_limbs(TEST_N, zp->prime);
    BIGNUM *expected = BN_new();
    assert(bn_context != NULL && p != NULL && expected != NULL);

    for (unsigned int round = 0; round < 64; ++round) {
        uint64_t left[TEST_N] = {next_random(), next_random() >> 1};
        uint64_t right[TEST_N] = {next_random(), next_random() >> 1};
        uint64_t result[TEST_N];
        BIGNUM *a = bn_from_limbs(TEST_N, left);
        BIGNUM *b = bn_from_limbs(TEST_N, right);
        assert(a != NULL && b != NULL);

        cczp_add_default_ws(NULL, zp, result, left, right);
        assert(BN_mod_add(expected, a, b, p, bn_context));
        assert_bn_limbs(expected, TEST_N, result);
        cczp_sub_default_ws(NULL, zp, result, left, right);
        assert(BN_mod_sub(expected, a, b, p, bn_context));
        assert_bn_limbs(expected, TEST_N, result);
        cczp_mul_default_ws(NULL, zp, result, left, right);
        assert(BN_mod_mul(expected, a, b, p, bn_context));
        assert_bn_limbs(expected, TEST_N, result);

        const uint64_t exponent[TEST_N] = {next_random() & 0xffffu, 0};
        BIGNUM *e = bn_from_limbs(TEST_N, exponent);
        assert(cczp_power_fast_ws(NULL, zp, result, left, exponent) == 0);
        assert(BN_mod_exp(expected, a, e, p, bn_context));
        assert_bn_limbs(expected, TEST_N, result);
        BN_free(e);
        BN_free(b);
        BN_free(a);
    }
    BN_free(expected);
    BN_free(p);
    BN_CTX_free(bn_context);
}

static void test_default_field(void) {
    TestFieldContext context = {
        .n = TEST_N,
        .storage = {UINT64_MAX, UINT64_C(0x7fffffffffffffff)},
    };
    AVPCCZP *zp = (AVPCCZP *)&context;
    assert(cczp_init_ws(NULL, zp) == 0);
    assert(zp->bit_length == 127);

    const uint64_t left[TEST_N] = {
        UINT64_C(0xfedcba9876543210), UINT64_C(0x123456789abcdef0),
    };
    const uint64_t right[TEST_N] = {
        UINT64_C(0x1122334455667788), UINT64_C(0x0102030405060708),
    };
    uint64_t result[TEST_N];
    BN_CTX *bn_context = BN_CTX_new();
    BIGNUM *p = bn_from_limbs(TEST_N, zp->prime);
    BIGNUM *a = bn_from_limbs(TEST_N, left);
    BIGNUM *b = bn_from_limbs(TEST_N, right);
    BIGNUM *expected = BN_new();
    assert(bn_context != NULL && p != NULL && a != NULL && b != NULL &&
           expected != NULL);

    cczp_add_default_ws(NULL, zp, result, left, right);
    assert(BN_mod_add(expected, a, b, p, bn_context));
    assert_bn_limbs(expected, TEST_N, result);
    cczp_sub_default_ws(NULL, zp, result, left, right);
    assert(BN_mod_sub(expected, a, b, p, bn_context));
    assert_bn_limbs(expected, TEST_N, result);
    cczp_mul_default_ws(NULL, zp, result, left, right);
    assert(BN_mod_mul(expected, a, b, p, bn_context));
    assert_bn_limbs(expected, TEST_N, result);
    cczp_sqr_default_ws(NULL, zp, result, left);
    assert(BN_mod_sqr(expected, a, p, bn_context));
    assert_bn_limbs(expected, TEST_N, result);

    const uint64_t exponent[TEST_N] = {UINT64_C(0x10001), 0};
    assert(cczp_power_fast_ws(NULL, zp, result, left, exponent) == 0);
    BIGNUM *e = bn_from_limbs(TEST_N, exponent);
    assert(BN_mod_exp(expected, a, e, p, bn_context));
    assert_bn_limbs(expected, TEST_N, result);

    assert(cczp_inv_default_ws(NULL, zp, result, left) == 0);
    BIGNUM *inverse = BN_mod_inverse(NULL, a, p, bn_context);
    assert(inverse != NULL);
    assert_bn_limbs(inverse, TEST_N, result);
    uint64_t product[TEST_N];
    cczp_mul_default_ws(NULL, zp, product, left, result);
    assert(product[0] == 1 && product[1] == 0);

    uint64_t square[TEST_N];
    cczp_sqr_default_ws(NULL, zp, square, right);
    assert(cczp_is_quadratic_residue_ws(NULL, zp, square) == 1);
    assert(cczp_sqrt_default_ws(NULL, zp, result, square) == 0);
    cczp_sqr_default_ws(NULL, zp, product, result);
    assert(memcmp(product, square, sizeof(square)) == 0);
    assert(cczp_sqrt_tonelli_shanks_precomp_ws(
               NULL, zp, result, square, 0, NULL, NULL) == 0);

    uint64_t negated[TEST_N];
    cczp_cond_negate(zp, 1, negated, right);
    cczp_add_default_ws(NULL, zp, product, right, negated);
    assert(product[0] == 0 && product[1] == 0);
    cczp_cond_negate(zp, 0, negated, right);
    assert(memcmp(negated, right, sizeof(negated)) == 0);

    const uint64_t wide[TEST_N * 2] = {
        UINT64_C(0x0123456789abcdef), UINT64_C(0xfedcba9876543210),
        UINT64_C(0x1111111111111111), UINT64_C(0x2222222222222222),
    };
    cczp_modn_ws(NULL, zp, result, TEST_N * 2, wide);
    BIGNUM *wide_bn = bn_from_limbs(TEST_N * 2, wide);
    assert(BN_mod(expected, wide_bn, p, bn_context));
    assert_bn_limbs(expected, TEST_N, result);
    cczp_mod_default_ws(NULL, zp, result, wide);
    assert_bn_limbs(expected, TEST_N, result);

    BN_free(wide_bn);
    BN_free(inverse);
    BN_free(e);
    BN_free(expected);
    BN_free(b);
    BN_free(a);
    BN_free(p);
    BN_CTX_free(bn_context);
}

static void test_montgomery_field(void) {
    TestFieldContext context = {0};
    AVPCCZP *zp = (AVPCCZP *)&context;
    const uint64_t prime[TEST_N] = {
        UINT64_MAX, UINT64_C(0x7fffffffffffffff),
    };
    assert(cczp_mm_init_ws(NULL, zp, TEST_N, prime) == 0);
    assert(prime[0] * context.storage[TEST_N] == UINT64_MAX);
    const uint64_t left[TEST_N] = {123456789, 1};
    const uint64_t right[TEST_N] = {987654321, 2};
    uint64_t left_mont[TEST_N], right_mont[TEST_N], result_mont[TEST_N];
    uint64_t result[TEST_N];
    cczp_mm_to_ws(NULL, zp, left_mont, left);
    cczp_mm_to_ws(NULL, zp, right_mont, right);
    cczp_mm_mul_ws(NULL, zp, result_mont, left_mont, right_mont);
    cczp_mm_from_ws(NULL, zp, result, result_mont);

    BN_CTX *bn_context = BN_CTX_new();
    BIGNUM *p = bn_from_limbs(TEST_N, prime);
    BIGNUM *a = bn_from_limbs(TEST_N, left);
    BIGNUM *b = bn_from_limbs(TEST_N, right);
    BIGNUM *expected = BN_new();
    assert(BN_mod_mul(expected, a, b, p, bn_context));
    assert_bn_limbs(expected, TEST_N, result);

    BIGNUM *radix = BN_new();
    BIGNUM *r2 = BN_new();
    assert(BN_one(radix));
    assert(BN_lshift(radix, radix, TEST_N * 64));
    assert(BN_mod_sqr(r2, radix, p, bn_context));
    assert_bn_limbs(r2, TEST_N, context.storage + TEST_N + 1);

    BIGNUM *left_mont_bn = bn_from_limbs(TEST_N, left_mont);
    BIGNUM *right_mont_bn = bn_from_limbs(TEST_N, right_mont);
    BIGNUM *wide_product = BN_new();
    uint64_t temporary[TEST_N * 2];
    assert(BN_mul(wide_product, left_mont_bn, right_mont_bn, bn_context));
    assert(BN_bn2lebinpad(wide_product, (unsigned char *)temporary,
                         sizeof(temporary)) >= 0);
    cczp_mm_redc_ws(NULL, zp, result_mont, temporary);
    cczp_mm_from_ws(NULL, zp, result, result_mont);
    assert_bn_limbs(expected, TEST_N, result);

    const uint64_t wide[TEST_N * 2] = {
        UINT64_C(0xaaaaaaaaaaaaaaaa), UINT64_C(0xbbbbbbbbbbbbbbbb),
        UINT64_C(0xcccccccccccccccc), UINT64_C(0xdddddddddddddddd),
    };
    BIGNUM *wide_bn = bn_from_limbs(TEST_N * 2, wide);
    cczp_mm_mod_ws(NULL, zp, result_mont, wide);
    cczp_mm_from_ws(NULL, zp, result, result_mont);
    assert(BN_mod(expected, wide_bn, p, bn_context));
    assert_bn_limbs(expected, TEST_N, result);

    uint64_t inverse_mont[TEST_N];
    assert(cczp_inv_default_ws(NULL, zp, inverse_mont, left_mont) == 0);
    cczp_mm_mul_ws(NULL, zp, result_mont, left_mont, inverse_mont);
    cczp_mm_from_ws(NULL, zp, result, result_mont);
    assert(result[0] == 1 && result[1] == 0);

    BN_free(wide_bn);
    BN_free(wide_product);
    BN_free(right_mont_bn);
    BN_free(left_mont_bn);
    BN_free(r2);
    BN_free(radix);
    BN_free(expected);
    BN_free(b);
    BN_free(a);
    BN_free(p);
    BN_CTX_free(bn_context);
}

static void test_integer_primitives(void) {
    const uint64_t value[TEST_N] = {
        UINT64_C(0xfedcba9876543210), UINT64_C(0x123456789abcdef0),
    };
    uint64_t square[TEST_N * 2];
    ccn_sqr_ws(NULL, TEST_N, square, value);
    BN_CTX *context = BN_CTX_new();
    BIGNUM *input = bn_from_limbs(TEST_N, value);
    BIGNUM *expected = BN_new();
    assert(BN_sqr(expected, input, context));
    assert_bn_limbs(expected, TEST_N * 2, square);

    const uint64_t divisor[TEST_N] = {
        UINT64_C(0x1122334455667789), UINT64_C(0x0102030405060708),
    };
    uint64_t quotient[TEST_N * 2], remainder[TEST_N];
    ccn_divmod_ws(
        NULL,
        TEST_N * 2,
        square,
        TEST_N * 2,
        quotient,
        TEST_N,
        remainder,
        divisor
    );
    BIGNUM *dividend_bn = bn_from_limbs(TEST_N * 2, square);
    BIGNUM *divisor_bn = bn_from_limbs(TEST_N, divisor);
    BIGNUM *quotient_bn = BN_new();
    BIGNUM *remainder_bn = BN_new();
    assert(BN_div(quotient_bn, remainder_bn, dividend_bn, divisor_bn, context));
    assert_bn_limbs(quotient_bn, TEST_N * 2, quotient);
    assert_bn_limbs(remainder_bn, TEST_N, remainder);

    uint64_t left_approximation, right_approximation;
    ccn_gcd_approximate(
        1,
        value,
        &left_approximation,
        divisor,
        &right_approximation
    );
    assert(left_approximation == value[0]);
    assert(right_approximation == divisor[0]);

    const uint64_t left[1] = {100};
    const uint64_t right[1] = {30};
    uint64_t updated[1];
    assert(ccn_gcd_update_ws(NULL, 1, updated, left,
                             (uint64_t)-(INT64_C(2) << 31), right,
                             UINT64_C(5) << 31) == 1);
    assert(updated[0] == 50);

    BN_free(remainder_bn);
    BN_free(quotient_bn);
    BN_free(divisor_bn);
    BN_free(dividend_bn);
    BN_free(expected);
    BN_free(input);
    BN_CTX_free(context);
}

static void test_inverse_update(void) {
    TestFieldContext context = {
        .n = 1,
        .storage = {101},
    };
    AVPCCZP *zp = (AVPCCZP *)&context;
    assert(cczp_init_ws(NULL, zp) == 0);
    const uint64_t left[1] = {20};
    const uint64_t right[1] = {30};
    uint64_t result[1];
    cczp_inv_update_redc_ws(
        NULL,
        zp,
        result,
        left,
        (uint64_t)-(INT64_C(2) << 31),
        right,
        UINT64_C(5) << 31
    );
    assert(result[0] == 9);

    const uint64_t zero[1] = {0};
    assert(cczp_inv_default_ws(NULL, zp, result, zero) == -1);

    TestFieldContext invalid = {.n = 1, .storage = {100}};
    assert(cczp_init_ws(NULL, (AVPCCZP *)&invalid) == -1);
    assert(cczp_mm_init_ws(NULL, (AVPCCZP *)&invalid, 1, invalid.storage) ==
           -1);
}

int main(void) {
    test_openssl_oracle_rounds();
    test_default_field();
    test_montgomery_field();
    test_integer_primitives();
    test_inverse_update();
    return 0;
}
