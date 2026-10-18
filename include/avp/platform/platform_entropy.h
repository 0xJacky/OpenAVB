#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


enum {
    PLATFORM_ENTROPY_SEED_SIZE = 48,
};

typedef enum {
    PLATFORM_ENTROPY_STATE_RESET = 0,
    PLATFORM_ENTROPY_STATE_SEEDED,
    PLATFORM_ENTROPY_STATE_ENABLED,
} PlatformEntropyState;

typedef enum {
    PLATFORM_ENTROPY_STATUS_OK = 0,
    PLATFORM_ENTROPY_STATUS_INVALID_ARGUMENT,
    PLATFORM_ENTROPY_STATUS_INVALID_STATE,
    PLATFORM_ENTROPY_STATUS_SOURCE_FAILED,
} PlatformEntropyStatus;

typedef uint32_t (*PlatformEntropyReadWord)(void *context);

typedef struct {
    PlatformEntropyReadWord read_word;
    void *context;
    PlatformEntropyState state;
    uint8_t seed[PLATFORM_ENTROPY_SEED_SIZE];
    size_t seed_offset;
    uint32_t generation;
    uint8_t control_flags;
} PlatformEntropy;

PlatformEntropyStatus platform_entropy_initialize(
    PlatformEntropy *entropy,
    PlatformEntropyReadWord read_word,
    void *context
);
PlatformEntropyStatus platform_entropy_seed_primary_data(
    PlatformEntropy *entropy,
    const uint8_t seed[PLATFORM_ENTROPY_SEED_SIZE]
);
PlatformEntropyStatus platform_entropy_seed_from_source(
    PlatformEntropy *entropy
);
PlatformEntropyStatus platform_entropy_reseed_primary_data(
    PlatformEntropy *entropy,
    const uint8_t seed[PLATFORM_ENTROPY_SEED_SIZE]
);
PlatformEntropyStatus platform_entropy_reseed_from_source(
    PlatformEntropy *entropy
);
PlatformEntropyStatus platform_entropy_enable_primary_request(
    PlatformEntropy *entropy
);
PlatformEntropyStatus platform_entropy_set_control_flag(
    PlatformEntropy *entropy,
    uint8_t bit,
    bool enabled
);
PlatformEntropyStatus platform_entropy_request(
    PlatformEntropy *entropy,
    uint8_t *output,
    size_t size
);
