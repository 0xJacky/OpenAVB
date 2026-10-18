#include "avp/crypto/bignum/corecrypto_cczp_accessors.h"


AVPCCSize AVP_CCZP_ACCESSOR_PUBLIC cczp_n(const AVPCCZP *zp) {
    return zp->n;
}

const AVPCCUnit *AVP_CCZP_ACCESSOR_PUBLIC cczp_prime(const AVPCCZP *zp) {
    return zp->prime;
}

void AVP_CCZP_ACCESSOR_PUBLIC cczp_mod_ws(
    AVPCCWorkspace workspace,
    const AVPCCZP *zp,
    AVPCCUnit *result,
    const AVPCCUnit *source
) {
    zp->functions->reduce(workspace, zp, result, source);
}

void AVP_CCZP_ACCESSOR_PUBLIC cczp_mul_ws(
    AVPCCWorkspace workspace,
    const AVPCCZP *zp,
    AVPCCUnit *result,
    const AVPCCUnit *left,
    const AVPCCUnit *right
) {
    zp->functions->multiply(workspace, zp, result, left, right);
}

void AVP_CCZP_ACCESSOR_PUBLIC cczp_sqr_ws(
    AVPCCWorkspace workspace,
    const AVPCCZP *zp,
    AVPCCUnit *result,
    const AVPCCUnit *source
) {
    zp->functions->square(workspace, zp, result, source);
}

void AVP_CCZP_ACCESSOR_PUBLIC cczp_to_ws(
    AVPCCWorkspace workspace,
    const AVPCCZP *zp,
    AVPCCUnit *result,
    const AVPCCUnit *source
) {
    zp->functions->to_representation(workspace, zp, result, source);
}

void AVP_CCZP_ACCESSOR_PUBLIC cczp_from_ws(
    AVPCCWorkspace workspace,
    const AVPCCZP *zp,
    AVPCCUnit *result,
    const AVPCCUnit *source
) {
    zp->functions->from_representation(workspace, zp, result, source);
}
