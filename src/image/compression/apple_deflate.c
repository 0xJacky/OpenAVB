#include "avp/image/compression/apple_deflate_lz4.h"

/*
 * Mark Adler's puff is a stack-only RFC 1951 reference decoder. Including the
 * pinned source here keeps its internal Huffman constructor available to the
 * readHuffmanTable compatibility entry.
 */
#include "zlib-puff/puff.c"

enum {
    ADLER_BASE = 65521,
    ADLER_BLOCK = 5552,
};

uint32_t adler32(uint32_t checksum, const uint8_t *bytes, size_t count) {
    uint32_t low = checksum & 0xffff;
    uint32_t high = checksum >> 16;

    while (count != 0) {
        size_t block = count < ADLER_BLOCK ? count : ADLER_BLOCK;
        count -= block;
        while (block-- != 0) {
            low += *bytes++;
            high += low;
        }
        low %= ADLER_BASE;
        high %= ADLER_BASE;
    }
    return low | (high << 16);
}

size_t zlib_decode_scratch_size(void) {
    return AVP_ZLIB_SCRATCH_SIZE;
}

static size_t decode_raw_deflate(
    uint8_t *destination,
    size_t destination_size,
    const uint8_t *source,
    size_t source_size
) {
    unsigned long output_count = destination_size;
    unsigned long input_count = source_size;
    int status = puff(
        destination,
        &output_count,
        source,
        &input_count
    );
    if (status != 0 || input_count != source_size) {
        return 0;
    }
    return output_count;
}

size_t zlibDecodeBuffer(
    uint8_t *destination,
    size_t destination_size,
    const uint8_t *source,
    size_t source_size,
    void *scratch
) {
    (void)scratch;
    return decode_raw_deflate(
        destination,
        destination_size,
        source,
        source_size
    );
}

size_t zlibDecodeBufferSafe(
    uint8_t *destination,
    size_t destination_size,
    const uint8_t *source,
    size_t source_size,
    void *scratch
) {
    return zlibDecodeBuffer(
        destination,
        destination_size,
        source,
        source_size,
        scratch
    );
}

int readHuffmanTable(
    AVPZlibHuffmanTable *table,
    const int16_t *code_lengths,
    size_t symbol_count
) {
    struct huffman huffman = {
        .count = table->count,
        .symbol = table->symbol,
    };
    return construct(&huffman, code_lengths, (int)symbol_count);
}

static uint32_t load_big_endian32(const uint8_t *bytes) {
    return ((uint32_t)bytes[0] << 24) |
        ((uint32_t)bytes[1] << 16) |
        ((uint32_t)bytes[2] << 8) |
        bytes[3];
}

static size_t decode_zlib_frame(
    uint8_t *destination,
    size_t destination_size,
    const uint8_t *source,
    size_t source_size
) {
    if (source_size < 6) {
        return 0;
    }

    uint8_t compression = source[0];
    uint8_t flags = source[1];
    uint16_t header = ((uint16_t)compression << 8) | flags;
    if ((compression & 0x0f) != 8 || (compression >> 4) > 7 ||
        header % 31 != 0 || (flags & 0x20) != 0) {
        return 0;
    }

    size_t decoded = decode_raw_deflate(
        destination,
        destination_size,
        source + 2,
        source_size - 6
    );
    if (decoded == 0) {
        return 0;
    }

    uint32_t expected = load_big_endian32(source + source_size - 4);
    if (adler32(1, destination, decoded) != expected) {
        return 0;
    }
    return decoded;
}

size_t zlib_decode_buffer(
    uint8_t *destination,
    size_t destination_size,
    const uint8_t *source,
    size_t source_size,
    void *scratch,
    uint32_t algorithm
) {
    switch (algorithm) {
        case AVP_COMPRESSION_DEFLATE:
            return zlibDecodeBuffer(
                destination,
                destination_size,
                source,
                source_size,
                scratch
            );
        case AVP_COMPRESSION_ZLIB:
            return decode_zlib_frame(
                destination,
                destination_size,
                source,
                source_size
            );
        default:
            return 0;
    }
}
