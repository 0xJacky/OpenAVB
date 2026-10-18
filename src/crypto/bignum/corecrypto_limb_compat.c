#include "avp/crypto/bignum/corecrypto_limb_compat.h"

#include "mbedtls/platform_util.h"

#include <string.h>


#define AVP_CCN_ENTRY __attribute__((used, retain))

static uint64_t nonzero_mask(uint64_t value) {
    return UINT64_C(0) - (uint64_t)(value != 0);
}

uint64_t AVP_CCN_ENTRY ccn_neg(
    AVPCCSize count,
    AVPCCUnit *result,
    const AVPCCUnit *source
) {
    uint64_t carry = 1;
    for (size_t index = 0; index < count; ++index) {
        const uint64_t complement = ~source[index];
        result[index] = complement + carry;
        carry &= result[index] == 0;
    }
    return carry;
}

void AVP_CCN_ENTRY ccn_zero(AVPCCSize count, AVPCCUnit *result) {
    mbedtls_platform_zeroize(result, count * sizeof(*result));
}

void AVP_CCN_ENTRY ccn_seti(
    AVPCCSize count,
    AVPCCUnit *result,
    AVPCCUnit value
) {
    result[0] = value;
    ccn_zero(count - 1, result + 1);
}

void AVP_CCN_ENTRY ccn_swap(AVPCCSize count, AVPCCUnit *value) {
    for (size_t lower = 0, upper = count; lower < upper; ++lower) {
        --upper;
        const uint64_t lower_value = __builtin_bswap64(value[lower]);
        const uint64_t upper_value = __builtin_bswap64(value[upper]);
        value[lower] = upper_value;
        value[upper] = lower_value;
    }
}

int AVP_CCN_ENTRY ccn_cmp_asm(
    AVPCCSize count,
    const AVPCCUnit *left,
    const AVPCCUnit *right
) {
    int result = 0;
    for (size_t index = 0; index < count; ++index) {
        const int greater = left[index] > right[index];
        const int less = left[index] < right[index];
        result = greater - less;
    }
    return result;
}

size_t AVP_CCN_ENTRY ccn_bitlen_public_value(
    AVPCCSize count,
    const AVPCCUnit *value
) {
    size_t result = 0;
    for (size_t index = 0; index < count; ++index) {
        const uint64_t limb = value[index];
        const uint64_t mask = nonzero_mask(limb);
        const size_t candidate = index * 64 +
            (64 - (size_t)__builtin_clzll(limb | UINT64_C(1)));
        result = (result & (size_t)~mask) | (candidate & (size_t)mask);
    }
    return result;
}

size_t AVP_CCN_ENTRY ccn_bitlen_internal(
    AVPCCSize count,
    const AVPCCUnit *value
) {
    return ccn_bitlen_public_value(count, value);
}

int AVP_CCN_ENTRY ccn_cmp_internal(
    AVPCCSize count,
    const AVPCCUnit *left,
    const AVPCCUnit *right
) {
    return ccn_cmp_asm(count, left, right);
}

AVPCCSize AVP_CCN_ENTRY ccn_n(
    AVPCCSize count,
    const AVPCCUnit *value
) {
    return ccn_n_noasm(count, value);
}

int AVP_CCN_ENTRY ccn_read_uint_public_value(
    AVPCCSize count,
    AVPCCUnit *result,
    size_t input_size,
    const uint8_t *input
) {
    const size_t capacity = count * sizeof(*result);
    if (input_size > capacity) {
        const size_t overflow_size = input_size - capacity;
        uint8_t overflow = 0;
        for (size_t index = 0; index < overflow_size; ++index) {
            overflow |= input[index];
        }
        if (overflow != 0) {
            return AVP_CCN_READ_UINT_OVERFLOW;
        }
        input += overflow_size;
        input_size = capacity;
    }

    ccn_zero(count, result);
    for (size_t byte = 0; byte < input_size; ++byte) {
        const size_t source_index = input_size - byte - 1;
        result[byte / sizeof(*result)] |=
            (uint64_t)input[source_index] << ((byte & 7) * 8);
    }
    return 0;
}

int AVP_CCN_ENTRY ccn_read_uint_internal(
    AVPCCSize count,
    AVPCCUnit *result,
    size_t input_size,
    const uint8_t *input
) {
    return ccn_read_uint_public_value(count, result, input_size, input);
}

void AVP_CCN_ENTRY ccn_set(
    AVPCCSize count,
    AVPCCUnit *result,
    const AVPCCUnit *source
) {
    if ((uintptr_t)result <= (uintptr_t)source) {
        for (size_t index = 0; index < count; ++index) {
            result[index] = source[index];
        }
    } else {
        for (size_t index = count; index > 0; --index) {
            result[index - 1] = source[index - 1];
        }
    }
}

size_t AVP_CCN_ENTRY ccn_write_uint_size_public_value(
    AVPCCSize count,
    const AVPCCUnit *value
) {
    return (ccn_bitlen_public_value(count, value) + 7) / 8;
}

size_t AVP_CCN_ENTRY ccn_trailing_zeros(
    AVPCCSize count,
    const AVPCCUnit *value
) {
    size_t result = 0;
    for (size_t index = count; index > 0; --index) {
        const size_t limb_index = index - 1;
        const uint64_t limb = value[limb_index];
        const uint64_t mask = nonzero_mask(limb);
        const size_t candidate = limb_index * 64 +
            (size_t)__builtin_ctzll(limb | (UINT64_C(1) << 63));
        result = (result & (size_t)~mask) | (candidate & (size_t)mask);
    }
    return result;
}

AVPCCSize AVP_CCN_ENTRY ccn_n_noasm(
    AVPCCSize count,
    const AVPCCUnit *value
) {
    size_t result = 0;
    for (size_t index = 0; index < count; ++index) {
        const size_t mask = (size_t)nonzero_mask(value[index]);
        result = (result & ~mask) | ((index + 1) & mask);
    }
    return result;
}
