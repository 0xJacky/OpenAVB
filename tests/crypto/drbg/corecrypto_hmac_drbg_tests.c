#define MBEDTLS_ALLOW_PRIVATE_ACCESS
#include "avp/crypto/drbg/corecrypto_hmac_drbg.h"

#include "mbedtls/memory_buffer_alloc.h"
#include "mbedtls/private/hmac_drbg.h"

#include <assert.h>
#include <string.h>


static AVPCCDRBGNISTHMACCustom strict_custom = {
    .digest_info = &avp_ccsha384_mbedtls_di,
    .strict_fips = true,
};

static void seed_reference(
    mbedtls_hmac_drbg_context *reference,
    const uint8_t *entropy,
    size_t entropy_size,
    const uint8_t *nonce,
    size_t nonce_size,
    const uint8_t *personalization,
    size_t personalization_size
) {
    uint8_t seed[96];
    size_t seed_size = 0;
    memcpy(seed + seed_size, entropy, entropy_size);
    seed_size += entropy_size;
    if (nonce_size != 0) {
        memcpy(seed + seed_size, nonce, nonce_size);
    }
    seed_size += nonce_size;
    if (personalization_size != 0) {
        memcpy(seed + seed_size, personalization, personalization_size);
    }
    seed_size += personalization_size;

    mbedtls_hmac_drbg_init(reference);
    assert(mbedtls_hmac_drbg_seed_buf(
        reference,
        mbedtls_md_info_from_type(MBEDTLS_MD_SHA384),
        seed,
        seed_size
    ) == 0);
}

static void test_factory_and_dispatch(void) {
    AVPCCDRBGInfo info;
    ccdrbg_factory_nisthmac(&info, &strict_custom);
    assert(ccdrbg_context_size(&info) == 0xa0);
    assert(info.init == ccdrbg_nisthmac_init);
    assert(info.reseed == ccdrbg_nisthmac_reseed);
    assert(info.generate == ccdrbg_nisthmac_generate);
    assert(info.done == ccdrbg_nisthmac_done);
    assert(info.must_reseed == ccdrbg_nisthmac_must_reseed);
}

static void test_generation_matches_mbedtls(void) {
    uint8_t entropy[32];
    uint8_t nonce[16];
    static const uint8_t personalization[] = "OpenBootROM DRBG";
    static const uint8_t additional[] = "generation input";
    for (size_t index = 0; index < sizeof(entropy); ++index) {
        entropy[index] = (uint8_t)(index * 7 + 3);
    }
    for (size_t index = 0; index < sizeof(nonce); ++index) {
        nonce[index] = (uint8_t)(index * 5 + 1);
    }

    AVPCCDRBGInfo info;
    AVPCCDRBGState state;
    ccdrbg_factory_nisthmac(&info, &strict_custom);
    assert(ccdrbg_init(
        &info,
        &state,
        sizeof(entropy),
        entropy,
        sizeof(nonce),
        nonce,
        sizeof(personalization) - 1,
        personalization
    ) == AVP_CCDRBG_STATUS_OK);

    mbedtls_hmac_drbg_context reference;
    seed_reference(
        &reference,
        entropy,
        sizeof(entropy),
        nonce,
        sizeof(nonce),
        personalization,
        sizeof(personalization) - 1
    );

    uint8_t actual[144];
    uint8_t expected[144];
    assert(ccdrbg_generate(
        &info,
        &state,
        sizeof(actual),
        actual,
        sizeof(additional) - 1,
        additional
    ) == AVP_CCDRBG_STATUS_OK);
    assert(mbedtls_hmac_drbg_random_with_add(
        &reference,
        expected,
        sizeof(expected),
        additional,
        sizeof(additional) - 1
    ) == 0);
    assert(memcmp(actual, expected, sizeof(actual)) == 0);
    assert(state.reseed_counter == 2);
    mbedtls_hmac_drbg_free(&reference);
}

static void test_reseed_matches_mbedtls_update(void) {
    uint8_t entropy[32];
    uint8_t nonce[16];
    uint8_t reseed_entropy[48];
    static const uint8_t additional[] = "reseed input";
    memset(entropy, 0x11, sizeof(entropy));
    memset(nonce, 0x22, sizeof(nonce));
    memset(reseed_entropy, 0x33, sizeof(reseed_entropy));

    AVPCCDRBGInfo info;
    AVPCCDRBGState state;
    ccdrbg_factory_nisthmac(&info, &strict_custom);
    assert(ccdrbg_init(
        &info,
        &state,
        sizeof(entropy),
        entropy,
        sizeof(nonce),
        nonce,
        0,
        0
    ) == AVP_CCDRBG_STATUS_OK);

    mbedtls_hmac_drbg_context reference;
    seed_reference(
        &reference,
        entropy,
        sizeof(entropy),
        nonce,
        sizeof(nonce),
        0,
        0
    );
    uint8_t reseed_input[sizeof(reseed_entropy) + sizeof(additional) - 1];
    memcpy(reseed_input, reseed_entropy, sizeof(reseed_entropy));
    memcpy(
        reseed_input + sizeof(reseed_entropy),
        additional,
        sizeof(additional) - 1
    );

    assert(ccdrbg_reseed(
        &info,
        &state,
        sizeof(reseed_entropy),
        reseed_entropy,
        sizeof(additional) - 1,
        additional
    ) == AVP_CCDRBG_STATUS_OK);
    assert(mbedtls_hmac_drbg_update(
        &reference,
        reseed_input,
        sizeof(reseed_input)
    ) == 0);

    uint8_t actual[96];
    uint8_t expected[96];
    assert(ccdrbg_generate(
        &info,
        &state,
        sizeof(actual),
        actual,
        0,
        0
    ) == AVP_CCDRBG_STATUS_OK);
    assert(mbedtls_hmac_drbg_random(
        &reference,
        expected,
        sizeof(expected)
    ) == 0);
    assert(memcmp(actual, expected, sizeof(actual)) == 0);
    mbedtls_hmac_drbg_free(&reference);
}

static void test_limits_reseed_gate_and_done(void) {
    uint8_t entropy[32] = { 0 };
    AVPCCDRBGInfo info;
    AVPCCDRBGState state;
    ccdrbg_factory_nisthmac(&info, &strict_custom);
    assert(ccdrbg_init(
        &info,
        &state,
        23,
        entropy,
        0,
        0,
        0,
        0
    ) == AVP_CCDRBG_STATUS_PARAM_ERROR);
    assert(ccdrbg_init(
        &info,
        &state,
        sizeof(entropy),
        entropy,
        0,
        0,
        0,
        0
    ) == AVP_CCDRBG_STATUS_OK);
    state.reseed_counter = AVP_CCDRBG_RESEED_INTERVAL + 1;
    assert(ccdrbg_nisthmac_must_reseed(&state));
    uint8_t output;
    assert(ccdrbg_generate(
        &info,
        &state,
        1,
        &output,
        0,
        0
    ) == AVP_CCDRBG_STATUS_NEED_RESEED);

    info.done(&state);
    uint8_t zeroes[64] = { 0 };
    assert(memcmp(state.key, zeroes, sizeof(state.key)) == 0);
    assert(memcmp(state.value, zeroes, sizeof(state.value)) == 0);
    assert(state.reseed_counter == UINT64_MAX);
}

int main(void) {
    static uint8_t heap[16384];
    mbedtls_memory_buffer_alloc_init(heap, sizeof(heap));
    test_factory_and_dispatch();
    test_generation_matches_mbedtls();
    test_reseed_matches_mbedtls_update();
    test_limits_reseed_gate_and_done();
    return 0;
}
