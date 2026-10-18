#pragma once

#include <stddef.h>
#include <stdint.h>

#include "mbedtls/private/sha512.h"

enum {
    AVP_SHA384_DIGEST_SIZE = 48,
    AVP_SHA512_DIGEST_SIZE = 64,
    AVP_SHA512_BLOCK_SIZE = 128,
};

typedef struct {
    size_t output_size;
} AVPCCSHA512Info;

typedef struct {
    mbedtls_sha512_context mbedtls;
} AVPCCSHA512Context;

#define AVP_CCSHA512_PUBLIC __attribute__((used, retain))

extern const AVPCCSHA512Info avp_ccsha384_info;
extern const AVPCCSHA512Info avp_ccsha512_info;

void avp_ccsha512_init(
    AVPCCSHA512Context *context,
    const AVPCCSHA512Info *info
);
int avp_ccsha512_update(
    AVPCCSHA512Context *context,
    const void *input,
    size_t input_size
);
AVP_CCSHA512_PUBLIC void ccsha384_vng_arm_hw_compress(
    uint64_t state[8],
    size_t block_count,
    const void *blocks
);
AVP_CCSHA512_PUBLIC void ccsha512_ltc_compress(
    uint64_t state[8],
    size_t block_count,
    const void *blocks
);
AVP_CCSHA512_PUBLIC int ccsha512_final(
    const AVPCCSHA512Info *info,
    const AVPCCSHA512Context *context,
    uint8_t *digest
);
AVP_CCSHA512_PUBLIC void
AccelerateCrypto_SHA512_compress_hwassist(
    uint64_t state[8],
    size_t block_count,
    const void *blocks
);
