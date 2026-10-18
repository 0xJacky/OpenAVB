#ifndef AVP_APPLE_DEFLATE_LZ4_H
#define AVP_APPLE_DEFLATE_LZ4_H

#include <stddef.h>
#include <stdint.h>

#define AVP_CODEC_PUBLIC __attribute__((used, retain))

enum {
    AVP_COMPRESSION_LZ4 = 0x100,
    AVP_COMPRESSION_LZ4_RAW = 0x101,
    AVP_COMPRESSION_DEFLATE = 0x205,
    AVP_COMPRESSION_ZLIB = 0x505,
    AVP_ZLIB_SCRATCH_SIZE = 0x20000,
};

enum {
    AVP_LZ4_COMPRESSED_MAGIC = 0x31347662,
    AVP_LZ4_RAW_MAGIC = 0x2d347662,
    AVP_LZ4_END_MAGIC = 0x24347662,
};

typedef struct {
    int16_t count[16];
    int16_t symbol[288];
} AVPZlibHuffmanTable;

AVP_CODEC_PUBLIC uint32_t adler32(
    uint32_t checksum,
    const uint8_t *bytes,
    size_t count
);
AVP_CODEC_PUBLIC size_t zlib_decode_scratch_size(void);
AVP_CODEC_PUBLIC size_t zlibDecodeBuffer(
    uint8_t *destination,
    size_t destination_size,
    const uint8_t *source,
    size_t source_size,
    void *scratch
);
AVP_CODEC_PUBLIC size_t zlibDecodeBufferSafe(
    uint8_t *destination,
    size_t destination_size,
    const uint8_t *source,
    size_t source_size,
    void *scratch
);
AVP_CODEC_PUBLIC int readHuffmanTable(
    AVPZlibHuffmanTable *table,
    const int16_t *code_lengths,
    size_t symbol_count
);
AVP_CODEC_PUBLIC size_t zlib_decode_buffer(
    uint8_t *destination,
    size_t destination_size,
    const uint8_t *source,
    size_t source_size,
    void *scratch,
    uint32_t algorithm
);

AVP_CODEC_PUBLIC int lz4_decode(
    uint8_t **destination,
    uint8_t *destination_begin,
    uint8_t *destination_end,
    const uint8_t **source,
    const uint8_t *source_end
);
AVP_CODEC_PUBLIC int lz4_decode_asm(
    uint8_t **destination,
    uint8_t *destination_begin,
    uint8_t *destination_end,
    const uint8_t **source,
    const uint8_t *source_end
);
AVP_CODEC_PUBLIC size_t lz4raw_decode_buffer(
    uint8_t *destination,
    size_t destination_size,
    const uint8_t *source,
    size_t source_size,
    void *scratch
);
AVP_CODEC_PUBLIC size_t lz4_decode_buffer(
    uint8_t *destination,
    size_t destination_size,
    const uint8_t *source,
    size_t source_size,
    void *scratch
);

#endif
