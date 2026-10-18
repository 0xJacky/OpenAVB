#define MBEDTLS_ALLOW_PRIVATE_ACCESS
#include "avp/crypto/hash/corecrypto_sha512_compat.h"

const AVPCCSHA512Info avp_ccsha384_info = {
    .output_size = AVP_SHA384_DIGEST_SIZE,
};

const AVPCCSHA512Info avp_ccsha512_info = {
    .output_size = AVP_SHA512_DIGEST_SIZE,
};

static const uint64_t sha384_initial_state[8] = {
    UINT64_C(0xcbbb9d5dc1059ed8), UINT64_C(0x629a292a367cd507),
    UINT64_C(0x9159015a3070dd17), UINT64_C(0x152fecd8f70e5939),
    UINT64_C(0x67332667ffc00b31), UINT64_C(0x8eb44a8768581511),
    UINT64_C(0xdb0c2e0d64f98fa7), UINT64_C(0x47b5481dbefa4fa4),
};

static const uint64_t sha512_initial_state[8] = {
    UINT64_C(0x6a09e667f3bcc908), UINT64_C(0xbb67ae8584caa73b),
    UINT64_C(0x3c6ef372fe94f82b), UINT64_C(0xa54ff53a5f1d36f1),
    UINT64_C(0x510e527fade682d1), UINT64_C(0x9b05688c2b3e6c1f),
    UINT64_C(0x1f83d9abfb41bd6b), UINT64_C(0x5be0cd19137e2179),
};

static _Noreturn void ccsha512_contract_failure(void) { __builtin_trap(); }

static int ccsha512_is384(const AVPCCSHA512Info *info) {
    if (info == 0 || (info->output_size != AVP_SHA384_DIGEST_SIZE &&
                      info->output_size != AVP_SHA512_DIGEST_SIZE)) {
        ccsha512_contract_failure();
    }
    return info->output_size == AVP_SHA384_DIGEST_SIZE;
}

void avp_ccsha512_init(
    AVPCCSHA512Context *context,
    const AVPCCSHA512Info *info
) {
    if (context == 0) {
        ccsha512_contract_failure();
    }
    const int is384 = ccsha512_is384(info);
    mbedtls_sha512_init(&context->mbedtls);
    context->mbedtls.total[0] = 0;
    context->mbedtls.total[1] = 0;
    const uint64_t *initial_state =
        is384 ? sha384_initial_state : sha512_initial_state;
    for (size_t index = 0; index < 8; ++index) {
        context->mbedtls.state[index] = initial_state[index];
    }
    for (size_t index = 0; index < sizeof(context->mbedtls.buffer); ++index) {
        context->mbedtls.buffer[index] = 0;
    }
    context->mbedtls.is384 = is384;
}

int avp_ccsha512_update(
    AVPCCSHA512Context *context,
    const void *input,
    size_t input_size
) {
    if (context == 0 || (input == 0 && input_size != 0)) {
        ccsha512_contract_failure();
    }
    return mbedtls_sha512_update(&context->mbedtls, input, input_size);
}

static void ccsha512_compress_with_mbedtls(
    uint64_t state[8],
    size_t block_count,
    const void *blocks
) {
    if (state == 0 || (blocks == 0 && block_count != 0)) {
        ccsha512_contract_failure();
    }

    mbedtls_sha512_context context;
    mbedtls_sha512_init(&context);
    for (size_t index = 0; index < 8; ++index) {
        context.state[index] = state[index];
    }
    context.total[0] = 0;
    context.total[1] = 0;
    context.is384 = 1;
    const int status = mbedtls_sha512_update(
        &context,
        blocks,
        block_count * AVP_SHA512_BLOCK_SIZE
    );
    if (status != 0) {
        ccsha512_contract_failure();
    }
    for (size_t index = 0; index < 8; ++index) {
        state[index] = context.state[index];
    }
    mbedtls_sha512_free(&context);
}

void AVP_CCSHA512_PUBLIC ccsha384_vng_arm_hw_compress(
    uint64_t state[8],
    size_t block_count,
    const void *blocks
) {
    ccsha512_compress_with_mbedtls(state, block_count, blocks);
}

void AVP_CCSHA512_PUBLIC ccsha512_ltc_compress(
    uint64_t state[8],
    size_t block_count,
    const void *blocks
) {
    ccsha512_compress_with_mbedtls(state, block_count, blocks);
}

int AVP_CCSHA512_PUBLIC ccsha512_final(
    const AVPCCSHA512Info *info,
    const AVPCCSHA512Context *context,
    uint8_t *digest
) {
    if (context == 0 || digest == 0) {
        ccsha512_contract_failure();
    }
    const int is384 = ccsha512_is384(info);
    mbedtls_sha512_context clone;
    mbedtls_sha512_init(&clone);
    mbedtls_sha512_clone(&clone, &context->mbedtls);
    clone.is384 = is384;
    /* Clone finalization preserves the caller's streaming state. */
    return mbedtls_sha512_finish(&clone, digest);
}

void AVP_CCSHA512_PUBLIC AccelerateCrypto_SHA512_compress_hwassist(
    uint64_t state[8],
    size_t block_count,
    const void *blocks
) {
    ccsha512_compress_with_mbedtls(state, block_count, blocks);
}
