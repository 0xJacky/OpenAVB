#include "avp/runtime/prng/runtime_prng.h"

#include "mbedtls/memory_buffer_alloc.h"

#include <assert.h>
#include <string.h>


typedef struct {
    uint8_t bytes[PLATFORM_ENTROPY_SEED_SIZE];
    size_t offset;
} FixedEntropy;

static uint8_t hex_digit(char character) {
    if (character >= '0' && character <= '9') {
        return (uint8_t)(character - '0');
    }
    return (uint8_t)(character - 'a' + 10);
}

static size_t decode_hex(const char *hex, uint8_t *output, size_t capacity) {
    const size_t length = strlen(hex);
    assert((length & 1) == 0 && length / 2 <= capacity);
    for (size_t index = 0; index < length / 2; ++index) {
        output[index] = (uint8_t)((hex_digit(hex[index * 2]) << 4) |
            hex_digit(hex[index * 2 + 1]));
    }
    return length / 2;
}

static uint32_t read_fixed_word(void *context) {
    FixedEntropy *source = context;
    assert(source->offset + 4 <= sizeof(source->bytes));
    const uint32_t word = (uint32_t)source->bytes[source->offset] |
        ((uint32_t)source->bytes[source->offset + 1] << 8) |
        ((uint32_t)source->bytes[source->offset + 2] << 16) |
        ((uint32_t)source->bytes[source->offset + 3] << 24);
    source->offset += 4;
    return word;
}

int main(void) {
    static uint8_t heap[4096];
    static const char entropy_hex[] =
        "a1dc2dfeda4f3a1124e0e75ebfbe5f98cac11018221dda3f"
        "dcf8f9125d68447abae5ea27166540515268a493a96b5187";
    static const char expected_hex[] =
        "228293e59b1e4545a4ff9f232616fc5108a1128debd0f7c2"
        "0ace837ca105cbf24c0dac1f9847dafd0d0500721ffad3c6"
        "84a992d110a549a264d14a8911c50be8cd6a7e8fac783ad9"
        "5b24f64fd8cc4c8b649eac2b15b363e30df79541a6b8a1c"
        "aac238949b46643694c85e1d5fcbcd9aaae6260acee660b8"
        "a79bea48e079ceb6a5eaf4993a82c3f1b758d7c53e3094e"
        "eac63dc255be6dcdcc2b51e5ca45d2b20684a5a8fa5806b"
        "96f8461ebf51bc515a7dd8c5475c0e70f2fd0faf7869a99"
        "ab6c";
    FixedEntropy source = {0};
    uint8_t expected[192];
    uint8_t output[192];
    PlatformEntropy entropy;
    RuntimePRNG prng = {0};

    assert(decode_hex(entropy_hex, source.bytes, sizeof(source.bytes)) ==
        sizeof(source.bytes));
    assert(decode_hex(expected_hex, expected, sizeof(expected)) ==
        sizeof(expected));
    mbedtls_memory_buffer_alloc_init(heap, sizeof(heap));
    assert(platform_entropy_initialize(&entropy, read_fixed_word, &source) ==
        PLATFORM_ENTROPY_STATUS_OK);
    assert(platform_entropy_seed_from_source(&entropy) ==
        PLATFORM_ENTROPY_STATUS_OK);
    assert(platform_entropy_enable_primary_request(&entropy) ==
        PLATFORM_ENTROPY_STATUS_OK);
    assert(runtime_prng_initialize(&prng, &entropy, 0, 0) ==
        RUNTIME_PRNG_STATUS_OK);
    assert(runtime_prng_generate(&prng, output, sizeof(output)) ==
        RUNTIME_PRNG_STATUS_OK);
    assert(runtime_prng_generate(&prng, output, sizeof(output)) ==
        RUNTIME_PRNG_STATUS_OK);
    assert(memcmp(output, expected, sizeof(output)) == 0);
    runtime_prng_deinitialize(&prng);
    assert(runtime_prng_generate(&prng, output, 1) ==
        RUNTIME_PRNG_STATUS_INVALID_STATE);
    return 0;
}
