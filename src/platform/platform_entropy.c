#include "avp/platform/platform_entropy.h"


static void copy_bytes(uint8_t *destination, const uint8_t *source, size_t size) {
    for (size_t index = 0; index < size; ++index) {
        destination[index] = source[index];
    }
}

static void install_seed(
    PlatformEntropy *entropy,
    const uint8_t seed[PLATFORM_ENTROPY_SEED_SIZE]
) {
    copy_bytes(entropy->seed, seed, sizeof(entropy->seed));
    entropy->seed_offset = 0;
    ++entropy->generation;
}

static void read_seed_from_source(
    PlatformEntropy *entropy,
    uint8_t seed[PLATFORM_ENTROPY_SEED_SIZE]
) {
    for (size_t offset = 0; offset < PLATFORM_ENTROPY_SEED_SIZE; offset += sizeof(uint32_t)) {
        const uint32_t word = entropy->read_word(entropy->context);
        for (size_t byte = 0; byte < sizeof(word); ++byte) {
            seed[offset + byte] = (uint8_t)(word >> (byte * 8));
        }
    }
}

PlatformEntropyStatus platform_entropy_initialize(
    PlatformEntropy *entropy,
    PlatformEntropyReadWord read_word,
    void *context
) {
    if (entropy == 0 || read_word == 0) {
        return PLATFORM_ENTROPY_STATUS_INVALID_ARGUMENT;
    }
    *entropy = (PlatformEntropy){
        .read_word = read_word,
        .context = context,
        .state = PLATFORM_ENTROPY_STATE_RESET,
    };
    return PLATFORM_ENTROPY_STATUS_OK;
}

PlatformEntropyStatus platform_entropy_seed_primary_data(
    PlatformEntropy *entropy,
    const uint8_t seed[PLATFORM_ENTROPY_SEED_SIZE]
) {
    if (entropy == 0 || seed == 0) {
        return PLATFORM_ENTROPY_STATUS_INVALID_ARGUMENT;
    }
    if (entropy->state == PLATFORM_ENTROPY_STATE_ENABLED) {
        return PLATFORM_ENTROPY_STATUS_INVALID_STATE;
    }
    install_seed(entropy, seed);
    entropy->state = PLATFORM_ENTROPY_STATE_SEEDED;
    return PLATFORM_ENTROPY_STATUS_OK;
}

PlatformEntropyStatus platform_entropy_seed_from_source(
    PlatformEntropy *entropy
) {
    if (entropy == 0 || entropy->read_word == 0) {
        return PLATFORM_ENTROPY_STATUS_INVALID_ARGUMENT;
    }
    if (entropy->state == PLATFORM_ENTROPY_STATE_ENABLED) {
        return PLATFORM_ENTROPY_STATUS_INVALID_STATE;
    }
    uint8_t seed[PLATFORM_ENTROPY_SEED_SIZE];
    read_seed_from_source(entropy, seed);
    return platform_entropy_seed_primary_data(entropy, seed);
}

PlatformEntropyStatus platform_entropy_reseed_primary_data(
    PlatformEntropy *entropy,
    const uint8_t seed[PLATFORM_ENTROPY_SEED_SIZE]
) {
    if (entropy == 0 || seed == 0) {
        return PLATFORM_ENTROPY_STATUS_INVALID_ARGUMENT;
    }
    if (entropy->state != PLATFORM_ENTROPY_STATE_ENABLED) {
        return PLATFORM_ENTROPY_STATUS_INVALID_STATE;
    }
    install_seed(entropy, seed);
    return PLATFORM_ENTROPY_STATUS_OK;
}

PlatformEntropyStatus platform_entropy_reseed_from_source(
    PlatformEntropy *entropy
) {
    if (entropy == 0 || entropy->read_word == 0) {
        return PLATFORM_ENTROPY_STATUS_INVALID_ARGUMENT;
    }
    if (entropy->state != PLATFORM_ENTROPY_STATE_ENABLED) {
        return PLATFORM_ENTROPY_STATUS_INVALID_STATE;
    }
    uint8_t seed[PLATFORM_ENTROPY_SEED_SIZE];
    read_seed_from_source(entropy, seed);
    return platform_entropy_reseed_primary_data(entropy, seed);
}

PlatformEntropyStatus platform_entropy_enable_primary_request(
    PlatformEntropy *entropy
) {
    if (entropy == 0) {
        return PLATFORM_ENTROPY_STATUS_INVALID_ARGUMENT;
    }
    if (entropy->state != PLATFORM_ENTROPY_STATE_SEEDED) {
        return PLATFORM_ENTROPY_STATUS_INVALID_STATE;
    }
    entropy->state = PLATFORM_ENTROPY_STATE_ENABLED;
    return PLATFORM_ENTROPY_STATUS_OK;
}

PlatformEntropyStatus platform_entropy_set_control_flag(
    PlatformEntropy *entropy,
    uint8_t bit,
    bool enabled
) {
    if (entropy == 0 || bit > 1) {
        return PLATFORM_ENTROPY_STATUS_INVALID_ARGUMENT;
    }
    if (entropy->state == PLATFORM_ENTROPY_STATE_RESET) {
        return PLATFORM_ENTROPY_STATUS_INVALID_STATE;
    }
    const uint8_t mask = (uint8_t)(1u << bit);
    entropy->control_flags = enabled
        ? (uint8_t)(entropy->control_flags | mask)
        : (uint8_t)(entropy->control_flags & (uint8_t)~mask);
    return PLATFORM_ENTROPY_STATUS_OK;
}

PlatformEntropyStatus platform_entropy_request(
    PlatformEntropy *entropy,
    uint8_t *output,
    size_t size
) {
    if (entropy == 0 || (output == 0 && size != 0)) {
        return PLATFORM_ENTROPY_STATUS_INVALID_ARGUMENT;
    }
    if (entropy->state != PLATFORM_ENTROPY_STATE_ENABLED) {
        return PLATFORM_ENTROPY_STATUS_INVALID_STATE;
    }
    if (size > sizeof(entropy->seed) - entropy->seed_offset) {
        return PLATFORM_ENTROPY_STATUS_SOURCE_FAILED;
    }
    copy_bytes(output, entropy->seed + entropy->seed_offset, size);
    entropy->seed_offset += size;
    return PLATFORM_ENTROPY_STATUS_OK;
}
