#pragma once

#include <stddef.h>
#include <stdint.h>


typedef uint64_t AVPCCUnit;
typedef size_t AVPCCSize;

enum {
    AVP_CCN_READ_UINT_OVERFLOW = -7,
};

uint64_t ccn_neg(AVPCCSize count, AVPCCUnit *result, const AVPCCUnit *source);
void ccn_zero(AVPCCSize count, AVPCCUnit *result);
void ccn_seti(AVPCCSize count, AVPCCUnit *result, AVPCCUnit value);
void ccn_swap(AVPCCSize count, AVPCCUnit *value);
int ccn_cmp_asm(
    AVPCCSize count,
    const AVPCCUnit *left,
    const AVPCCUnit *right
);
size_t ccn_bitlen_public_value(AVPCCSize count, const AVPCCUnit *value);
size_t ccn_bitlen_internal(AVPCCSize count, const AVPCCUnit *value);
int ccn_cmp_internal(
    AVPCCSize count,
    const AVPCCUnit *left,
    const AVPCCUnit *right
);
AVPCCSize ccn_n(AVPCCSize count, const AVPCCUnit *value);
int ccn_read_uint_public_value(
    AVPCCSize count,
    AVPCCUnit *result,
    size_t input_size,
    const uint8_t *input
);
int ccn_read_uint_internal(
    AVPCCSize count,
    AVPCCUnit *result,
    size_t input_size,
    const uint8_t *input
);
void ccn_set(AVPCCSize count, AVPCCUnit *result, const AVPCCUnit *source);
size_t ccn_write_uint_size_public_value(
    AVPCCSize count,
    const AVPCCUnit *value
);
size_t ccn_trailing_zeros(AVPCCSize count, const AVPCCUnit *value);
AVPCCSize ccn_n_noasm(AVPCCSize count, const AVPCCUnit *value);

