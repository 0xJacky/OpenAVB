/* Montgomery representation adapters follow LibTomMath's public radix model. */
#include "corecrypto_field_private.h"

#include <string.h>


static const AVPCCUnit *montgomery_r2(const AVPCCZP *zp) {
    return zp->prime + zp->n + 1u;
}

static AVPCCUnit *mutable_montgomery_r2(AVPCCZP *zp) {
    return zp->prime + zp->n + 1u;
}

static AVPCCUnit *mutable_montgomery_inverse(AVPCCZP *zp) {
    return zp->prime + zp->n;
}

static void montgomery_radix(const AVPCCZP *zp, AVPCCUnit *radix) {
    memset(radix, 0, zp->n * sizeof(*radix));
    radix[0] = 1;
    for (size_t bit = 0; bit < zp->n * 64u; ++bit) {
        avp_field_add_mod(zp->n, radix, radix, radix, zp->prime);
    }
}

static void montgomery_radix_inverse(
    const AVPCCZP *zp,
    AVPCCUnit *inverse
) {
    AVPCCUnit radix[AVP_CC_FIELD_MAX_N];
    montgomery_radix(zp, radix);
    if (avp_field_inverse_odd(zp->n, inverse, radix, zp->prime) != 0) {
        __builtin_trap();
    }
}

int AVP_CC_FIELD_PUBLIC cczp_mm_init_ws(
    AVPCCWorkspace workspace,
    AVPCCZP *zp,
    AVPCCSize count,
    const AVPCCUnit *prime
) {
    (void)workspace;
    if (zp == NULL || prime == NULL || count == 0 ||
        count > AVP_CC_FIELD_MAX_N || (prime[0] & 1u) == 0 ||
        avp_field_is_zero(count, prime)) {
        return -1;
    }
    zp->n = count;
    zp->bit_length = avp_field_bit_length(count, prime);
    zp->functions = &avp_cczp_montgomery_functions;
    memcpy(zp->prime, prime, count * sizeof(*prime));

    uint64_t inverse = 1;
    for (unsigned int round = 0; round < 6; ++round) {
        inverse *= 2u - prime[0] * inverse;
    }
    *mutable_montgomery_inverse(zp) = 0u - inverse;

    AVPCCUnit radix[AVP_CC_FIELD_MAX_N];
    montgomery_radix(zp, radix);
    avp_field_mul_mod(
        count,
        mutable_montgomery_r2(zp),
        radix,
        radix,
        zp->prime
    );
    return 0;
}

void AVP_CC_FIELD_PUBLIC cczp_mm_to_ws(
    AVPCCWorkspace workspace,
    const AVPCCZP *zp,
    AVPCCUnit *result,
    const AVPCCUnit *source
) {
    cczp_mm_mul_ws(workspace, zp, result, source, montgomery_r2(zp));
}

void AVP_CC_FIELD_PUBLIC cczp_mm_from_ws(
    AVPCCWorkspace workspace,
    const AVPCCZP *zp,
    AVPCCUnit *result,
    const AVPCCUnit *source
) {
    (void)workspace;
    AVPCCUnit inverse[AVP_CC_FIELD_MAX_N];
    montgomery_radix_inverse(zp, inverse);
    avp_field_mul_mod(zp->n, result, source, inverse, zp->prime);
}

void AVP_CC_FIELD_PUBLIC cczp_mm_mul_ws(
    AVPCCWorkspace workspace,
    const AVPCCZP *zp,
    AVPCCUnit *result,
    const AVPCCUnit *left,
    const AVPCCUnit *right
) {
    (void)workspace;
    AVPCCUnit product[AVP_CC_FIELD_MAX_N];
    AVPCCUnit inverse[AVP_CC_FIELD_MAX_N];
    avp_field_mul_mod(zp->n, product, left, right, zp->prime);
    montgomery_radix_inverse(zp, inverse);
    avp_field_mul_mod(zp->n, result, product, inverse, zp->prime);
}

void AVP_CC_FIELD_PUBLIC cczp_mm_sqr_ws(
    AVPCCWorkspace workspace,
    const AVPCCZP *zp,
    AVPCCUnit *result,
    const AVPCCUnit *source
) {
    cczp_mm_mul_ws(workspace, zp, result, source, source);
}

void AVP_CC_FIELD_PUBLIC cczp_mm_mod_ws(
    AVPCCWorkspace workspace,
    const AVPCCZP *zp,
    AVPCCUnit *result,
    const AVPCCUnit *source_2n
) {
    AVPCCUnit reduced[AVP_CC_FIELD_MAX_N];
    cczp_modn_ws(workspace, zp, reduced, zp->n * 2u, source_2n);
    cczp_mm_to_ws(workspace, zp, result, reduced);
}

void AVP_CC_FIELD_PUBLIC cczp_mm_redc_ws(
    AVPCCWorkspace workspace,
    const AVPCCZP *zp,
    AVPCCUnit *result,
    AVPCCUnit *temporary
) {
    (void)workspace;
    AVPCCUnit reduced[AVP_CC_FIELD_MAX_N];
    AVPCCUnit inverse[AVP_CC_FIELD_MAX_N];
    avp_field_reduce(zp->n, reduced, zp->n * 2u, temporary, zp->prime);
    montgomery_radix_inverse(zp, inverse);
    avp_field_mul_mod(zp->n, result, reduced, inverse, zp->prime);
}

const AVPCCZPFunctions avp_cczp_montgomery_functions = {
    .add = cczp_add_default_ws,
    .subtract = cczp_sub_default_ws,
    .multiply = cczp_mm_mul_ws,
    .square = cczp_mm_sqr_ws,
    .reduce = cczp_mm_mod_ws,
    .invert = cczp_inv_default_ws,
    .square_root = cczp_sqrt_default_ws,
    .to_representation = cczp_mm_to_ws,
    .from_representation = cczp_mm_from_ws,
};

_Static_assert(
    sizeof(AVPCCZPFunctions) == 9u * sizeof(void *),
    "cczp function table layout"
);
