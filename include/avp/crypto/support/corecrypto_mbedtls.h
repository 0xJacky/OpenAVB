#pragma once

#include <psa/crypto_values.h>

#include "mbedtls/md.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


enum {
    AVP_CCSHA384_OUTPUT_SIZE = 48,
    AVP_CCSHA384_BLOCK_SIZE = 128,
};

typedef struct {
    mbedtls_md_type_t md_type;
    size_t output_size;
    size_t block_size;
} AVPCCDigestInfo;

typedef struct {
    mbedtls_md_context_t md;
    bool initialized;
} AVPCCDigestContext;

typedef struct {
    mbedtls_md_context_t md;
    bool initialized;
} AVPCCHmacContext;

extern const AVPCCDigestInfo avp_ccsha384_mbedtls_di;

int cc_cmp_safe_internal(size_t count, const void *left, const void *right);
int cc_cmp_safe(size_t count, const void *left, const void *right);

int ccdigest_init_internal(
    const AVPCCDigestInfo *info,
    AVPCCDigestContext *context
);
int ccdigest_update_internal(
    const AVPCCDigestInfo *info,
    AVPCCDigestContext *context,
    size_t input_size,
    const void *input
);
int ccdigest_finish_mbedtls(
    const AVPCCDigestInfo *info,
    AVPCCDigestContext *context,
    void *digest
);
void ccdigest_free_mbedtls(AVPCCDigestContext *context);
int ccdigest_internal(
    const AVPCCDigestInfo *info,
    size_t input_size,
    const void *input,
    void *digest
);
int ccdigest(
    const AVPCCDigestInfo *info,
    size_t input_size,
    const void *input,
    void *digest
);

int cchmac_init_internal(
    const AVPCCDigestInfo *info,
    AVPCCHmacContext *context,
    size_t key_size,
    const void *key
);
int cchmac_update_internal(
    const AVPCCDigestInfo *info,
    AVPCCHmacContext *context,
    size_t input_size,
    const void *input
);
int cchmac_final_internal(
    const AVPCCDigestInfo *info,
    AVPCCHmacContext *context,
    unsigned char *mac
);
void cchmac_free_mbedtls(AVPCCHmacContext *context);
int cchmac_internal(
    const AVPCCDigestInfo *info,
    size_t key_size,
    const void *key,
    size_t input_size,
    const void *input,
    unsigned char *mac
);
