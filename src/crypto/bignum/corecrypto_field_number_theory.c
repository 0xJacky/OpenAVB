/*
 * Modular inversion and square roots use the binary extended-GCD and
 * Tonelli-Shanks algorithms published by LibTomMath.
 */
#include "corecrypto_field_private.h"

#include <string.h>


static void field_set_u64(size_t count, uint64_t *value, uint64_t scalar) {
    memset(value, 0, count * sizeof(*value));
    value[0] = scalar;
}

static void field_sub_u64(size_t count, uint64_t *value, uint64_t scalar) {
    uint64_t borrow = scalar;
    for (size_t index = 0; index < count; ++index) {
        const uint64_t original = value[index];
        value[index] -= borrow;
        borrow = original < borrow;
    }
}

static void euler_exponent(const AVPCCZP *zp, uint64_t *exponent) {
    memcpy(exponent, zp->prime, zp->n * sizeof(*exponent));
    field_sub_u64(zp->n, exponent, 1);
    avp_field_shift_right_one(zp->n, exponent, 0);
}

static void field_add_u64(size_t count, uint64_t *value, uint64_t scalar) {
    uint64_t carry = scalar;
    for (size_t index = 0; index < count; ++index) {
        const uint64_t original = value[index];
        value[index] += carry;
        carry = value[index] < original;
    }
}

static void half_coefficient(
    size_t count,
    uint64_t *coefficient,
    const uint64_t *modulus
) {
    uint64_t high = 0;
    if ((coefficient[0] & 1u) != 0) {
        high = avp_field_add(count, coefficient, coefficient, modulus);
    }
    avp_field_shift_right_one(count, coefficient, high);
}

int avp_field_inverse_odd(
    size_t count,
    uint64_t *result,
    const uint64_t *source,
    const uint64_t *modulus
) {
    if (count == 0 || count > AVP_CC_FIELD_MAX_N ||
        (modulus[0] & 1u) == 0 || avp_field_is_zero(count, source)) {
        return -1;
    }
    uint64_t u[AVP_CC_FIELD_MAX_N];
    uint64_t v[AVP_CC_FIELD_MAX_N];
    uint64_t x1[AVP_CC_FIELD_MAX_N] = {1};
    uint64_t x2[AVP_CC_FIELD_MAX_N] = {0};
    avp_field_reduce(count, u, count, source, modulus);
    memcpy(v, modulus, count * sizeof(*v));

    while (!avp_field_is_one(count, u) && !avp_field_is_one(count, v)) {
        if (avp_field_is_zero(count, u) || avp_field_is_zero(count, v)) {
            return -1;
        }
        while ((u[0] & 1u) == 0) {
            avp_field_shift_right_one(count, u, 0);
            half_coefficient(count, x1, modulus);
        }
        while ((v[0] & 1u) == 0) {
            avp_field_shift_right_one(count, v, 0);
            half_coefficient(count, x2, modulus);
        }
        if (avp_field_compare(count, u, v) >= 0) {
            (void)avp_field_sub(count, u, u, v);
            avp_field_sub_mod(count, x1, x1, x2, modulus);
        } else {
            (void)avp_field_sub(count, v, v, u);
            avp_field_sub_mod(count, x2, x2, x1, modulus);
        }
    }
    memcpy(
        result,
        avp_field_is_one(count, u) ? x1 : x2,
        count * sizeof(*result)
    );
    return 0;
}

int AVP_CC_FIELD_PUBLIC cczp_power_fast_ws(
    AVPCCWorkspace workspace,
    const AVPCCZP *zp,
    AVPCCUnit *result,
    const AVPCCUnit *base,
    const AVPCCUnit *exponent
) {
    if (zp == NULL || zp->functions == NULL || zp->n == 0 ||
        zp->n > AVP_CC_FIELD_MAX_N) {
        return -1;
    }
    uint64_t accumulator[AVP_CC_FIELD_MAX_N] = {0};
    uint64_t power[AVP_CC_FIELD_MAX_N];
    uint64_t temporary[AVP_CC_FIELD_MAX_N];
    uint64_t one[AVP_CC_FIELD_MAX_N] = {1};
    zp->functions->to_representation(workspace, zp, accumulator, one);
    memcpy(power, base, zp->n * sizeof(*power));
    const size_t bits = avp_field_bit_length(zp->n, exponent);
    for (size_t bit = 0; bit < bits; ++bit) {
        if (((exponent[bit / 64] >> (bit & 63)) & 1u) != 0) {
            zp->functions->multiply(
                workspace,
                zp,
                temporary,
                accumulator,
                power
            );
            memcpy(accumulator, temporary, zp->n * sizeof(*accumulator));
        }
        if (bit + 1 != bits) {
            zp->functions->square(workspace, zp, temporary, power);
            memcpy(power, temporary, zp->n * sizeof(*power));
        }
    }
    memcpy(result, accumulator, zp->n * sizeof(*result));
    return 0;
}

int AVP_CC_FIELD_PUBLIC cczp_inv_default_ws(
    AVPCCWorkspace workspace,
    const AVPCCZP *zp,
    AVPCCUnit *result,
    const AVPCCUnit *source
) {
    uint64_t standard[AVP_CC_FIELD_MAX_N];
    uint64_t inverse[AVP_CC_FIELD_MAX_N];
    zp->functions->from_representation(workspace, zp, standard, source);
    if (avp_field_inverse_odd(zp->n, inverse, standard, zp->prime) != 0) {
        return -1;
    }
    zp->functions->to_representation(workspace, zp, result, inverse);
    return 0;
}

int AVP_CC_FIELD_PUBLIC cczp_is_quadratic_residue_ws(
    AVPCCWorkspace workspace,
    const AVPCCZP *zp,
    const AVPCCUnit *source
) {
    uint64_t standard[AVP_CC_FIELD_MAX_N];
    uint64_t exponent[AVP_CC_FIELD_MAX_N];
    uint64_t symbol[AVP_CC_FIELD_MAX_N];
    zp->functions->from_representation(workspace, zp, standard, source);
    if (avp_field_is_zero(zp->n, standard)) {
        return 1;
    }
    euler_exponent(zp, exponent);
    avp_field_pow_mod(zp->n, symbol, standard, exponent, zp->prime);
    return avp_field_is_one(zp->n, symbol) ? 1 : 0;
}

static int tonelli_shanks(
    const AVPCCZP *zp,
    uint64_t *result,
    const uint64_t *source
) {
    const size_t count = zp->n;
    if (avp_field_is_zero(count, source)) {
        memset(result, 0, count * sizeof(*result));
        return 0;
    }

    uint64_t legendre_exponent[AVP_CC_FIELD_MAX_N];
    uint64_t legendre[AVP_CC_FIELD_MAX_N];
    euler_exponent(zp, legendre_exponent);
    avp_field_pow_mod(count, legendre, source, legendre_exponent, zp->prime);
    if (!avp_field_is_one(count, legendre)) {
        return -1;
    }

    uint64_t q[AVP_CC_FIELD_MAX_N];
    memcpy(q, zp->prime, count * sizeof(*q));
    field_sub_u64(count, q, 1);
    unsigned int s = 0;
    while ((q[0] & 1u) == 0) {
        avp_field_shift_right_one(count, q, 0);
        ++s;
    }

    uint64_t z[AVP_CC_FIELD_MAX_N];
    uint64_t candidate_symbol[AVP_CC_FIELD_MAX_N];
    field_set_u64(count, z, 2);
    for (;;) {
        avp_field_pow_mod(
            count,
            candidate_symbol,
            z,
            legendre_exponent,
            zp->prime
        );
        if (!avp_field_is_one(count, candidate_symbol)) {
            break;
        }
        field_add_u64(count, z, 1);
    }

    uint64_t c[AVP_CC_FIELD_MAX_N];
    uint64_t x[AVP_CC_FIELD_MAX_N];
    uint64_t t[AVP_CC_FIELD_MAX_N];
    uint64_t exponent[AVP_CC_FIELD_MAX_N];
    avp_field_pow_mod(count, c, z, q, zp->prime);
    memcpy(exponent, q, count * sizeof(*exponent));
    field_add_u64(count, exponent, 1);
    avp_field_shift_right_one(count, exponent, 0);
    avp_field_pow_mod(count, x, source, exponent, zp->prime);
    avp_field_pow_mod(count, t, source, q, zp->prime);
    unsigned int m = s;

    while (!avp_field_is_one(count, t)) {
        uint64_t squared[AVP_CC_FIELD_MAX_N];
        memcpy(squared, t, count * sizeof(*squared));
        unsigned int i = 0;
        for (i = 1; i < m; ++i) {
            avp_field_mul_mod(count, squared, squared, squared, zp->prime);
            if (avp_field_is_one(count, squared)) {
                break;
            }
        }
        if (i == m) {
            return -1;
        }
        uint64_t b[AVP_CC_FIELD_MAX_N];
        uint64_t b_squared[AVP_CC_FIELD_MAX_N];
        memcpy(b, c, count * sizeof(*b));
        for (unsigned int power = 0; power < m - i - 1; ++power) {
            avp_field_mul_mod(count, b, b, b, zp->prime);
        }
        avp_field_mul_mod(count, x, x, b, zp->prime);
        avp_field_mul_mod(count, b_squared, b, b, zp->prime);
        avp_field_mul_mod(count, t, t, b_squared, zp->prime);
        memcpy(c, b_squared, count * sizeof(*c));
        m = i;
    }
    memcpy(result, x, count * sizeof(*result));
    return 0;
}

int AVP_CC_FIELD_PUBLIC cczp_sqrt_default_ws(
    AVPCCWorkspace workspace,
    const AVPCCZP *zp,
    AVPCCUnit *result,
    const AVPCCUnit *source
) {
    uint64_t standard[AVP_CC_FIELD_MAX_N];
    uint64_t root[AVP_CC_FIELD_MAX_N];
    zp->functions->from_representation(workspace, zp, standard, source);
    if (tonelli_shanks(zp, root, standard) != 0) {
        return -1;
    }
    zp->functions->to_representation(workspace, zp, result, root);
    return 0;
}

int AVP_CC_FIELD_PUBLIC cczp_sqrt_tonelli_shanks_precomp_ws(
    AVPCCWorkspace workspace,
    const AVPCCZP *zp,
    AVPCCUnit *result,
    const AVPCCUnit *source,
    uint64_t c1,
    const AVPCCUnit *c3,
    const AVPCCUnit *c5
) {
    (void)c1;
    (void)c3;
    (void)c5;
    return cczp_sqrt_default_ws(workspace, zp, result, source);
}

void AVP_CC_FIELD_PUBLIC cczp_cond_negate(
    const AVPCCZP *zp,
    AVPCCUnit selector,
    AVPCCUnit *result,
    const AVPCCUnit *source
) {
    if ((selector & 1u) == 0 || avp_field_is_zero(zp->n, source)) {
        memcpy(result, source, zp->n * sizeof(*result));
        return;
    }
    (void)avp_field_sub(zp->n, result, zp->prime, source);
}

void AVP_CC_FIELD_PUBLIC ccn_gcd_approximate(
    AVPCCSize count,
    const AVPCCUnit *left,
    AVPCCUnit *left_approximation,
    const AVPCCUnit *right,
    AVPCCUnit *right_approximation
) {
    if (count == 0) {
        __builtin_trap();
    }
    const size_t left_bits = avp_field_bit_length(count, left);
    const size_t right_bits = avp_field_bit_length(count, right);
    const size_t top_bit = left_bits > right_bits ? left_bits : right_bits;
    uint64_t left_value = left[0] & UINT64_C(0x7fffffff);
    uint64_t right_value = right[0] & UINT64_C(0x7fffffff);
    if (top_bit > 31) {
        const size_t shift = top_bit > 64 ? top_bit - 64 : 0;
        const size_t limb = shift / 64;
        const size_t offset = shift & 63;
        left_value = left[limb] >> offset;
        right_value = right[limb] >> offset;
        if (offset != 0 && limb + 1 < count) {
            left_value |= left[limb + 1] << (64 - offset);
            right_value |= right[limb + 1] << (64 - offset);
        }
        left_value = (left_value & ~UINT64_C(0x7fffffff)) |
                     (left[0] & UINT64_C(0x7fffffff));
        right_value = (right_value & ~UINT64_C(0x7fffffff)) |
                      (right[0] & UINT64_C(0x7fffffff));
    }
    *left_approximation = left_value;
    *right_approximation = right_value;
}

static uint64_t multiply_scalar(
    size_t count,
    uint64_t *result,
    const uint64_t *value,
    uint64_t scalar
) {
    __uint128_t carry = 0;
    for (size_t index = 0; index < count; ++index) {
        const __uint128_t product = (__uint128_t)value[index] * scalar + carry;
        result[index] = (uint64_t)product;
        carry = product >> 64;
    }
    return (uint64_t)carry;
}

static uint64_t signed_linear_shift_31(
    size_t count,
    uint64_t *result,
    const uint64_t *left,
    uint64_t left_factor,
    const uint64_t *right,
    uint64_t right_factor
) {
    const int64_t signed_left = (int64_t)left_factor;
    const int64_t signed_right = (int64_t)right_factor;
    const uint64_t left_magnitude =
        signed_left < 0 ? 0u - left_factor : left_factor;
    const uint64_t right_magnitude =
        signed_right < 0 ? 0u - right_factor : right_factor;
    uint64_t left_term[AVP_CC_FIELD_MAX_N + 1];
    uint64_t right_term[AVP_CC_FIELD_MAX_N + 1];
    left_term[count] = multiply_scalar(count, left_term, left, left_magnitude);
    right_term[count] =
        multiply_scalar(count, right_term, right, right_magnitude);

    uint64_t combined[AVP_CC_FIELD_MAX_N + 1];
    uint64_t negative;
    if ((signed_left < 0) == (signed_right < 0)) {
        (void)avp_field_add(count + 1, combined, left_term, right_term);
        negative = signed_left < 0;
    } else if (avp_field_compare(count + 1, left_term, right_term) >= 0) {
        (void)avp_field_sub(count + 1, combined, left_term, right_term);
        negative = signed_left < 0;
    } else {
        (void)avp_field_sub(count + 1, combined, right_term, left_term);
        negative = signed_right < 0;
    }
    for (size_t index = 0; index < count; ++index) {
        result[index] = (combined[index] >> 31) |
                        (combined[index + 1] << 33);
    }
    return negative;
}

void AVP_CC_FIELD_PUBLIC cczp_inv_update_redc_ws(
    AVPCCWorkspace workspace,
    const AVPCCZP *zp,
    AVPCCUnit *result,
    const AVPCCUnit *left,
    AVPCCUnit left_factor,
    const AVPCCUnit *right,
    AVPCCUnit right_factor
) {
    (void)workspace;
    uint64_t magnitude[AVP_CC_FIELD_MAX_N];
    const uint64_t negative = signed_linear_shift_31(
        zp->n,
        magnitude,
        left,
        left_factor,
        right,
        right_factor
    );
    avp_field_reduce(zp->n, result, zp->n, magnitude, zp->prime);
    cczp_cond_negate(zp, negative, result, result);
}

AVPCCUnit AVP_CC_FIELD_PUBLIC ccn_gcd_update_ws(
    AVPCCWorkspace workspace,
    AVPCCSize count,
    AVPCCUnit *result,
    const AVPCCUnit *left,
    AVPCCUnit left_factor,
    const AVPCCUnit *right,
    AVPCCUnit right_factor
) {
    (void)workspace;
    if (count > AVP_CC_FIELD_MAX_N) {
        __builtin_trap();
    }
    return signed_linear_shift_31(
        count,
        result,
        left,
        left_factor,
        right,
        right_factor
    );
}
