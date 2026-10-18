#pragma once

#include "avp/runtime/firebloom/runtime_firebloom.h"
#include "avp/runtime/prng/runtime_prng.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


enum {
    RUNTIME_PRNG_CONFIGURATION_ENTROPY_LENGTH = 0x0b00,
    RUNTIME_PRNG_CONFIGURATION_RESEED_INTERVAL = 0x0c00,
    RUNTIME_PRNG_SHA384_DIGEST_INFO_SIZE = 0x50,
    RUNTIME_PRNG_SHA384_OUTPUT_SIZE = 48,
};

typedef struct {
    uint8_t *lower;
    uint8_t *cursor;
    uint8_t *upper;
    uintptr_t type;
} RuntimePRNGRecoveredSpan;

typedef struct {
    uint64_t lower;
    uint64_t cursor;
    uint64_t upper;
    uint64_t type;
} RuntimePRNGRecoveredTuple;

typedef struct {
    uint64_t x0;
    uint64_t x1;
    uint64_t x2;
    uint64_t x3;
    uint64_t x4;
    uint64_t x5;
} RuntimePRNGRecoveredArguments;

typedef uint32_t (*RuntimePRNGConfigurationRead)(uint32_t key, void *context);

typedef struct {
    uint32_t entropy_length;
    uint32_t reseed_interval;
    bool prepared;
} RuntimePRNGRecoveredConfiguration;

#define RUNTIME_PRNG_RECOVERED_PUBLIC __attribute__((used, retain))

RUNTIME_PRNG_RECOVERED_PUBLIC _Noreturn void
runtime_prng_entropy_workspace_size_fatal(void);
RUNTIME_PRNG_RECOVERED_PUBLIC RuntimePRNGRecoveredSpan
runtime_prng_sha384_digest_info_span(void);
RUNTIME_PRNG_RECOVERED_PUBLIC const RuntimeFirebloomType *
runtime_prng_byte_type(void);
RUNTIME_PRNG_RECOVERED_PUBLIC const RuntimeFirebloomType *
runtime_prng_u32_type(void);
RUNTIME_PRNG_RECOVERED_PUBLIC RuntimePRNGStatus
runtime_prng_initialize_with_entropy_workspace(
    RuntimePRNG *prng,
    PlatformEntropy *entropy,
    RuntimePRNGRecoveredSpan workspace,
    const uint8_t *personalization,
    size_t personalization_size
);
void runtime_prng_recovered_bind_configuration_reader(
    RuntimePRNGConfigurationRead read,
    void *context
);
RUNTIME_PRNG_RECOVERED_PUBLIC uint32_t
runtime_prng_read_configuration_u32(uint32_t key);
RUNTIME_PRNG_RECOVERED_PUBLIC const RuntimePRNGRecoveredConfiguration *
runtime_prng_prepare_hmac_drbg_configuration(void);
RUNTIME_PRNG_RECOVERED_PUBLIC void runtime_prng_fill_entropy_words(
    PlatformEntropy *entropy,
    RuntimePRNGRecoveredSpan workspace,
    size_t size
);
RUNTIME_PRNG_RECOVERED_PUBLIC RuntimePRNGStatus
runtime_prng_initialize_allocating(
    RuntimePRNG *prng,
    PlatformEntropy *entropy,
    const uint8_t *personalization,
    size_t personalization_size
);
RUNTIME_PRNG_RECOVERED_PUBLIC RuntimePRNGStatus
runtime_prng_generate_with_entropy_workspace(
    RuntimePRNG *prng,
    PlatformEntropy *entropy,
    RuntimePRNGRecoveredSpan output,
    size_t size,
    const uint8_t *additional,
    size_t additional_size
);
RUNTIME_PRNG_RECOVERED_PUBLIC RuntimePRNGRecoveredSpan
runtime_prng_validate_byte_tuple(
    RuntimePRNGRecoveredSpan span
);
RUNTIME_PRNG_RECOVERED_PUBLIC RuntimePRNGRecoveredSpan
runtime_prng_validate_u32_tuple(
    RuntimePRNGRecoveredSpan span
);
RUNTIME_PRNG_RECOVERED_PUBLIC uint64_t runtime_prng_source_token(void);
RUNTIME_PRNG_RECOVERED_PUBLIC RuntimePRNGRecoveredTuple
runtime_prng_restore_saved_tuple_x22_x19(RuntimePRNGRecoveredTuple saved);
RUNTIME_PRNG_RECOVERED_PUBLIC RuntimePRNGRecoveredArguments
runtime_prng_clear_additional_input_arguments(
    RuntimePRNGRecoveredArguments arguments
);
