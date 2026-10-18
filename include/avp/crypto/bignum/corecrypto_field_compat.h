#pragma once

#include "avp/crypto/bignum/corecrypto_cczp_accessors.h"

#include <stddef.h>
#include <stdint.h>


enum {
    AVP_CC_FIELD_MAX_N = 64,
};

#define AVP_CC_FIELD_PUBLIC __attribute__((used, retain))

extern const AVPCCZPFunctions avp_cczp_default_functions;
extern const AVPCCZPFunctions avp_cczp_montgomery_functions;

AVP_CC_FIELD_PUBLIC int cczp_mm_init_ws(
    AVPCCWorkspace workspace,
    AVPCCZP *zp,
    AVPCCSize count,
    const AVPCCUnit *prime
);
AVP_CC_FIELD_PUBLIC void cczp_mm_to_ws(
    AVPCCWorkspace workspace,
    const AVPCCZP *zp,
    AVPCCUnit *result,
    const AVPCCUnit *source
);
AVP_CC_FIELD_PUBLIC void cczp_mm_from_ws(
    AVPCCWorkspace workspace,
    const AVPCCZP *zp,
    AVPCCUnit *result,
    const AVPCCUnit *source
);
AVP_CC_FIELD_PUBLIC void cczp_mm_mul_ws(
    AVPCCWorkspace workspace,
    const AVPCCZP *zp,
    AVPCCUnit *result,
    const AVPCCUnit *left,
    const AVPCCUnit *right
);
AVP_CC_FIELD_PUBLIC void cczp_mm_sqr_ws(
    AVPCCWorkspace workspace,
    const AVPCCZP *zp,
    AVPCCUnit *result,
    const AVPCCUnit *source
);
AVP_CC_FIELD_PUBLIC void cczp_mm_mod_ws(
    AVPCCWorkspace workspace,
    const AVPCCZP *zp,
    AVPCCUnit *result,
    const AVPCCUnit *source_2n
);
AVP_CC_FIELD_PUBLIC void cczp_mm_redc_ws(
    AVPCCWorkspace workspace,
    const AVPCCZP *zp,
    AVPCCUnit *result,
    AVPCCUnit *temporary
);

AVP_CC_FIELD_PUBLIC void cczp_from_default_ws(
    AVPCCWorkspace workspace,
    const AVPCCZP *zp,
    AVPCCUnit *result,
    const AVPCCUnit *source
);
AVP_CC_FIELD_PUBLIC void cczp_to_default_ws(
    AVPCCWorkspace workspace,
    const AVPCCZP *zp,
    AVPCCUnit *result,
    const AVPCCUnit *source
);
AVP_CC_FIELD_PUBLIC void cczp_add_default_ws(
    AVPCCWorkspace workspace,
    const AVPCCZP *zp,
    AVPCCUnit *result,
    const AVPCCUnit *left,
    const AVPCCUnit *right
);
AVP_CC_FIELD_PUBLIC void cczp_sub_default_ws(
    AVPCCWorkspace workspace,
    const AVPCCZP *zp,
    AVPCCUnit *result,
    const AVPCCUnit *left,
    const AVPCCUnit *right
);
AVP_CC_FIELD_PUBLIC int cczp_init_ws(AVPCCWorkspace workspace, AVPCCZP *zp);
AVP_CC_FIELD_PUBLIC void cczp_modn_ws(
    AVPCCWorkspace workspace,
    const AVPCCZP *zp,
    AVPCCUnit *result,
    AVPCCSize source_count,
    const AVPCCUnit *source
);
AVP_CC_FIELD_PUBLIC void cczp_mod_default_ws(
    AVPCCWorkspace workspace,
    const AVPCCZP *zp,
    AVPCCUnit *result,
    const AVPCCUnit *source_2n
);
AVP_CC_FIELD_PUBLIC void cczp_mul_default_ws(
    AVPCCWorkspace workspace,
    const AVPCCZP *zp,
    AVPCCUnit *result,
    const AVPCCUnit *left,
    const AVPCCUnit *right
);
AVP_CC_FIELD_PUBLIC void cczp_sqr_default_ws(
    AVPCCWorkspace workspace,
    const AVPCCZP *zp,
    AVPCCUnit *result,
    const AVPCCUnit *source
);
AVP_CC_FIELD_PUBLIC void ccn_sqr_ws(
    AVPCCWorkspace workspace,
    AVPCCSize count,
    AVPCCUnit *result_2n,
    const AVPCCUnit *source
);
AVP_CC_FIELD_PUBLIC void ccn_divmod_ws(
    AVPCCWorkspace workspace,
    AVPCCSize dividend_count,
    const AVPCCUnit *dividend,
    AVPCCSize quotient_count,
    AVPCCUnit *quotient,
    AVPCCSize divisor_count,
    AVPCCUnit *remainder,
    const AVPCCUnit *divisor
);

AVP_CC_FIELD_PUBLIC int cczp_power_fast_ws(
    AVPCCWorkspace workspace,
    const AVPCCZP *zp,
    AVPCCUnit *result,
    const AVPCCUnit *base,
    const AVPCCUnit *exponent
);
AVP_CC_FIELD_PUBLIC int cczp_is_quadratic_residue_ws(
    AVPCCWorkspace workspace,
    const AVPCCZP *zp,
    const AVPCCUnit *source
);
AVP_CC_FIELD_PUBLIC int cczp_inv_default_ws(
    AVPCCWorkspace workspace,
    const AVPCCZP *zp,
    AVPCCUnit *result,
    const AVPCCUnit *source
);
AVP_CC_FIELD_PUBLIC void cczp_inv_update_redc_ws(
    AVPCCWorkspace workspace,
    const AVPCCZP *zp,
    AVPCCUnit *result,
    const AVPCCUnit *left,
    AVPCCUnit left_factor,
    const AVPCCUnit *right,
    AVPCCUnit right_factor
);
AVP_CC_FIELD_PUBLIC int cczp_sqrt_tonelli_shanks_precomp_ws(
    AVPCCWorkspace workspace,
    const AVPCCZP *zp,
    AVPCCUnit *result,
    const AVPCCUnit *source,
    uint64_t c1,
    const AVPCCUnit *c3,
    const AVPCCUnit *c5
);
AVP_CC_FIELD_PUBLIC int cczp_sqrt_default_ws(
    AVPCCWorkspace workspace,
    const AVPCCZP *zp,
    AVPCCUnit *result,
    const AVPCCUnit *source
);
AVP_CC_FIELD_PUBLIC void ccn_gcd_approximate(
    AVPCCSize count,
    const AVPCCUnit *left,
    AVPCCUnit *left_approximation,
    const AVPCCUnit *right,
    AVPCCUnit *right_approximation
);
AVP_CC_FIELD_PUBLIC AVPCCUnit ccn_gcd_update_ws(
    AVPCCWorkspace workspace,
    AVPCCSize count,
    AVPCCUnit *result,
    const AVPCCUnit *left,
    AVPCCUnit left_factor,
    const AVPCCUnit *right,
    AVPCCUnit right_factor
);
AVP_CC_FIELD_PUBLIC void cczp_cond_negate(
    const AVPCCZP *zp,
    AVPCCUnit selector,
    AVPCCUnit *result,
    const AVPCCUnit *source
);
