#define MBEDTLS_ALLOW_PRIVATE_ACCESS
#include "avp/crypto/support/corecrypto_mbedtls.h"

#include "mbedtls/platform_util.h"

#include <limits.h>


#define AVP_CORECRYPTO_ENTRY __attribute__((used, retain))

const AVPCCDigestInfo avp_ccsha384_mbedtls_di AVP_CORECRYPTO_ENTRY = {
    .md_type = MBEDTLS_MD_SHA384,
    .output_size = AVP_CCSHA384_OUTPUT_SIZE,
    .block_size = AVP_CCSHA384_BLOCK_SIZE,
};

static int digest_info_is_supported(const AVPCCDigestInfo *info) {
    return info != 0 && info->md_type == MBEDTLS_MD_SHA384 &&
        info->output_size == AVP_CCSHA384_OUTPUT_SIZE &&
        info->block_size == AVP_CCSHA384_BLOCK_SIZE;
}

/*
 * Volatile reads preserve the full-buffer access contract of the public
 * TF-PSA-Crypto constant-time comparison routine under optimization.
 */
static int compare_all_bytes(const void *left, const void *right, size_t count) {
    const volatile unsigned char *left_bytes = left;
    const volatile unsigned char *right_bytes = right;
    uint32_t difference = 0;

    for (size_t index = 0; index < count; ++index) {
        difference |= (uint32_t)(left_bytes[index] ^ right_bytes[index]);
    }

#if INT_MAX < INT32_MAX
#error "cc_cmp_safe requires at least 32-bit int"
#else
    return (int)((difference & UINT32_C(0xffff)) | (difference >> 16));
#endif
}

int AVP_CORECRYPTO_ENTRY cc_cmp_safe_internal(
    size_t count,
    const void *left,
    const void *right
) {
    if (count == 0) {
        return 1;
    }
    return compare_all_bytes(left, right, count) != 0;
}

int AVP_CORECRYPTO_ENTRY cc_cmp_safe(
    size_t count,
    const void *left,
    const void *right
) {
    return cc_cmp_safe_internal(count, left, right);
}

int AVP_CORECRYPTO_ENTRY ccdigest_init_internal(
    const AVPCCDigestInfo *info,
    AVPCCDigestContext *context
) {
    if (!digest_info_is_supported(info) || context == 0) {
        return MBEDTLS_ERR_MD_BAD_INPUT_DATA;
    }
    mbedtls_md_init(&context->md);
    const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(info->md_type);
    int status = mbedtls_md_setup(&context->md, md_info, 0);
    if (status == 0) {
        status = mbedtls_md_starts(&context->md);
    }
    if (status != 0) {
        mbedtls_md_free(&context->md);
    }
    context->initialized = status == 0;
    return status;
}

int AVP_CORECRYPTO_ENTRY ccdigest_update_internal(
    const AVPCCDigestInfo *info,
    AVPCCDigestContext *context,
    size_t input_size,
    const void *input
) {
    if (!digest_info_is_supported(info) || context == 0 ||
        !context->initialized || (input == 0 && input_size != 0)) {
        return MBEDTLS_ERR_MD_BAD_INPUT_DATA;
    }
    return mbedtls_md_update(&context->md, input, input_size);
}

int ccdigest_finish_mbedtls(
    const AVPCCDigestInfo *info,
    AVPCCDigestContext *context,
    void *digest
) {
    if (!digest_info_is_supported(info) || context == 0 ||
        !context->initialized || digest == 0) {
        return MBEDTLS_ERR_MD_BAD_INPUT_DATA;
    }
    return mbedtls_md_finish(&context->md, digest);
}

void ccdigest_free_mbedtls(AVPCCDigestContext *context) {
    if (context == 0) {
        return;
    }
    mbedtls_md_free(&context->md);
    context->initialized = false;
}

int AVP_CORECRYPTO_ENTRY ccdigest_internal(
    const AVPCCDigestInfo *info,
    size_t input_size,
    const void *input,
    void *digest
) {
    if (!digest_info_is_supported(info) || digest == 0 ||
        (input == 0 && input_size != 0)) {
        return MBEDTLS_ERR_MD_BAD_INPUT_DATA;
    }
    const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(info->md_type);
    return mbedtls_md(md_info, input, input_size, digest);
}

int AVP_CORECRYPTO_ENTRY ccdigest(
    const AVPCCDigestInfo *info,
    size_t input_size,
    const void *input,
    void *digest
) {
    return ccdigest_internal(info, input_size, input, digest);
}

int AVP_CORECRYPTO_ENTRY cchmac_init_internal(
    const AVPCCDigestInfo *info,
    AVPCCHmacContext *context,
    size_t key_size,
    const void *key
) {
    if (!digest_info_is_supported(info) || context == 0 ||
        (key == 0 && key_size != 0)) {
        return MBEDTLS_ERR_MD_BAD_INPUT_DATA;
    }

    mbedtls_md_init(&context->md);
    const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(info->md_type);
    int status = mbedtls_md_setup(&context->md, md_info, 0);
    if (status == 0) {
        status = mbedtls_md_hmac_setup(&context->md, md_info);
    }
    if (status == 0) {
        status = mbedtls_md_hmac_starts(&context->md, key, key_size);
    }
    if (status != 0) {
        mbedtls_md_free(&context->md);
    }
    context->initialized = status == 0;
    return status;
}

int AVP_CORECRYPTO_ENTRY cchmac_update_internal(
    const AVPCCDigestInfo *info,
    AVPCCHmacContext *context,
    size_t input_size,
    const void *input
) {
    if (!digest_info_is_supported(info) || context == 0 ||
        !context->initialized || (input == 0 && input_size != 0)) {
        return MBEDTLS_ERR_MD_BAD_INPUT_DATA;
    }
    return mbedtls_md_hmac_update(&context->md, input, input_size);
}

int AVP_CORECRYPTO_ENTRY cchmac_final_internal(
    const AVPCCDigestInfo *info,
    AVPCCHmacContext *context,
    unsigned char *mac
) {
    if (!digest_info_is_supported(info) || context == 0 ||
        !context->initialized || mac == 0) {
        return MBEDTLS_ERR_MD_BAD_INPUT_DATA;
    }
    return mbedtls_md_hmac_finish(&context->md, mac);
}

void cchmac_free_mbedtls(AVPCCHmacContext *context) {
    if (context == 0) {
        return;
    }
    mbedtls_md_free(&context->md);
    context->initialized = false;
}

int AVP_CORECRYPTO_ENTRY cchmac_internal(
    const AVPCCDigestInfo *info,
    size_t key_size,
    const void *key,
    size_t input_size,
    const void *input,
    unsigned char *mac
) {
    if (!digest_info_is_supported(info) || mac == 0 ||
        (key == 0 && key_size != 0) ||
        (input == 0 && input_size != 0)) {
        return MBEDTLS_ERR_MD_BAD_INPUT_DATA;
    }
    const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(info->md_type);
    return mbedtls_md_hmac(
        md_info,
        key,
        key_size,
        input,
        input_size,
        mac
    );
}
