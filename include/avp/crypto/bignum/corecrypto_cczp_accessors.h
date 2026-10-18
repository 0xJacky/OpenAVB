#pragma once

#include "avp/crypto/bignum/corecrypto_limb_compat.h"


#define AVP_CCZP_ACCESSOR_PUBLIC __attribute__((used, retain))

typedef void *AVPCCWorkspace;
struct AVPCCZP;

typedef void (*AVPCCZPBinaryOperation)(
    AVPCCWorkspace workspace,
    const struct AVPCCZP *zp,
    AVPCCUnit *result,
    const AVPCCUnit *left,
    const AVPCCUnit *right
);
typedef void (*AVPCCZPUnaryOperation)(
    AVPCCWorkspace workspace,
    const struct AVPCCZP *zp,
    AVPCCUnit *result,
    const AVPCCUnit *source
);
typedef int (*AVPCCZPUnaryStatusOperation)(
    AVPCCWorkspace workspace,
    const struct AVPCCZP *zp,
    AVPCCUnit *result,
    const AVPCCUnit *source
);

typedef struct {
    AVPCCZPBinaryOperation add;
    AVPCCZPBinaryOperation subtract;
    AVPCCZPBinaryOperation multiply;
    AVPCCZPUnaryOperation square;
    AVPCCZPUnaryOperation reduce;
    AVPCCZPUnaryStatusOperation invert;
    AVPCCZPUnaryStatusOperation square_root;
    AVPCCZPUnaryOperation to_representation;
    AVPCCZPUnaryOperation from_representation;
} AVPCCZPFunctions;

typedef struct AVPCCZP {
    AVPCCSize n;
    AVPCCUnit bit_length;
    const AVPCCZPFunctions *functions;
    AVPCCUnit prime[];
} AVPCCZP;

_Static_assert(offsetof(AVPCCZP, prime) == 0x18, "cczp prime offset");

AVP_CCZP_ACCESSOR_PUBLIC AVPCCSize cczp_n(const AVPCCZP *zp);
AVP_CCZP_ACCESSOR_PUBLIC const AVPCCUnit *cczp_prime(const AVPCCZP *zp);
AVP_CCZP_ACCESSOR_PUBLIC void cczp_mod_ws(
    AVPCCWorkspace workspace,
    const AVPCCZP *zp,
    AVPCCUnit *result,
    const AVPCCUnit *source
);
AVP_CCZP_ACCESSOR_PUBLIC void cczp_mul_ws(
    AVPCCWorkspace workspace,
    const AVPCCZP *zp,
    AVPCCUnit *result,
    const AVPCCUnit *left,
    const AVPCCUnit *right
);
AVP_CCZP_ACCESSOR_PUBLIC void cczp_sqr_ws(
    AVPCCWorkspace workspace,
    const AVPCCZP *zp,
    AVPCCUnit *result,
    const AVPCCUnit *source
);
AVP_CCZP_ACCESSOR_PUBLIC void cczp_to_ws(
    AVPCCWorkspace workspace,
    const AVPCCZP *zp,
    AVPCCUnit *result,
    const AVPCCUnit *source
);
AVP_CCZP_ACCESSOR_PUBLIC void cczp_from_ws(
    AVPCCWorkspace workspace,
    const AVPCCZP *zp,
    AVPCCUnit *result,
    const AVPCCUnit *source
);
