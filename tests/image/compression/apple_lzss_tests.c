#include "avp/image/compression/apple_lzss.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    static const uint8_t literals[] = {
        0xff, 'h', 'e', 'l', 'l', 'o',
    };
    static const uint8_t match[] = {
        0x07, 'a', 'b', 'c', 0xee, 0xf3,
    };
    uint8_t output[16] = {0};

    assert(runtime_lzss_decompress(
        output,
        5,
        literals,
        sizeof(literals)
    ) == 5);
    assert(memcmp(output, "hello", 5) == 0);

    memset(output, 0, sizeof(output));
    assert(runtime_lzss_decompress(
        output,
        9,
        match,
        sizeof(match)
    ) == 9);
    assert(memcmp(output, "abcabcabc", 9) == 0);
    assert(runtime_lzss_decompress(output, 0, match, sizeof(match)) == 0);
    assert(runtime_lzss_decompress(output, sizeof(output), match, 1) == 0);
    puts("apple_lzss_tests: ok");
}
