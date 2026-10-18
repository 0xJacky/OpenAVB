#include "avp/platform/platform_entropy.h"

#include <assert.h>
#include <string.h>


typedef struct {
    uint32_t next;
    size_t reads;
} WordSource;

static uint32_t read_incrementing_word(void *context) {
    WordSource *source = context;
    ++source->reads;
    return source->next++;
}

int main(void) {
    WordSource source = {.next = UINT32_C(0x03020100)};
    PlatformEntropy entropy;
    uint8_t output[PLATFORM_ENTROPY_SEED_SIZE];

    assert(platform_entropy_initialize(&entropy, read_incrementing_word,
        &source) == PLATFORM_ENTROPY_STATUS_OK);
    assert(platform_entropy_request(&entropy, output, 1) ==
        PLATFORM_ENTROPY_STATUS_INVALID_STATE);
    assert(platform_entropy_enable_primary_request(&entropy) ==
        PLATFORM_ENTROPY_STATUS_INVALID_STATE);
    assert(platform_entropy_seed_from_source(&entropy) ==
        PLATFORM_ENTROPY_STATUS_OK);
    assert(source.reads == 12);
    assert(entropy.state == PLATFORM_ENTROPY_STATE_SEEDED);
    assert(entropy.generation == 1);
    assert(platform_entropy_set_control_flag(&entropy, 0, true) ==
        PLATFORM_ENTROPY_STATUS_OK);
    assert(platform_entropy_set_control_flag(&entropy, 1, true) ==
        PLATFORM_ENTROPY_STATUS_OK);
    assert(entropy.control_flags == 3);
    assert(platform_entropy_enable_primary_request(&entropy) ==
        PLATFORM_ENTROPY_STATUS_OK);
    assert(platform_entropy_request(&entropy, output, 32) ==
        PLATFORM_ENTROPY_STATUS_OK);
    assert(output[0] == 0x00 && output[1] == 0x01 &&
        output[2] == 0x02 && output[3] == 0x03);
    assert(platform_entropy_request(&entropy, output + 32, 16) ==
        PLATFORM_ENTROPY_STATUS_OK);
    assert(platform_entropy_request(&entropy, output, 1) ==
        PLATFORM_ENTROPY_STATUS_SOURCE_FAILED);

    const uint32_t previous_generation = entropy.generation;
    assert(platform_entropy_reseed_from_source(&entropy) ==
        PLATFORM_ENTROPY_STATUS_OK);
    assert(entropy.generation == previous_generation + 1);
    assert(source.reads == 24);
    assert(platform_entropy_request(&entropy, output, sizeof(output)) ==
        PLATFORM_ENTROPY_STATUS_OK);
    assert(output[0] == 0x0c && output[4] == 0x0d);

    uint8_t fixed_seed[PLATFORM_ENTROPY_SEED_SIZE];
    memset(fixed_seed, 0xa5, sizeof(fixed_seed));
    assert(platform_entropy_reseed_primary_data(&entropy, fixed_seed) ==
        PLATFORM_ENTROPY_STATUS_OK);
    memset(output, 0, sizeof(output));
    assert(platform_entropy_request(&entropy, output, sizeof(output)) ==
        PLATFORM_ENTROPY_STATUS_OK);
    assert(memcmp(output, fixed_seed, sizeof(output)) == 0);
    assert(platform_entropy_set_control_flag(&entropy, 0, false) ==
        PLATFORM_ENTROPY_STATUS_OK);
    assert(entropy.control_flags == 2);
    assert(platform_entropy_set_control_flag(&entropy, 2, true) ==
        PLATFORM_ENTROPY_STATUS_INVALID_ARGUMENT);
    return 0;
}
