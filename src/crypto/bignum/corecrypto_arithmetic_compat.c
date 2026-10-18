#include "avp/crypto/bignum/corecrypto_arithmetic_compat.h"


static AVPCCUnit ccn_mux_mask_state = UINT64_C(0x63636e2d6d61736b);

void ccn_mux_seed_mask_state(AVPCCUnit seed) {
    ccn_mux_mask_state = seed;
}

AVPCCUnit AVP_CCN_ARITHMETIC_PUBLIC ccn_mux_next_mask(void) {
    AVPCCUnit value = ccn_mux_mask_state;
    value ^= value << 13;
    value ^= value >> 7;
    value ^= value << 17;
    ccn_mux_mask_state = value;
    return value;
}

AVPCCUnit AVP_CCN_ARITHMETIC_PUBLIC ccn_add_asm(
    AVPCCSize count,
    AVPCCUnit *result,
    const AVPCCUnit *left,
    const AVPCCUnit *right
) {
    AVPCCUnit carry = 0;
    for (size_t index = 0; index < count; ++index) {
        const __uint128_t sum = (__uint128_t)left[index] + right[index] + carry;
        result[index] = (AVPCCUnit)sum;
        carry = (AVPCCUnit)(sum >> 64);
    }
    return carry;
}

AVPCCUnit AVP_CCN_ARITHMETIC_PUBLIC ccn_sub_asm(
    AVPCCSize count,
    AVPCCUnit *result,
    const AVPCCUnit *left,
    const AVPCCUnit *right
) {
    AVPCCUnit borrow = 0;
    for (size_t index = 0; index < count; ++index) {
        const AVPCCUnit right_with_borrow = right[index] + borrow;
        const AVPCCUnit carry_into_right = right_with_borrow < right[index];
        const AVPCCUnit next_borrow =
            carry_into_right | (left[index] < right_with_borrow);
        result[index] = left[index] - right_with_borrow;
        borrow = next_borrow;
    }
    return borrow;
}

AVPCCUnit AVP_CCN_ARITHMETIC_PUBLIC ccn_add_ws(
    AVPCCWorkspace workspace,
    AVPCCSize count,
    AVPCCUnit *result,
    const AVPCCUnit *left,
    const AVPCCUnit *right
) {
    (void)workspace;
    return ccn_add_asm(count, result, left, right);
}

AVPCCUnit AVP_CCN_ARITHMETIC_PUBLIC ccn_sub_ws(
    AVPCCWorkspace workspace,
    AVPCCSize count,
    AVPCCUnit *result,
    const AVPCCUnit *left,
    const AVPCCUnit *right
) {
    (void)workspace;
    return ccn_sub_asm(count, result, left, right);
}

AVPCCUnit AVP_CCN_ARITHMETIC_PUBLIC ccn_add1_ws(
    AVPCCWorkspace workspace,
    AVPCCSize count,
    AVPCCUnit *result,
    const AVPCCUnit *source,
    AVPCCUnit value
) {
    (void)workspace;
    AVPCCUnit carry = value;
    for (size_t index = 0; index < count; ++index) {
        const __uint128_t sum = (__uint128_t)source[index] + carry;
        result[index] = (AVPCCUnit)sum;
        carry = (AVPCCUnit)(sum >> 64);
    }
    return carry;
}

AVPCCUnit AVP_CCN_ARITHMETIC_PUBLIC ccn_add1_asm(
    AVPCCSize count,
    AVPCCUnit *result,
    const AVPCCUnit *source,
    AVPCCUnit value
) {
    return ccn_add1_ws(0, count, result, source, value);
}

AVPCCUnit AVP_CCN_ARITHMETIC_PUBLIC ccn_sub1(
    AVPCCSize count,
    AVPCCUnit *result,
    const AVPCCUnit *source,
    AVPCCUnit value
) {
    AVPCCUnit borrow = value;
    for (size_t index = 0; index < count; ++index) {
        const AVPCCUnit next_borrow = source[index] < borrow;
        result[index] = source[index] - borrow;
        borrow = next_borrow;
    }
    return borrow;
}

AVPCCUnit AVP_CCN_ARITHMETIC_PUBLIC ccn_mul1_asm(
    AVPCCSize count,
    AVPCCUnit *result,
    const AVPCCUnit *source,
    AVPCCUnit multiplier
) {
    AVPCCUnit carry = 0;
    for (size_t index = 0; index < count; ++index) {
        const __uint128_t product =
            (__uint128_t)source[index] * multiplier + carry;
        result[index] = (AVPCCUnit)product;
        carry = (AVPCCUnit)(product >> 64);
    }
    return carry;
}

AVPCCUnit AVP_CCN_ARITHMETIC_PUBLIC ccn_mul1(
    AVPCCSize count,
    AVPCCUnit *result,
    const AVPCCUnit *source,
    AVPCCUnit multiplier
) {
    return ccn_mul1_asm(count, result, source, multiplier);
}

AVPCCUnit AVP_CCN_ARITHMETIC_PUBLIC ccn_addmul1_asm(
    AVPCCSize count,
    AVPCCUnit *result,
    const AVPCCUnit *source,
    AVPCCUnit multiplier
) {
    AVPCCUnit carry = 0;
    for (size_t index = 0; index < count; ++index) {
        const __uint128_t sum = (__uint128_t)source[index] * multiplier +
            result[index] + carry;
        result[index] = (AVPCCUnit)sum;
        carry = (AVPCCUnit)(sum >> 64);
    }
    return carry;
}

AVPCCUnit AVP_CCN_ARITHMETIC_PUBLIC ccn_addmul1(
    AVPCCSize count,
    AVPCCUnit *result,
    const AVPCCUnit *source,
    AVPCCUnit multiplier
) {
    return ccn_addmul1_asm(count, result, source, multiplier);
}

void AVP_CCN_ARITHMETIC_PUBLIC ccn_mul(
    AVPCCSize count,
    AVPCCUnit *result,
    const AVPCCUnit *left,
    const AVPCCUnit *right
) {
    for (size_t index = 0; index < count * 2; ++index) {
        result[index] = 0;
    }
    for (size_t left_index = 0; left_index < count; ++left_index) {
        AVPCCUnit carry = 0;
        for (size_t right_index = 0; right_index < count; ++right_index) {
            const size_t output_index = left_index + right_index;
            const __uint128_t sum =
                (__uint128_t)left[left_index] * right[right_index] +
                result[output_index] + carry;
            result[output_index] = (AVPCCUnit)sum;
            carry = (AVPCCUnit)(sum >> 64);
        }
        result[left_index + count] = carry;
    }
}

void AVP_CCN_ARITHMETIC_PUBLIC ccn_mul_ws(
    AVPCCWorkspace workspace,
    AVPCCSize count,
    AVPCCUnit *result,
    const AVPCCUnit *left,
    const AVPCCUnit *right
) {
    (void)workspace;
    ccn_mul(count, result, left, right);
}

void AVP_CCN_ARITHMETIC_PUBLIC ccn_mux(
    AVPCCSize count,
    AVPCCUnit selector,
    AVPCCUnit *result,
    const AVPCCUnit *selected,
    const AVPCCUnit *alternate
) {
    const AVPCCUnit mask = UINT64_C(0) - (selector & 1);
    for (size_t index = 0; index < count; ++index) {
        result[index] = (selected[index] & mask) |
            (alternate[index] & ~mask);
    }
}

AVPCCUnit AVP_CCN_ARITHMETIC_PUBLIC ccn_cond_add(
    AVPCCSize count,
    AVPCCUnit selector,
    AVPCCUnit *result,
    const AVPCCUnit *left,
    const AVPCCUnit *right
) {
    const AVPCCUnit mask = UINT64_C(0) - (selector & 1);
    AVPCCUnit carry = 0;
    for (size_t index = 0; index < count; ++index) {
        const __uint128_t sum = (__uint128_t)left[index] +
            (right[index] & mask) + carry;
        result[index] = (AVPCCUnit)sum;
        carry = (AVPCCUnit)(sum >> 64);
    }
    return carry;
}

AVPCCUnit AVP_CCN_ARITHMETIC_PUBLIC ccn_cond_sub(
    AVPCCSize count,
    AVPCCUnit selector,
    AVPCCUnit *result,
    const AVPCCUnit *left,
    const AVPCCUnit *right
) {
    const AVPCCUnit mask = UINT64_C(0) - (selector & 1);
    AVPCCUnit borrow = 0;
    for (size_t index = 0; index < count; ++index) {
        const AVPCCUnit operand = right[index] & mask;
        const AVPCCUnit operand_with_borrow = operand + borrow;
        const AVPCCUnit carry_into_operand = operand_with_borrow < operand;
        const AVPCCUnit next_borrow =
            carry_into_operand | (left[index] < operand_with_borrow);
        result[index] = left[index] - operand_with_borrow;
        borrow = next_borrow;
    }
    return borrow;
}

void AVP_CCN_ARITHMETIC_PUBLIC ccn_cond_neg(
    AVPCCSize count,
    AVPCCUnit selector,
    AVPCCUnit *result,
    const AVPCCUnit *source
) {
    const AVPCCUnit mask = UINT64_C(0) - (selector & 1);
    AVPCCUnit carry = selector & 1;
    for (size_t index = 0; index < count; ++index) {
        const __uint128_t value = (__uint128_t)(source[index] ^ mask) + carry;
        result[index] = (AVPCCUnit)value;
        carry = (AVPCCUnit)(value >> 64);
    }
}

void AVP_CCN_ARITHMETIC_PUBLIC ccn_shift_right_asm(
    AVPCCSize count,
    AVPCCUnit *result,
    const AVPCCUnit *source,
    size_t shift
) {
    if (shift == 0) {
        for (size_t index = 0; index < count; ++index) {
            result[index] = source[index];
        }
        return;
    }
    for (size_t index = 0; index < count; ++index) {
        const AVPCCUnit high = index + 1 < count ? source[index + 1] : 0;
        result[index] = (source[index] >> shift) | (high << (64 - shift));
    }
}

void AVP_CCN_ARITHMETIC_PUBLIC ccn_shift_right(
    AVPCCSize count,
    AVPCCUnit *result,
    const AVPCCUnit *source,
    size_t shift
) {
    if (count != 0) {
        ccn_shift_right_asm(count, result, source, shift);
    }
}

void AVP_CCN_ARITHMETIC_PUBLIC ccn_shift_left(
    AVPCCSize count,
    AVPCCUnit *result,
    const AVPCCUnit *source,
    size_t shift
) {
    if (shift == 0) {
        for (size_t index = count; index > 0; --index) {
            result[index - 1] = source[index - 1];
        }
        return;
    }
    for (size_t index = count; index > 0; --index) {
        const size_t output_index = index - 1;
        const AVPCCUnit low = output_index == 0 ? 0 : source[output_index - 1];
        result[output_index] = (source[output_index] << shift) |
            (low >> (64 - shift));
    }
}

void AVP_CCN_ARITHMETIC_PUBLIC ccn_shift_right_multi(
    AVPCCSize count,
    AVPCCUnit *result,
    const AVPCCUnit *source,
    size_t shift
) {
    const size_t limb_offset = shift / 64;
    const size_t bit_shift = shift & 63;
    ccn_shift_right(count, result, source, bit_shift);
    for (size_t index = 0; index < count; ++index) {
        result[index] = index + limb_offset < count
            ? result[index + limb_offset]
            : 0;
    }
}

AVPCCUnit AVP_CCN_ARITHMETIC_PUBLIC ccn_subn(
    AVPCCSize count,
    AVPCCUnit *result,
    const AVPCCUnit *left,
    AVPCCSize right_count,
    const AVPCCUnit *right
) {
    if (right_count > count) {
        __builtin_trap();
    }
    const AVPCCUnit borrow =
        ccn_sub_asm(right_count, result, left, right);
    return ccn_sub1(
        count - right_count,
        result + right_count,
        left + right_count,
        borrow
    );
}
