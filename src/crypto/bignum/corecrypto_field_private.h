#pragma once

#include "avp/crypto/bignum/corecrypto_field_compat.h"

#include <stdbool.h>


int avp_field_compare(
    size_t count,
    const uint64_t *left,
    const uint64_t *right
);
uint64_t avp_field_add(
    size_t count,
    uint64_t *result,
    const uint64_t *left,
    const uint64_t *right
);
uint64_t avp_field_sub(
    size_t count,
    uint64_t *result,
    const uint64_t *left,
    const uint64_t *right
);
void avp_field_shift_right_one(
    size_t count,
    uint64_t *value,
    uint64_t high_bit
);
bool avp_field_is_zero(size_t count, const uint64_t *value);
bool avp_field_is_one(size_t count, const uint64_t *value);
size_t avp_field_bit_length(size_t count, const uint64_t *value);
void avp_field_reduce(
    size_t modulus_count,
    uint64_t *result,
    size_t source_count,
    const uint64_t *source,
    const uint64_t *modulus
);
void avp_field_add_mod(
    size_t count,
    uint64_t *result,
    const uint64_t *left,
    const uint64_t *right,
    const uint64_t *modulus
);
void avp_field_sub_mod(
    size_t count,
    uint64_t *result,
    const uint64_t *left,
    const uint64_t *right,
    const uint64_t *modulus
);
void avp_field_mul_mod(
    size_t count,
    uint64_t *result,
    const uint64_t *left,
    const uint64_t *right,
    const uint64_t *modulus
);
void avp_field_pow_mod(
    size_t count,
    uint64_t *result,
    const uint64_t *base,
    const uint64_t *exponent,
    const uint64_t *modulus
);
int avp_field_inverse_odd(
    size_t count,
    uint64_t *result,
    const uint64_t *source,
    const uint64_t *modulus
);
