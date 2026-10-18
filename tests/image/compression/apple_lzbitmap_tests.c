#include "avp/image/compression/apple_lzbitmap.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_public_zbm_decoder_adapter(void) {
    static const uint8_t frame[] = {
        'Z', 'B', 'M', 0x09,
        0x0b, 0x00, 0x00, 0x05, 0x00, 0x00,
        'h', 'e', 'l', 'l', 'o',
        0x06, 0x00, 0x00, 0x00, 0x00, 0x00,
    };
    uint8_t output[8] = {0};
    uint8_t scratch[APPLE_LZBITMAP_SCRATCH_SIZE] = {0};

    assert(lzbitmap_decode_buffer(
        output,
        sizeof(output),
        frame,
        sizeof(frame),
        scratch
    ) == 5);
    assert(memcmp(output, "hello", 5) == 0);
    assert(lzbitmap_decode(
        output,
        sizeof(output),
        frame,
        3,
        scratch
    ) == 0);

    uint8_t invalid[sizeof(frame)];
    memcpy(invalid, frame, sizeof(frame));
    invalid[0] = 'X';
    assert(lzbitmap_decode_body(
        output,
        sizeof(output),
        invalid,
        sizeof(invalid),
        scratch
    ) == 0);
    assert(lzbitmap_decode_scratch_size(0x702) == 0x2080);
}

static void test_recovered_rle_entry(void) {
    static const uint8_t encoded[] = {'A', 'B', 0x0f, 0x04};
    uint8_t output[9] = {0};
    assert(lzbitmap_decode_rle(output, sizeof(output), encoded) == 9);
    assert(output[0] == 'A');
    for (size_t index = 1; index < sizeof(output); ++index) {
        assert(output[index] == 'B');
    }
    assert(lzbitmap_decode_rle(output, 0, encoded) == 0);
    assert(lzbitmap_decode_rle(output, sizeof(output), (uint8_t[]){0x0f}) == 0);
}

int main(void) {
    test_public_zbm_decoder_adapter();
    test_recovered_rle_entry();
    puts("apple_lzbitmap_tests: ok");
}
