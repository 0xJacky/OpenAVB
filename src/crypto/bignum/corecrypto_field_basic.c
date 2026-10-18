/*
 * Fixed-width reduction and field operations follow the bitwise division and
 * multiplication models used by LibTomMath. Storage remains caller-owned.
 */
#include "corecrypto_field_private.h"

#include <string.h>


int avp_field_compare(
    size_t count,
    const uint64_t *left,
    const uint64_t *right
) {
    for (size_t index = count; index != 0; --index) {
        if (left[index - 1] > right[index - 1]) {
            return 1;
        }
        if (left[index - 1] < right[index - 1]) {
            return -1;
        }
    }
    return 0;
}

uint64_t avp_field_add(
    size_t count,
    uint64_t *result,
    const uint64_t *left,
    const uint64_t *right
) {
    uint64_t carry = 0;
    for (size_t index = 0; index < count; ++index) {
        const uint64_t partial = left[index] + carry;
        const uint64_t carry_a = partial < left[index];
        const uint64_t sum = partial + right[index];
        const uint64_t carry_b = sum < partial;
        result[index] = sum;
        carry = carry_a | carry_b;
    }
    return carry;
}

uint64_t avp_field_sub(
    size_t count,
    uint64_t *result,
    const uint64_t *left,
    const uint64_t *right
) {
    uint64_t borrow = 0;
    for (size_t index = 0; index < count; ++index) {
        const uint64_t subtrahend = right[index] + borrow;
        const uint64_t overflow = subtrahend < right[index];
        const uint64_t difference = left[index] - subtrahend;
        borrow = overflow | (left[index] < subtrahend);
        result[index] = difference;
    }
    return borrow;
}

void avp_field_shift_right_one(
    size_t count,
    uint64_t *value,
    uint64_t high_bit
) {
    for (size_t index = count; index != 0; --index) {
        const uint64_t next = value[index - 1] & 1u;
        value[index - 1] = (value[index - 1] >> 1) | (high_bit << 63);
        high_bit = next;
    }
}

bool avp_field_is_zero(size_t count, const uint64_t *value) {
    uint64_t aggregate = 0;
    for (size_t index = 0; index < count; ++index) {
        aggregate |= value[index];
    }
    return aggregate == 0;
}

bool avp_field_is_one(size_t count, const uint64_t *value) {
    if (count == 0 || value[0] != 1) {
        return false;
    }
    return avp_field_is_zero(count - 1, value + 1);
}

size_t avp_field_bit_length(size_t count, const uint64_t *value) {
    while (count != 0 && value[count - 1] == 0) {
        --count;
    }
    return count == 0 ? 0
                      : (count - 1) * 64u +
                            (64u - (size_t)__builtin_clzll(value[count - 1]));
}

static uint64_t shift_left_bit(size_t count, uint64_t *value, uint64_t bit) {
    uint64_t carry = bit;
    for (size_t index = 0; index < count; ++index) {
        const uint64_t next = value[index] >> 63;
        value[index] = (value[index] << 1) | carry;
        carry = next;
    }
    return carry;
}

void avp_field_reduce(
    size_t modulus_count,
    uint64_t *result,
    size_t source_count,
    const uint64_t *source,
    const uint64_t *modulus
) {
    memset(result, 0, modulus_count * sizeof(*result));
    const size_t source_bits = avp_field_bit_length(source_count, source);
    for (size_t bit = source_bits; bit != 0; --bit) {
        const uint64_t source_bit =
            (source[(bit - 1) / 64] >> ((bit - 1) & 63)) & 1u;
        const uint64_t overflow =
            shift_left_bit(modulus_count, result, source_bit);
        if (overflow != 0 ||
            avp_field_compare(modulus_count, result, modulus) >= 0) {
            (void)avp_field_sub(modulus_count, result, result, modulus);
        }
    }
}

void avp_field_add_mod(
    size_t count,
    uint64_t *result,
    const uint64_t *left,
    const uint64_t *right,
    const uint64_t *modulus
) {
    uint64_t threshold[AVP_CC_FIELD_MAX_N];
    (void)avp_field_sub(count, threshold, modulus, right);
    if (avp_field_compare(count, left, threshold) >= 0) {
        (void)avp_field_sub(count, result, left, threshold);
    } else {
        (void)avp_field_add(count, result, left, right);
    }
}

void avp_field_sub_mod(
    size_t count,
    uint64_t *result,
    const uint64_t *left,
    const uint64_t *right,
    const uint64_t *modulus
) {
    if (avp_field_compare(count, left, right) >= 0) {
        (void)avp_field_sub(count, result, left, right);
        return;
    }
    uint64_t difference[AVP_CC_FIELD_MAX_N];
    (void)avp_field_sub(count, difference, right, left);
    (void)avp_field_sub(count, result, modulus, difference);
}

void avp_field_mul_mod(
    size_t count,
    uint64_t *result,
    const uint64_t *left,
    const uint64_t *right,
    const uint64_t *modulus
) {
    uint64_t accumulator[AVP_CC_FIELD_MAX_N] = {0};
    uint64_t doubled[AVP_CC_FIELD_MAX_N];
    memcpy(doubled, left, count * sizeof(*doubled));
    const size_t right_bits = avp_field_bit_length(count, right);
    for (size_t bit = 0; bit < right_bits; ++bit) {
        if (((right[bit / 64] >> (bit & 63)) & 1u) != 0) {
            avp_field_add_mod(
                count,
                accumulator,
                accumulator,
                doubled,
                modulus
            );
        }
        if (bit + 1 != right_bits) {
            avp_field_add_mod(count, doubled, doubled, doubled, modulus);
        }
    }
    memcpy(result, accumulator, count * sizeof(*result));
}

void avp_field_pow_mod(
    size_t count,
    uint64_t *result,
    const uint64_t *base,
    const uint64_t *exponent,
    const uint64_t *modulus
) {
    uint64_t accumulator[AVP_CC_FIELD_MAX_N] = {1};
    uint64_t power[AVP_CC_FIELD_MAX_N];
    uint64_t temporary[AVP_CC_FIELD_MAX_N];
    memcpy(power, base, count * sizeof(*power));
    const size_t exponent_bits = avp_field_bit_length(count, exponent);
    for (size_t bit = 0; bit < exponent_bits; ++bit) {
        if (((exponent[bit / 64] >> (bit & 63)) & 1u) != 0) {
            avp_field_mul_mod(count, temporary, accumulator, power, modulus);
            memcpy(accumulator, temporary, count * sizeof(*accumulator));
        }
        if (bit + 1 != exponent_bits) {
            avp_field_mul_mod(count, temporary, power, power, modulus);
            memcpy(power, temporary, count * sizeof(*power));
        }
    }
    memcpy(result, accumulator, count * sizeof(*result));
}

void AVP_CC_FIELD_PUBLIC ccn_sqr_ws(
    AVPCCWorkspace workspace,
    AVPCCSize count,
    AVPCCUnit *result_2n,
    const AVPCCUnit *source
) {
    (void)workspace;
    if (count > AVP_CC_FIELD_MAX_N) {
        __builtin_trap();
    }
    memset(result_2n, 0, count * 2u * sizeof(*result_2n));
    for (size_t left = 0; left < count; ++left) {
        __uint128_t carry = 0;
        for (size_t right = 0; right < count; ++right) {
            const size_t output = left + right;
            const __uint128_t product =
                (__uint128_t)source[left] * source[right] +
                result_2n[output] + carry;
            result_2n[output] = (uint64_t)product;
            carry = product >> 64;
        }
        result_2n[left + count] = (uint64_t)carry;
    }
}

void AVP_CC_FIELD_PUBLIC ccn_divmod_ws(
    AVPCCWorkspace workspace,
    AVPCCSize dividend_count,
    const AVPCCUnit *dividend,
    AVPCCSize quotient_count,
    AVPCCUnit *quotient,
    AVPCCSize divisor_count,
    AVPCCUnit *remainder,
    const AVPCCUnit *divisor
) {
    (void)workspace;
    if (divisor_count == 0 || divisor_count > AVP_CC_FIELD_MAX_N ||
        avp_field_is_zero(divisor_count, divisor)) {
        __builtin_trap();
    }
    if (quotient != NULL) {
        memset(quotient, 0, quotient_count * sizeof(*quotient));
    }
    memset(remainder, 0, divisor_count * sizeof(*remainder));
    const size_t bits = avp_field_bit_length(dividend_count, dividend);
    for (size_t bit = bits; bit != 0; --bit) {
        const uint64_t input =
            (dividend[(bit - 1) / 64] >> ((bit - 1) & 63)) & 1u;
        const uint64_t overflow =
            shift_left_bit(divisor_count, remainder, input);
        if (overflow != 0 ||
            avp_field_compare(divisor_count, remainder, divisor) >= 0) {
            (void)avp_field_sub(divisor_count, remainder, remainder, divisor);
            if (quotient != NULL && bit - 1 < quotient_count * 64u) {
                quotient[(bit - 1) / 64] |= UINT64_C(1) << ((bit - 1) & 63);
            }
        }
    }
}

void AVP_CC_FIELD_PUBLIC cczp_from_default_ws(
    AVPCCWorkspace workspace,
    const AVPCCZP *zp,
    AVPCCUnit *result,
    const AVPCCUnit *source
) {
    (void)workspace;
    memcpy(result, source, zp->n * sizeof(*result));
}

void AVP_CC_FIELD_PUBLIC cczp_to_default_ws(
    AVPCCWorkspace workspace,
    const AVPCCZP *zp,
    AVPCCUnit *result,
    const AVPCCUnit *source
) {
    (void)workspace;
    memcpy(result, source, zp->n * sizeof(*result));
}

void AVP_CC_FIELD_PUBLIC cczp_add_default_ws(
    AVPCCWorkspace workspace,
    const AVPCCZP *zp,
    AVPCCUnit *result,
    const AVPCCUnit *left,
    const AVPCCUnit *right
) {
    (void)workspace;
    avp_field_add_mod(zp->n, result, left, right, zp->prime);
}

void AVP_CC_FIELD_PUBLIC cczp_sub_default_ws(
    AVPCCWorkspace workspace,
    const AVPCCZP *zp,
    AVPCCUnit *result,
    const AVPCCUnit *left,
    const AVPCCUnit *right
) {
    (void)workspace;
    avp_field_sub_mod(zp->n, result, left, right, zp->prime);
}

int AVP_CC_FIELD_PUBLIC cczp_init_ws(AVPCCWorkspace workspace, AVPCCZP *zp) {
    (void)workspace;
    if (zp == NULL || zp->n == 0 || zp->n > AVP_CC_FIELD_MAX_N ||
        (zp->prime[0] & 1u) == 0) {
        return -1;
    }
    const size_t bit_length = avp_field_bit_length(zp->n, zp->prime);
    if (bit_length < 2) {
        return -1;
    }
    zp->bit_length = bit_length;
    zp->functions = &avp_cczp_default_functions;
    return 0;
}

void AVP_CC_FIELD_PUBLIC cczp_modn_ws(
    AVPCCWorkspace workspace,
    const AVPCCZP *zp,
    AVPCCUnit *result,
    AVPCCSize source_count,
    const AVPCCUnit *source
) {
    (void)workspace;
    avp_field_reduce(zp->n, result, source_count, source, zp->prime);
}

void AVP_CC_FIELD_PUBLIC cczp_mod_default_ws(
    AVPCCWorkspace workspace,
    const AVPCCZP *zp,
    AVPCCUnit *result,
    const AVPCCUnit *source_2n
) {
    cczp_modn_ws(workspace, zp, result, zp->n * 2u, source_2n);
}

void AVP_CC_FIELD_PUBLIC cczp_mul_default_ws(
    AVPCCWorkspace workspace,
    const AVPCCZP *zp,
    AVPCCUnit *result,
    const AVPCCUnit *left,
    const AVPCCUnit *right
) {
    (void)workspace;
    avp_field_mul_mod(zp->n, result, left, right, zp->prime);
}

void AVP_CC_FIELD_PUBLIC cczp_sqr_default_ws(
    AVPCCWorkspace workspace,
    const AVPCCZP *zp,
    AVPCCUnit *result,
    const AVPCCUnit *source
) {
    cczp_mul_default_ws(workspace, zp, result, source, source);
}

const AVPCCZPFunctions avp_cczp_default_functions = {
    .add = cczp_add_default_ws,
    .subtract = cczp_sub_default_ws,
    .multiply = cczp_mul_default_ws,
    .square = cczp_sqr_default_ws,
    .reduce = cczp_mod_default_ws,
    .invert = cczp_inv_default_ws,
    .square_root = cczp_sqrt_default_ws,
    .to_representation = cczp_to_default_ws,
    .from_representation = cczp_from_default_ws,
};
