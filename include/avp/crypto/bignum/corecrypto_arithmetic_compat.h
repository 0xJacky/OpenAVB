#pragma once

#include "avp/crypto/bignum/corecrypto_limb_compat.h"


#define AVP_CCN_ARITHMETIC_PUBLIC __attribute__((used, retain))

typedef void *AVPCCWorkspace;

AVP_CCN_ARITHMETIC_PUBLIC AVPCCUnit ccn_addmul1_asm(
    AVPCCSize count,
    AVPCCUnit *result,
    const AVPCCUnit *source,
    AVPCCUnit multiplier
);
AVP_CCN_ARITHMETIC_PUBLIC void ccn_mul(
    AVPCCSize count,
    AVPCCUnit *result,
    const AVPCCUnit *left,
    const AVPCCUnit *right
);
AVP_CCN_ARITHMETIC_PUBLIC AVPCCUnit ccn_mul1_asm(
    AVPCCSize count,
    AVPCCUnit *result,
    const AVPCCUnit *source,
    AVPCCUnit multiplier
);
AVP_CCN_ARITHMETIC_PUBLIC AVPCCUnit ccn_addmul1(
    AVPCCSize count,
    AVPCCUnit *result,
    const AVPCCUnit *source,
    AVPCCUnit multiplier
);
AVP_CCN_ARITHMETIC_PUBLIC void ccn_mux(
    AVPCCSize count,
    AVPCCUnit selector,
    AVPCCUnit *result,
    const AVPCCUnit *selected,
    const AVPCCUnit *alternate
);
AVP_CCN_ARITHMETIC_PUBLIC AVPCCUnit ccn_cond_sub(
    AVPCCSize count,
    AVPCCUnit selector,
    AVPCCUnit *result,
    const AVPCCUnit *left,
    const AVPCCUnit *right
);
AVP_CCN_ARITHMETIC_PUBLIC AVPCCUnit ccn_add_ws(
    AVPCCWorkspace workspace,
    AVPCCSize count,
    AVPCCUnit *result,
    const AVPCCUnit *left,
    const AVPCCUnit *right
);
AVP_CCN_ARITHMETIC_PUBLIC AVPCCUnit ccn_add_asm(
    AVPCCSize count,
    AVPCCUnit *result,
    const AVPCCUnit *left,
    const AVPCCUnit *right
);
AVP_CCN_ARITHMETIC_PUBLIC AVPCCUnit ccn_add1_ws(
    AVPCCWorkspace workspace,
    AVPCCSize count,
    AVPCCUnit *result,
    const AVPCCUnit *source,
    AVPCCUnit value
);
AVP_CCN_ARITHMETIC_PUBLIC void ccn_mul_ws(
    AVPCCWorkspace workspace,
    AVPCCSize count,
    AVPCCUnit *result,
    const AVPCCUnit *left,
    const AVPCCUnit *right
);
AVP_CCN_ARITHMETIC_PUBLIC AVPCCUnit ccn_mul1(
    AVPCCSize count,
    AVPCCUnit *result,
    const AVPCCUnit *source,
    AVPCCUnit multiplier
);
AVP_CCN_ARITHMETIC_PUBLIC void ccn_shift_right(
    AVPCCSize count,
    AVPCCUnit *result,
    const AVPCCUnit *source,
    size_t shift
);
AVP_CCN_ARITHMETIC_PUBLIC AVPCCUnit ccn_sub_ws(
    AVPCCWorkspace workspace,
    AVPCCSize count,
    AVPCCUnit *result,
    const AVPCCUnit *left,
    const AVPCCUnit *right
);
AVP_CCN_ARITHMETIC_PUBLIC AVPCCUnit ccn_sub_asm(
    AVPCCSize count,
    AVPCCUnit *result,
    const AVPCCUnit *left,
    const AVPCCUnit *right
);
AVP_CCN_ARITHMETIC_PUBLIC AVPCCUnit ccn_sub1(
    AVPCCSize count,
    AVPCCUnit *result,
    const AVPCCUnit *source,
    AVPCCUnit value
);
AVP_CCN_ARITHMETIC_PUBLIC AVPCCUnit ccn_cond_add(
    AVPCCSize count,
    AVPCCUnit selector,
    AVPCCUnit *result,
    const AVPCCUnit *left,
    const AVPCCUnit *right
);
AVP_CCN_ARITHMETIC_PUBLIC void ccn_cond_neg(
    AVPCCSize count,
    AVPCCUnit selector,
    AVPCCUnit *result,
    const AVPCCUnit *source
);
AVP_CCN_ARITHMETIC_PUBLIC void ccn_shift_left(
    AVPCCSize count,
    AVPCCUnit *result,
    const AVPCCUnit *source,
    size_t shift
);
AVP_CCN_ARITHMETIC_PUBLIC void ccn_shift_right_asm(
    AVPCCSize count,
    AVPCCUnit *result,
    const AVPCCUnit *source,
    size_t shift
);
void ccn_mux_seed_mask_state(AVPCCUnit seed);
AVP_CCN_ARITHMETIC_PUBLIC AVPCCUnit ccn_mux_next_mask(void);
AVP_CCN_ARITHMETIC_PUBLIC void ccn_shift_right_multi(
    AVPCCSize count,
    AVPCCUnit *result,
    const AVPCCUnit *source,
    size_t shift
);
AVP_CCN_ARITHMETIC_PUBLIC AVPCCUnit ccn_subn(
    AVPCCSize count,
    AVPCCUnit *result,
    const AVPCCUnit *left,
    AVPCCSize right_count,
    const AVPCCUnit *right
);
AVP_CCN_ARITHMETIC_PUBLIC AVPCCUnit ccn_add1_asm(
    AVPCCSize count,
    AVPCCUnit *result,
    const AVPCCUnit *source,
    AVPCCUnit value
);
