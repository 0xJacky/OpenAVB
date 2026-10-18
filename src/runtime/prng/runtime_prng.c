#include "avp/runtime/prng/runtime_prng.h"

#include "mbedtls/md.h"


static int entropy_callback(void *context, unsigned char *output, size_t size) {
    PlatformEntropy *entropy = context;
    PlatformEntropyStatus status = platform_entropy_request(
        entropy,
        output,
        size
    );
    if (status == PLATFORM_ENTROPY_STATUS_SOURCE_FAILED) {
        status = platform_entropy_reseed_from_source(entropy);
        if (status == PLATFORM_ENTROPY_STATUS_OK) {
            status = platform_entropy_request(entropy, output, size);
        }
    }
    return status == PLATFORM_ENTROPY_STATUS_OK ? 0 : -1;
}

void runtime_prng_deinitialize(RuntimePRNG *prng) {
    if (prng == 0) {
        return;
    }
    mbedtls_hmac_drbg_free(&prng->drbg);
    prng->entropy = 0;
    prng->initialized = false;
}

static RuntimePRNGStatus crypto_status(int status) {
    if (status == MBEDTLS_ERR_HMAC_DRBG_ENTROPY_SOURCE_FAILED) {
        return RUNTIME_PRNG_STATUS_ENTROPY_FAILED;
    }
    return status == 0
        ? RUNTIME_PRNG_STATUS_OK
        : RUNTIME_PRNG_STATUS_CRYPTO_FAILED;
}

RuntimePRNGStatus runtime_prng_initialize(
    RuntimePRNG *prng,
    PlatformEntropy *entropy,
    const uint8_t *personalization,
    size_t personalization_size
) {
    if (prng == 0 || entropy == 0 ||
        (personalization == 0 && personalization_size != 0)) {
        return RUNTIME_PRNG_STATUS_INVALID_ARGUMENT;
    }
    if (prng->initialized || entropy->state != PLATFORM_ENTROPY_STATE_ENABLED) {
        return RUNTIME_PRNG_STATUS_INVALID_STATE;
    }
    const mbedtls_md_info_t *sha384 = mbedtls_md_info_from_type(
        MBEDTLS_MD_SHA384
    );
    if (sha384 == 0) {
        return RUNTIME_PRNG_STATUS_CRYPTO_FAILED;
    }
    mbedtls_hmac_drbg_init(&prng->drbg);
    mbedtls_hmac_drbg_set_entropy_len(&prng->drbg, 32);
    mbedtls_hmac_drbg_set_reseed_interval(
        &prng->drbg,
        RUNTIME_PRNG_RESEED_INTERVAL
    );
    const int status = mbedtls_hmac_drbg_seed(
        &prng->drbg,
        sha384,
        entropy_callback,
        entropy,
        personalization,
        personalization_size
    );
    if (status != 0) {
        mbedtls_hmac_drbg_free(&prng->drbg);
        return crypto_status(status);
    }
    prng->entropy = entropy;
    prng->initialized = true;
    return RUNTIME_PRNG_STATUS_OK;
}

RuntimePRNGStatus runtime_prng_reseed(
    RuntimePRNG *prng,
    const uint8_t *additional,
    size_t additional_size
) {
    if (prng == 0 || (additional == 0 && additional_size != 0)) {
        return RUNTIME_PRNG_STATUS_INVALID_ARGUMENT;
    }
    if (!prng->initialized) {
        return RUNTIME_PRNG_STATUS_INVALID_STATE;
    }
    if (platform_entropy_reseed_from_source(prng->entropy) !=
            PLATFORM_ENTROPY_STATUS_OK) {
        return RUNTIME_PRNG_STATUS_ENTROPY_FAILED;
    }
    return crypto_status(mbedtls_hmac_drbg_reseed(
        &prng->drbg,
        additional,
        additional_size
    ));
}

RuntimePRNGStatus runtime_prng_generate_with_additional(
    RuntimePRNG *prng,
    uint8_t *output,
    size_t size,
    const uint8_t *additional,
    size_t additional_size
) {
    if (prng == 0 || (output == 0 && size != 0) ||
        (additional == 0 && additional_size != 0)) {
        return RUNTIME_PRNG_STATUS_INVALID_ARGUMENT;
    }
    if (!prng->initialized) {
        return RUNTIME_PRNG_STATUS_INVALID_STATE;
    }
    return crypto_status(mbedtls_hmac_drbg_random_with_add(
        &prng->drbg,
        output,
        size,
        additional,
        additional_size
    ));
}

RuntimePRNGStatus runtime_prng_generate(
    RuntimePRNG *prng,
    uint8_t *output,
    size_t size
) {
    return runtime_prng_generate_with_additional(
        prng,
        output,
        size,
        0,
        0
    );
}
