#pragma once

#include "avp/crypto/support/corecrypto_mbedtls.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


enum {
    AVP_CCDRBG_MAX_ENTROPY_SIZE = 1U << 16,
    AVP_CCDRBG_MAX_ADDITIONAL_INPUT_SIZE = 1U << 16,
    AVP_CCDRBG_MAX_PERSONALIZATION_SIZE = 1U << 16,
    AVP_CCDRBG_MAX_REQUEST_SIZE = 1U << 16,
    AVP_CCDRBG_STATUS_OK = 0,
    AVP_CCDRBG_STATUS_ERROR = -61,
    AVP_CCDRBG_STATUS_NEED_RESEED = -62,
    AVP_CCDRBG_STATUS_PARAM_ERROR = -63,
    AVP_CCDRBG_STATUS_ABORT = -64,
};

#define AVP_CCDRBG_RESEED_INTERVAL (UINT64_C(1) << 48)

typedef struct {
    const AVPCCDigestInfo *digest_info;
    bool strict_fips;
} AVPCCDRBGNISTHMACCustom;

typedef struct AVPCCDRBGState {
    const AVPCCDRBGNISTHMACCustom *custom;
    uint8_t key[64];
    uint8_t value[64];
    uint64_t reseed_counter;
} AVPCCDRBGState;

typedef struct AVPCCDRBGInfo AVPCCDRBGInfo;

struct AVPCCDRBGInfo {
    size_t size;
    int (*init)(
        const AVPCCDRBGInfo *info,
        AVPCCDRBGState *state,
        size_t entropy_size,
        const void *entropy,
        size_t nonce_size,
        const void *nonce,
        size_t personalization_size,
        const void *personalization
    );
    int (*reseed)(
        AVPCCDRBGState *state,
        size_t entropy_size,
        const void *entropy,
        size_t additional_size,
        const void *additional
    );
    int (*generate)(
        AVPCCDRBGState *state,
        size_t output_size,
        void *output,
        size_t additional_size,
        const void *additional
    );
    void (*done)(AVPCCDRBGState *state);
    const AVPCCDRBGNISTHMACCustom *custom;
    bool (*must_reseed)(const AVPCCDRBGState *state);
};

int ccdrbg_init(
    const AVPCCDRBGInfo *info,
    AVPCCDRBGState *state,
    size_t entropy_size,
    const void *entropy,
    size_t nonce_size,
    const void *nonce,
    size_t personalization_size,
    const void *personalization
);
int ccdrbg_reseed(
    const AVPCCDRBGInfo *info,
    AVPCCDRBGState *state,
    size_t entropy_size,
    const void *entropy,
    size_t additional_size,
    const void *additional
);
int ccdrbg_generate(
    const AVPCCDRBGInfo *info,
    AVPCCDRBGState *state,
    size_t output_size,
    void *output,
    size_t additional_size,
    const void *additional
);
size_t ccdrbg_context_size(const AVPCCDRBGInfo *info);

void ccdrbg_factory_nisthmac(
    AVPCCDRBGInfo *info,
    const AVPCCDRBGNISTHMACCustom *custom
);
void ccdrbg_nisthmac_done(AVPCCDRBGState *state);
bool ccdrbg_nisthmac_must_reseed(const AVPCCDRBGState *state);
int ccdrbg_nisthmac_update(
    AVPCCDRBGState *state,
    unsigned input_count,
    ...
);
int ccdrbg_nisthmac_init(
    const AVPCCDRBGInfo *info,
    AVPCCDRBGState *state,
    size_t entropy_size,
    const void *entropy,
    size_t nonce_size,
    const void *nonce,
    size_t personalization_size,
    const void *personalization
);
int ccdrbg_nisthmac_generate(
    AVPCCDRBGState *state,
    size_t output_size,
    void *output,
    size_t additional_size,
    const void *additional
);
int ccdrbg_nisthmac_reseed(
    AVPCCDRBGState *state,
    size_t entropy_size,
    const void *entropy,
    size_t additional_size,
    const void *additional
);

