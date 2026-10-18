#include "avp/runtime/prng/runtime_prng_recovered.h"

#include "avp/runtime/memory/allocator/runtime_allocator.h"
#include "avp/runtime/memory/allocator/runtime_allocator_recovered.h"
#include "mbedtls/md.h"
#include "mbedtls/platform_util.h"


typedef struct {
    RuntimePRNGConfigurationRead read;
    void *context;
    RuntimePRNGRecoveredConfiguration configuration;
    RuntimePRNG *cache_owner;
    PlatformEntropy *cache_entropy;
    uint8_t cache[RUNTIME_PRNG_SHA384_OUTPUT_SIZE];
    size_t cache_cursor;
    size_t cache_available;
    uint32_t refills_since_reseed;
} RuntimePRNGRecoveredState;

#if defined(__ELF__)
#define RUNTIME_PRNG_RECOVERED_STATE \
    __attribute__((section(".runtime_prng.state"), used))
#else
#define RUNTIME_PRNG_RECOVERED_STATE
#endif

/* The AVPBooter-compatible ABI publishes an 0x50-byte descriptor object.
 * Keeping the public Mbed TLS implementation pointer inside that envelope avoids exposing
 * the smaller private mbedtls_md_info_t allocation as an oversized span. */
typedef struct {
    const mbedtls_md_info_t *implementation;
    uint32_t output_size;
    uint32_t block_size;
    uint8_t reserved[RUNTIME_PRNG_SHA384_DIGEST_INFO_SIZE -
                     sizeof(const mbedtls_md_info_t *) - 2u * sizeof(uint32_t)];
} RuntimePRNGSHA384DigestInfo;

_Static_assert(sizeof(RuntimePRNGSHA384DigestInfo) ==
                   RUNTIME_PRNG_SHA384_DIGEST_INFO_SIZE,
               "recovered SHA-384 descriptor size");

static const RuntimeFirebloomType byte_type = {
    .identity = UINT64_C(0x00144f18),
    .element_size = 1,
    .bytewise_copyable = true,
};

static const RuntimeFirebloomType u32_type = {
    .identity = UINT64_C(0x00144f00),
    .element_size = sizeof(uint32_t),
    .bytewise_copyable = true,
};

static const RuntimeFirebloomType digest_info_type = {
    .identity = UINT64_C(0x001451f0),
    .element_size = RUNTIME_PRNG_SHA384_DIGEST_INFO_SIZE,
    .bytewise_copyable = false,
};

static RuntimePRNGSHA384DigestInfo digest_info;
static RuntimePRNGRecoveredState recovered_state RUNTIME_PRNG_RECOVERED_STATE;

static bool span_is_ordered(RuntimePRNGRecoveredSpan span) {
    return span.lower != 0 && (uintptr_t)span.cursor >= (uintptr_t)span.lower &&
        (uintptr_t)span.upper >= (uintptr_t)span.cursor;
}

static size_t span_available(RuntimePRNGRecoveredSpan span) {
    return (size_t)((uintptr_t)span.upper - (uintptr_t)span.cursor);
}

static bool span_has_type(
    RuntimePRNGRecoveredSpan span,
    const RuntimeFirebloomType *expected
) {
    const RuntimeFirebloomType *actual =
        (const RuntimeFirebloomType *)span.type;
    return actual != 0 &&
        (actual == expected ||
         (actual->identity == expected->identity &&
          actual->element_size == expected->element_size));
}

static void clear_cache(void) {
    mbedtls_platform_zeroize(
        recovered_state.cache,
        sizeof(recovered_state.cache)
    );
    recovered_state.cache_owner = 0;
    recovered_state.cache_entropy = 0;
    recovered_state.cache_cursor = 0;
    recovered_state.cache_available = 0;
    recovered_state.refills_since_reseed = 0;
}

_Noreturn void runtime_prng_entropy_workspace_size_fatal(void) {
    __builtin_trap();
}

RuntimePRNGRecoveredSpan runtime_prng_sha384_digest_info_span(void) {
    const mbedtls_md_info_t *info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA384);
    if (info == 0) {
        runtime_prng_entropy_workspace_size_fatal();
    }
    digest_info.implementation = info;
    digest_info.output_size = RUNTIME_PRNG_SHA384_OUTPUT_SIZE;
    digest_info.block_size = 128;
    uint8_t *lower = (uint8_t *)(void *)&digest_info;
    return (RuntimePRNGRecoveredSpan){
        .lower = lower,
        .cursor = lower,
        .upper = (uint8_t *)((uintptr_t)lower +
            RUNTIME_PRNG_SHA384_DIGEST_INFO_SIZE),
        .type = (uintptr_t)&digest_info_type,
    };
}

const RuntimeFirebloomType *runtime_prng_byte_type(void) {
    return &byte_type;
}

const RuntimeFirebloomType *runtime_prng_u32_type(void) {
    return &u32_type;
}

void runtime_prng_recovered_bind_configuration_reader(
    RuntimePRNGConfigurationRead read,
    void *context
) {
    recovered_state.read = read;
    recovered_state.context = context;
    recovered_state.configuration.prepared = false;
    clear_cache();
}

uint32_t runtime_prng_read_configuration_u32(uint32_t key) {
    if (recovered_state.read != 0) {
        return recovered_state.read(key, recovered_state.context);
    }
    if (key == RUNTIME_PRNG_CONFIGURATION_ENTROPY_LENGTH) {
        return 32;
    }
    if (key == RUNTIME_PRNG_CONFIGURATION_RESEED_INTERVAL) {
        return RUNTIME_PRNG_RESEED_INTERVAL;
    }
    return 0;
}

const RuntimePRNGRecoveredConfiguration *
runtime_prng_prepare_hmac_drbg_configuration(void) {
    if (!recovered_state.configuration.prepared) {
        const RuntimePRNGRecoveredSpan digest =
            runtime_prng_sha384_digest_info_span();
        if (digest.lower == 0 ||
            span_available(digest) != RUNTIME_PRNG_SHA384_DIGEST_INFO_SIZE) {
            runtime_prng_entropy_workspace_size_fatal();
        }
        recovered_state.configuration.entropy_length =
            runtime_prng_read_configuration_u32(
                RUNTIME_PRNG_CONFIGURATION_ENTROPY_LENGTH
            );
        recovered_state.configuration.reseed_interval =
            runtime_prng_read_configuration_u32(
                RUNTIME_PRNG_CONFIGURATION_RESEED_INTERVAL
            );
        if (recovered_state.configuration.entropy_length == 0 ||
            recovered_state.configuration.reseed_interval == 0) {
            runtime_prng_entropy_workspace_size_fatal();
        }
        recovered_state.configuration.prepared = true;
    }
    return &recovered_state.configuration;
}

void runtime_prng_fill_entropy_words(
    PlatformEntropy *entropy,
    RuntimePRNGRecoveredSpan workspace,
    size_t size
) {
    workspace = runtime_prng_validate_u32_tuple(workspace);
    if ((size & 3u) != 0 || span_available(workspace) < size || entropy == 0 ||
        platform_entropy_request(entropy, workspace.cursor, size) !=
            PLATFORM_ENTROPY_STATUS_OK) {
        runtime_prng_entropy_workspace_size_fatal();
    }
}

RuntimePRNGStatus runtime_prng_initialize_with_entropy_workspace(
    RuntimePRNG *prng,
    PlatformEntropy *entropy,
    RuntimePRNGRecoveredSpan workspace,
    const uint8_t *personalization,
    size_t personalization_size
) {
    const RuntimePRNGRecoveredConfiguration *configuration =
        runtime_prng_prepare_hmac_drbg_configuration();
    const size_t workspace_size = (size_t)configuration->entropy_length * 2u;
    workspace = runtime_prng_validate_u32_tuple(workspace);
    if (span_available(workspace) < workspace_size) {
        runtime_prng_entropy_workspace_size_fatal();
    }
    runtime_prng_fill_entropy_words(entropy, workspace, workspace_size);
    const RuntimePRNGStatus status = runtime_prng_initialize(
        prng,
        entropy,
        personalization,
        personalization_size
    );
    if (status != RUNTIME_PRNG_STATUS_OK) {
        return status;
    }
    mbedtls_hmac_drbg_set_entropy_len(
        &prng->drbg,
        configuration->entropy_length
    );
    mbedtls_hmac_drbg_set_reseed_interval(
        &prng->drbg,
        (int)configuration->reseed_interval
    );
    if (mbedtls_hmac_drbg_update(
            &prng->drbg,
            workspace.cursor,
            workspace_size
        ) != 0) {
        runtime_prng_deinitialize(prng);
        return RUNTIME_PRNG_STATUS_CRYPTO_FAILED;
    }
    /* Initialization consumes one DRBG block to prime the cache, so
     * the first request observes the same stream position as AVPBooter. */
    clear_cache();
    recovered_state.cache_owner = prng;
    recovered_state.cache_entropy = entropy;
    const RuntimePRNGStatus cache_status = runtime_prng_generate_with_additional(
        prng,
        recovered_state.cache,
        sizeof(recovered_state.cache),
        0,
        0
    );
    if (cache_status != RUNTIME_PRNG_STATUS_OK) {
        runtime_prng_deinitialize(prng);
        clear_cache();
        return cache_status;
    }
    recovered_state.cache_cursor = 0;
    recovered_state.cache_available = sizeof(recovered_state.cache);
    recovered_state.refills_since_reseed = 1;
    return RUNTIME_PRNG_STATUS_OK;
}

RuntimePRNGStatus runtime_prng_initialize_allocating(
    RuntimePRNG *prng,
    PlatformEntropy *entropy,
    const uint8_t *personalization,
    size_t personalization_size
) {
    const RuntimePRNGRecoveredConfiguration *configuration =
        runtime_prng_prepare_hmac_drbg_configuration();
    size_t workspace_size = 0;
    if (__builtin_mul_overflow(
            (size_t)configuration->entropy_length,
            (size_t)2,
            &workspace_size
        )) {
        runtime_prng_entropy_workspace_size_fatal();
    }
    uint8_t *workspace = runtime_malloc(workspace_size);
    if (workspace == 0) {
        runtime_prng_entropy_workspace_size_fatal();
    }
    const RuntimePRNGRecoveredSpan span = {
        .lower = workspace,
        .cursor = workspace,
        .upper = workspace + workspace_size,
        .type = (uintptr_t)&u32_type,
    };
    const RuntimePRNGStatus status = runtime_prng_initialize_with_entropy_workspace(
        prng,
        entropy,
        span,
        personalization,
        personalization_size
    );
    mbedtls_platform_zeroize(workspace, workspace_size);
    runtime_allocator_release_address(workspace);
    return status;
}

static RuntimePRNGStatus refill_cache(
    RuntimePRNG *prng,
    PlatformEntropy *entropy,
    const uint8_t *additional,
    size_t additional_size
) {
    const RuntimePRNGRecoveredConfiguration *configuration =
        runtime_prng_prepare_hmac_drbg_configuration();
    prng->entropy = entropy;
    if (recovered_state.refills_since_reseed >=
        configuration->reseed_interval) {
        const RuntimePRNGStatus status = runtime_prng_reseed(
            prng,
            additional,
            additional_size
        );
        if (status != RUNTIME_PRNG_STATUS_OK) {
            return status;
        }
        additional = 0;
        additional_size = 0;
        recovered_state.refills_since_reseed = 0;
    }
    const RuntimePRNGStatus status = runtime_prng_generate_with_additional(
        prng,
        recovered_state.cache,
        sizeof(recovered_state.cache),
        additional,
        additional_size
    );
    if (status == RUNTIME_PRNG_STATUS_OK) {
        recovered_state.cache_cursor = 0;
        recovered_state.cache_available = sizeof(recovered_state.cache);
        ++recovered_state.refills_since_reseed;
    }
    return status;
}

RuntimePRNGStatus runtime_prng_generate_with_entropy_workspace(
    RuntimePRNG *prng,
    PlatformEntropy *entropy,
    RuntimePRNGRecoveredSpan output,
    size_t size,
    const uint8_t *additional,
    size_t additional_size
) {
    output = runtime_prng_validate_byte_tuple(output);
    if (span_available(output) < size) {
        runtime_prng_entropy_workspace_size_fatal();
    }
    if (prng == 0 || entropy == 0 ||
        (additional == 0 && additional_size != 0)) {
        return RUNTIME_PRNG_STATUS_INVALID_ARGUMENT;
    }
    if (!prng->initialized) {
        return RUNTIME_PRNG_STATUS_INVALID_STATE;
    }
    if (recovered_state.cache_owner != prng ||
        recovered_state.cache_entropy != entropy || additional_size != 0) {
        /* Additional input belongs to exactly one DRBG generation. Discarding
         * older cached bytes preserves that binding across short reads. */
        clear_cache();
        recovered_state.cache_owner = prng;
        recovered_state.cache_entropy = entropy;
    }
    size_t written = 0;
    while (written < size) {
        if (recovered_state.cache_available == 0) {
            const RuntimePRNGStatus status = refill_cache(
                prng,
                entropy,
                additional,
                additional_size
            );
            if (status != RUNTIME_PRNG_STATUS_OK) {
                clear_cache();
                return status;
            }
            additional = 0;
            additional_size = 0;
        }
        const size_t remaining = size - written;
        const size_t copied = remaining < recovered_state.cache_available
            ? remaining
            : recovered_state.cache_available;
        __builtin_memcpy(
            output.cursor + written,
            recovered_state.cache + recovered_state.cache_cursor,
            copied
        );
        mbedtls_platform_zeroize(
            recovered_state.cache + recovered_state.cache_cursor,
            copied
        );
        recovered_state.cache_cursor += copied;
        recovered_state.cache_available -= copied;
        written += copied;
    }
    return RUNTIME_PRNG_STATUS_OK;
}

RuntimePRNGRecoveredSpan runtime_prng_validate_byte_tuple(
    RuntimePRNGRecoveredSpan span
) {
    if (!span_is_ordered(span) || !span_has_type(span, &byte_type)) {
        runtime_prng_entropy_workspace_size_fatal();
    }
    span.type = (uintptr_t)&byte_type;
    return span;
}

RuntimePRNGRecoveredSpan runtime_prng_validate_u32_tuple(
    RuntimePRNGRecoveredSpan span
) {
    if (!span_is_ordered(span) ||
        (((uintptr_t)span.lower | (uintptr_t)span.cursor |
          (uintptr_t)span.upper) & (_Alignof(uint32_t) - 1u)) != 0 ||
        !span_has_type(span, &u32_type)) {
        runtime_prng_entropy_workspace_size_fatal();
    }
    span.type = (uintptr_t)&u32_type;
    return span;
}

uint64_t runtime_prng_source_token(void) {
    return UINT64_C(0x083eeee7ebe28907);
}

RuntimePRNGRecoveredTuple runtime_prng_restore_saved_tuple_x22_x19(
    RuntimePRNGRecoveredTuple saved
) {
    return saved;
}

RuntimePRNGRecoveredArguments runtime_prng_clear_additional_input_arguments(
    RuntimePRNGRecoveredArguments arguments
) {
    arguments.x4 = 0;
    arguments.x5 = 0;
    return arguments;
}
