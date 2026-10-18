#pragma once

#include "avp/platform/platform_entropy.h"

#include "mbedtls/private/hmac_drbg.h"


enum {
    RUNTIME_PRNG_RESEED_INTERVAL = 10000,
    RUNTIME_PRNG_MBEDTLS_HEAP_SIZE = 2048,
};

typedef enum {
    RUNTIME_PRNG_STATUS_OK = 0,
    RUNTIME_PRNG_STATUS_INVALID_ARGUMENT,
    RUNTIME_PRNG_STATUS_INVALID_STATE,
    RUNTIME_PRNG_STATUS_ENTROPY_FAILED,
    RUNTIME_PRNG_STATUS_CRYPTO_FAILED,
} RuntimePRNGStatus;

typedef struct {
    mbedtls_hmac_drbg_context drbg;
    PlatformEntropy *entropy;
    bool initialized;
} RuntimePRNG;

void runtime_prng_platform_initialize(void);
PlatformEntropy *runtime_prng_platform_entropy(void);
RuntimePRNG *runtime_prng_global(void);

RuntimePRNGStatus runtime_prng_initialize(
    RuntimePRNG *prng,
    PlatformEntropy *entropy,
    const uint8_t *personalization,
    size_t personalization_size
);
void runtime_prng_deinitialize(RuntimePRNG *prng);
RuntimePRNGStatus runtime_prng_reseed(
    RuntimePRNG *prng,
    const uint8_t *additional,
    size_t additional_size
);
RuntimePRNGStatus runtime_prng_generate(
    RuntimePRNG *prng,
    uint8_t *output,
    size_t size
);
RuntimePRNGStatus runtime_prng_generate_with_additional(
    RuntimePRNG *prng,
    uint8_t *output,
    size_t size,
    const uint8_t *additional,
    size_t additional_size
);
