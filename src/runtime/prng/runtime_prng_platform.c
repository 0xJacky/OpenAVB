#include "avp/platform/mmio.h"
#include "avp/runtime/exception/pvpanic.h"
#include "avp/runtime/prng/runtime_prng.h"

#include "mbedtls/memory_buffer_alloc.h"
#include "mbedtls/platform.h"


enum {
    PLATFORM_ENTROPY_REGISTER = 0x00400038,
};

#define RUNTIME_PRNG_STATE \
    __attribute__((section(".runtime_prng.state"), used))

static PlatformEntropy platform_entropy RUNTIME_PRNG_STATE;
static RuntimePRNG global_prng RUNTIME_PRNG_STATE;
static uint8_t mbedtls_heap[RUNTIME_PRNG_MBEDTLS_HEAP_SIZE]
    RUNTIME_PRNG_STATE __attribute__((aligned(16)));

static uint32_t read_entropy_word(void *context) {
    (void)context;
    return mmio_read32(PLATFORM_ENTROPY_REGISTER);
}

static _Noreturn void crypto_fatal(int status) {
    (void)status;
    pvpanic_trigger();
}

void runtime_prng_platform_initialize(void) {
    if (global_prng.initialized) {
        return;
    }
    mbedtls_memory_buffer_alloc_init(mbedtls_heap, sizeof(mbedtls_heap));
    if (mbedtls_platform_set_exit(crypto_fatal) != 0 ||
        platform_entropy_initialize(
            &platform_entropy,
            read_entropy_word,
            0
        ) != PLATFORM_ENTROPY_STATUS_OK ||
        platform_entropy_seed_from_source(&platform_entropy) !=
            PLATFORM_ENTROPY_STATUS_OK ||
        platform_entropy_enable_primary_request(&platform_entropy) !=
            PLATFORM_ENTROPY_STATUS_OK ||
        runtime_prng_initialize(
            &global_prng,
            &platform_entropy,
            (const uint8_t *)"OpenBootROM runtime PRNG",
            sizeof("OpenBootROM runtime PRNG") - 1
        ) != RUNTIME_PRNG_STATUS_OK) {
        pvpanic_trigger();
    }
}

PlatformEntropy *runtime_prng_platform_entropy(void) {
    return &platform_entropy;
}

RuntimePRNG *runtime_prng_global(void) {
    return &global_prng;
}
