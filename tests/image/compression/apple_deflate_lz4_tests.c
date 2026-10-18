#include "avp/image/compression/apple_deflate_lz4.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void store32le(uint8_t *destination, uint32_t value) {
    destination[0] = (uint8_t)value;
    destination[1] = (uint8_t)(value >> 8);
    destination[2] = (uint8_t)(value >> 16);
    destination[3] = (uint8_t)(value >> 24);
}

static void test_adler_and_huffman(void) {
    static const uint8_t hello[] = "hello";
    assert(adler32(1, hello, 5) == UINT32_C(0x062c0215));
    assert(adler32(1, hello, 0) == 1);
    assert(zlib_decode_scratch_size() == AVP_ZLIB_SCRATCH_SIZE);

    AVPZlibHuffmanTable table = {0};
    int16_t complete[] = {1, 1};
    assert(readHuffmanTable(&table, complete, 2) == 0);
    assert(table.count[1] == 2);
    assert(table.symbol[0] == 0 && table.symbol[1] == 1);

    int16_t oversubscribed[] = {1, 1, 1};
    assert(readHuffmanTable(&table, oversubscribed, 3) < 0);
}

static void test_raw_and_framed_deflate(void) {
    static const uint8_t raw[] = {
        0x01, 0x05, 0x00, 0xfa, 0xff, 'h', 'e', 'l', 'l', 'o',
    };
    static const uint8_t framed[] = {
        0x78, 0x01,
        0x01, 0x05, 0x00, 0xfa, 0xff, 'h', 'e', 'l', 'l', 'o',
        0x06, 0x2c, 0x02, 0x15,
    };
    uint8_t output[32] = {0};
    uint8_t scratch[AVP_ZLIB_SCRATCH_SIZE] = {0};

    assert(
        zlibDecodeBuffer(
            output,
            sizeof(output),
            raw,
            sizeof(raw),
            scratch
        ) == 5
    );
    assert(memcmp(output, "hello", 5) == 0);

    memset(output, 0, sizeof(output));
    assert(
        zlibDecodeBufferSafe(
            output,
            sizeof(output),
            raw,
            sizeof(raw),
            scratch
        ) == 5
    );
    assert(
        zlib_decode_buffer(
            output,
            sizeof(output),
            framed,
            sizeof(framed),
            scratch,
            AVP_COMPRESSION_ZLIB
        ) == 5
    );
    assert(memcmp(output, "hello", 5) == 0);

    uint8_t corrupt[sizeof(framed)];
    memcpy(corrupt, framed, sizeof(framed));
    corrupt[sizeof(corrupt) - 1] ^= 1;
    assert(
        zlib_decode_buffer(
            output,
            sizeof(output),
            corrupt,
            sizeof(corrupt),
            scratch,
            AVP_COMPRESSION_ZLIB
        ) == 0
    );
    assert(zlibDecodeBuffer(output, sizeof(output), raw, 1, scratch) == 0);
}

static void test_dynamic_huffman_deflate(void) {
    static const uint8_t framed[] = {
        0x78, 0xda, 0xed, 0xca, 0x37, 0x11, 0x00, 0x20, 0x10, 0x00,
        0x30, 0xad, 0x4f, 0x79, 0xfc, 0x3b, 0xa0, 0x1c, 0x78, 0x60,
        0x48, 0xe6, 0x44, 0x3c, 0xe5, 0xaa, 0x47, 0xdb, 0xfa, 0x92,
        0x99, 0x63, 0x84, 0x65, 0x59, 0x96, 0x65, 0x59, 0x96, 0x65,
        0x59, 0x96, 0x65, 0x7d, 0xb9, 0x26, 0xc7, 0x78, 0xbe, 0x9c,
    };
    static const uint8_t pattern[] = "aaaaaaaaaabbbbbbbbccccccdddddeeeefffgg";
    uint8_t output[3800] = {0};
    uint8_t scratch[AVP_ZLIB_SCRATCH_SIZE] = {0};

    assert(
        zlib_decode_buffer(
            output,
            sizeof(output),
            framed,
            sizeof(framed),
            scratch,
            AVP_COMPRESSION_ZLIB
        ) == sizeof(output)
    );
    for (size_t offset = 0; offset < sizeof(output); offset += 38) {
        assert(memcmp(output + offset, pattern, 38) == 0);
    }
}

static void test_raw_lz4(void) {
    static const uint8_t literals[] = {0x50, 'h', 'e', 'l', 'l', 'o'};
    static const uint8_t match[] = {
        0x40, 'a', 'b', 'c', 'd', 0x04, 0x00,
    };
    uint8_t output[32] = {0};

    assert(
        lz4raw_decode_buffer(
            output,
            sizeof(output),
            literals,
            sizeof(literals),
            NULL
        ) == 5
    );
    assert(memcmp(output, "hello", 5) == 0);

    memset(output, 0, sizeof(output));
    assert(
        lz4raw_decode_buffer(
            output,
            sizeof(output),
            match,
            sizeof(match),
            NULL
        ) == 8
    );
    assert(memcmp(output, "abcdabcd", 8) == 0);

    const uint8_t invalid[] = {0x00, 0x00, 0x00};
    assert(
        lz4raw_decode_buffer(
            output,
            sizeof(output),
            invalid,
            sizeof(invalid),
            NULL
        ) == 0
    );
}

static void test_apple_lz4_frames(void) {
    uint8_t compressed[32] = {0};
    store32le(compressed, AVP_LZ4_COMPRESSED_MAGIC);
    store32le(compressed + 4, 5);
    store32le(compressed + 8, 6);
    memcpy(compressed + 12, "\x50hello", 6);
    store32le(compressed + 18, AVP_LZ4_END_MAGIC);

    uint8_t raw[32] = {0};
    store32le(raw, AVP_LZ4_RAW_MAGIC);
    store32le(raw + 4, 5);
    memcpy(raw + 8, "hello", 5);
    store32le(raw + 13, AVP_LZ4_END_MAGIC);

    uint8_t output[32] = {0};
    assert(
        lz4_decode_buffer(
            output,
            sizeof(output),
            compressed,
            22,
            NULL
        ) == 5
    );
    assert(memcmp(output, "hello", 5) == 0);

    memset(output, 0, sizeof(output));
    assert(
        lz4_decode_buffer(output, sizeof(output), raw, 17, NULL) == 5
    );
    assert(memcmp(output, "hello", 5) == 0);
    raw[0] = 0;
    assert(lz4_decode_buffer(output, sizeof(output), raw, 17, NULL) == 0);
}

int main(void) {
    test_adler_and_huffman();
    test_raw_and_framed_deflate();
    test_dynamic_huffman_deflate();
    test_raw_lz4();
    test_apple_lz4_frames();
    puts("apple_deflate_lz4_tests: ok");
}
