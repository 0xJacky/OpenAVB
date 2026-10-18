#include "avp/crypto/drbg/corecrypto_hmac_drbg.h"

#include "mbedtls/platform_util.h"

#include <stdarg.h>
#include <string.h>


#define AVP_CCDRBG_ENTRY __attribute__((used, retain))

_Static_assert(
    sizeof(AVPCCDRBGState) + sizeof(AVPCCDRBGNISTHMACCustom) == 0xa0,
    "NIST HMAC_DRBG context contract changed"
);

typedef struct {
    size_t size;
    const void *bytes;
} DRBGInput;

static bool input_is_valid(size_t size, const void *bytes) {
    return size == 0 || bytes != 0;
}

static bool state_is_valid(const AVPCCDRBGState *state) {
    return state != 0 && state->custom != 0 &&
        state->custom->digest_info == &avp_ccsha384_mbedtls_di;
}

static int hmac_status(int status) {
    return status == 0 ? AVP_CCDRBG_STATUS_OK : AVP_CCDRBG_STATUS_ERROR;
}

static int update_inputs(
    AVPCCDRBGState *state,
    unsigned input_count,
    const DRBGInput *inputs
) {
    const AVPCCDigestInfo *digest_info = state->custom->digest_info;
    bool has_input = false;
    for (unsigned index = 0; index < input_count; ++index) {
        if (!input_is_valid(inputs[index].size, inputs[index].bytes)) {
            return AVP_CCDRBG_STATUS_PARAM_ERROR;
        }
        has_input = has_input || inputs[index].size != 0;
    }

    const unsigned rounds = has_input ? 2 : 1;
    for (unsigned round = 0; round < rounds; ++round) {
        AVPCCHmacContext hmac;
        int status = cchmac_init_internal(
            digest_info,
            &hmac,
            digest_info->output_size,
            state->key
        );
        if (status == 0) {
            status = cchmac_update_internal(
                digest_info,
                &hmac,
                digest_info->output_size,
                state->value
            );
        }
        const uint8_t separator = (uint8_t)round;
        if (status == 0) {
            status = cchmac_update_internal(
                digest_info,
                &hmac,
                sizeof(separator),
                &separator
            );
        }
        for (unsigned index = 0; status == 0 && index < input_count; ++index) {
            status = cchmac_update_internal(
                digest_info,
                &hmac,
                inputs[index].size,
                inputs[index].bytes
            );
        }
        if (status == 0) {
            status = cchmac_final_internal(
                digest_info,
                &hmac,
                state->key
            );
        }
        cchmac_free_mbedtls(&hmac);
        if (status != 0) {
            return hmac_status(status);
        }
        status = cchmac_internal(
            digest_info,
            digest_info->output_size,
            state->key,
            digest_info->output_size,
            state->value,
            state->value
        );
        if (status != 0) {
            return hmac_status(status);
        }
    }
    return AVP_CCDRBG_STATUS_OK;
}

int AVP_CCDRBG_ENTRY ccdrbg_init(
    const AVPCCDRBGInfo *info,
    AVPCCDRBGState *state,
    size_t entropy_size,
    const void *entropy,
    size_t nonce_size,
    const void *nonce,
    size_t personalization_size,
    const void *personalization
) {
    return info->init(
        info,
        state,
        entropy_size,
        entropy,
        nonce_size,
        nonce,
        personalization_size,
        personalization
    );
}

int AVP_CCDRBG_ENTRY ccdrbg_reseed(
    const AVPCCDRBGInfo *info,
    AVPCCDRBGState *state,
    size_t entropy_size,
    const void *entropy,
    size_t additional_size,
    const void *additional
) {
    return info->reseed(
        state,
        entropy_size,
        entropy,
        additional_size,
        additional
    );
}

int AVP_CCDRBG_ENTRY ccdrbg_generate(
    const AVPCCDRBGInfo *info,
    AVPCCDRBGState *state,
    size_t output_size,
    void *output,
    size_t additional_size,
    const void *additional
) {
    return info->generate(
        state,
        output_size,
        output,
        additional_size,
        additional
    );
}

size_t AVP_CCDRBG_ENTRY ccdrbg_context_size(const AVPCCDRBGInfo *info) {
    return info->size;
}

void AVP_CCDRBG_ENTRY ccdrbg_factory_nisthmac(
    AVPCCDRBGInfo *info,
    const AVPCCDRBGNISTHMACCustom *custom
) {
    info->size = sizeof(AVPCCDRBGState) + sizeof(*custom);
    info->init = ccdrbg_nisthmac_init;
    info->reseed = ccdrbg_nisthmac_reseed;
    info->generate = ccdrbg_nisthmac_generate;
    info->done = ccdrbg_nisthmac_done;
    info->custom = custom;
    info->must_reseed = ccdrbg_nisthmac_must_reseed;
}

void AVP_CCDRBG_ENTRY ccdrbg_nisthmac_done(AVPCCDRBGState *state) {
    mbedtls_platform_zeroize(state->key, sizeof(state->key));
    mbedtls_platform_zeroize(state->value, sizeof(state->value));
    state->reseed_counter = UINT64_MAX;
}

bool AVP_CCDRBG_ENTRY ccdrbg_nisthmac_must_reseed(
    const AVPCCDRBGState *state
) {
    return state->custom->strict_fips &&
        state->reseed_counter > AVP_CCDRBG_RESEED_INTERVAL;
}

int AVP_CCDRBG_ENTRY ccdrbg_nisthmac_update(
    AVPCCDRBGState *state,
    unsigned input_count,
    ...
) {
    if (!state_is_valid(state) || input_count > 3) {
        return AVP_CCDRBG_STATUS_PARAM_ERROR;
    }
    DRBGInput inputs[3];
    va_list arguments;
    va_start(arguments, input_count);
    for (unsigned index = 0; index < input_count; ++index) {
        inputs[index].size = va_arg(arguments, size_t);
        inputs[index].bytes = va_arg(arguments, const void *);
    }
    va_end(arguments);
    return update_inputs(state, input_count, inputs);
}

int AVP_CCDRBG_ENTRY ccdrbg_nisthmac_init(
    const AVPCCDRBGInfo *info,
    AVPCCDRBGState *state,
    size_t entropy_size,
    const void *entropy,
    size_t nonce_size,
    const void *nonce,
    size_t personalization_size,
    const void *personalization
) {
    if (info == 0 || state == 0 || info->custom == 0 ||
        info->custom->digest_info != &avp_ccsha384_mbedtls_di ||
        entropy_size < AVP_CCSHA384_OUTPUT_SIZE / 2 ||
        entropy_size > AVP_CCDRBG_MAX_ENTROPY_SIZE ||
        personalization_size > AVP_CCDRBG_MAX_PERSONALIZATION_SIZE ||
        !input_is_valid(entropy_size, entropy) ||
        !input_is_valid(nonce_size, nonce) ||
        !input_is_valid(personalization_size, personalization)) {
        return AVP_CCDRBG_STATUS_PARAM_ERROR;
    }

    state->custom = info->custom;
    memset(state->key, 0, sizeof(state->key));
    memset(state->value, 0, sizeof(state->value));
    memset(state->value, 1, info->custom->digest_info->output_size);
    const int status = ccdrbg_nisthmac_update(
        state,
        3,
        entropy_size,
        entropy,
        nonce_size,
        nonce,
        personalization_size,
        personalization
    );
    if (status != AVP_CCDRBG_STATUS_OK) {
        ccdrbg_nisthmac_done(state);
        return status;
    }
    state->reseed_counter = 1;
    return AVP_CCDRBG_STATUS_OK;
}

int AVP_CCDRBG_ENTRY ccdrbg_nisthmac_generate(
    AVPCCDRBGState *state,
    size_t output_size,
    void *output,
    size_t additional_size,
    const void *additional
) {
    if (!state_is_valid(state) || output_size > AVP_CCDRBG_MAX_REQUEST_SIZE ||
        additional_size > AVP_CCDRBG_MAX_ADDITIONAL_INPUT_SIZE ||
        !input_is_valid(output_size, output) ||
        !input_is_valid(additional_size, additional)) {
        return AVP_CCDRBG_STATUS_PARAM_ERROR;
    }
    if (ccdrbg_nisthmac_must_reseed(state)) {
        return AVP_CCDRBG_STATUS_NEED_RESEED;
    }
    if (additional_size != 0) {
        const int status = ccdrbg_nisthmac_update(
            state,
            1,
            additional_size,
            additional
        );
        if (status != AVP_CCDRBG_STATUS_OK) {
            return status;
        }
    }

    uint8_t previous[AVP_CCSHA384_OUTPUT_SIZE];
    uint8_t *output_bytes = output;
    size_t remaining = output_size;
    const size_t digest_size = state->custom->digest_info->output_size;
    while (remaining != 0) {
        memcpy(previous, state->value, digest_size);
        const int status = cchmac_internal(
            state->custom->digest_info,
            digest_size,
            state->key,
            digest_size,
            state->value,
            state->value
        );
        if (status != 0) {
            mbedtls_platform_zeroize(previous, sizeof(previous));
            return hmac_status(status);
        }
        if (cc_cmp_safe_internal(
            digest_size,
            previous,
            state->value
        ) == 0) {
            ccdrbg_nisthmac_done(state);
            mbedtls_platform_zeroize(previous, sizeof(previous));
            return AVP_CCDRBG_STATUS_ABORT;
        }
        const size_t chunk = remaining < digest_size
            ? remaining
            : digest_size;
        memcpy(output_bytes, state->value, chunk);
        output_bytes += chunk;
        remaining -= chunk;
    }
    mbedtls_platform_zeroize(previous, sizeof(previous));

    const int status = ccdrbg_nisthmac_update(
        state,
        1,
        additional_size,
        additional
    );
    if (status == AVP_CCDRBG_STATUS_OK) {
        state->reseed_counter += 1;
    }
    return status;
}

int AVP_CCDRBG_ENTRY ccdrbg_nisthmac_reseed(
    AVPCCDRBGState *state,
    size_t entropy_size,
    const void *entropy,
    size_t additional_size,
    const void *additional
) {
    if (!state_is_valid(state) ||
        entropy_size < AVP_CCSHA384_OUTPUT_SIZE / 2 ||
        entropy_size > AVP_CCDRBG_MAX_ENTROPY_SIZE ||
        additional_size > AVP_CCDRBG_MAX_ADDITIONAL_INPUT_SIZE ||
        !input_is_valid(entropy_size, entropy) ||
        !input_is_valid(additional_size, additional)) {
        return AVP_CCDRBG_STATUS_PARAM_ERROR;
    }
    const int status = ccdrbg_nisthmac_update(
        state,
        2,
        entropy_size,
        entropy,
        additional_size,
        additional
    );
    if (status == AVP_CCDRBG_STATUS_OK) {
        state->reseed_counter = 1;
    }
    return status;
}
