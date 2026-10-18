/*
 * Copyright (c) 2026 owngoal-dev
 * SPDX-License-Identifier: MIT
 *
 * This file is NOT the upstream TF-PSA-Crypto build_info.h. It was written
 * for OpenAVB and replaces upstream include/tf-psa-crypto/build_info.h
 * (TF-PSA-Crypto commit a92cb6a97d84762d2f4355eae99af7b9ff5c6ae1) with a
 * minimal configuration for the SHA-384 HMAC_DRBG subset. Changed by
 * owngoal-dev, 2026 (Apache-2.0 section 4(b) notice). The other files in
 * vendor/mbedtls are unmodified upstream files under Apache-2.0.
 */
#pragma once


/* OpenAVB enables only the primitives needed by SHA-384 HMAC_DRBG. */
#define MBEDTLS_HMAC_DRBG_C
#define MBEDTLS_MD_C
#define MBEDTLS_MD_LIGHT
#define MBEDTLS_MEMORY_BUFFER_ALLOC_C
#define MBEDTLS_PLATFORM_C
#define MBEDTLS_PLATFORM_EXIT_ALT
#define MBEDTLS_PLATFORM_MEMORY
#define MBEDTLS_PLATFORM_NO_STD_FUNCTIONS
#define MBEDTLS_SHA384_C
#define PSA_WANT_ALG_SHA_384 1

#define MBEDTLS_PLATFORM_STD_CALLOC 0
#define MBEDTLS_PLATFORM_STD_FREE 0
#define MBEDTLS_PLATFORM_STD_EXIT 0
