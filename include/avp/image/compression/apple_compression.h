/*
 * Copyright (c) 2026 owngoal-dev
 *
 * Apple LZFSE-compatible decode adapters built on the upstream lzfse project.
 */

#ifndef AVP_APPLE_COMPRESSION_H
#define AVP_APPLE_COMPRESSION_H

#include <stddef.h>
#include <stdint.h>

#include "lzfse.h"
#include "lzfse_internal.h"
#include "lzvn_decode_base.h"

#define AVP_COMPRESSION_PUBLIC __attribute__((used, retain))

enum {
    AVP_COMPRESSION_LZFSE = 0x891,
    AVP_COMPRESSION_LZFSE_IBOOT = 0x8a1,
};

typedef struct {
    uint8_t *begin;
    uint8_t *cursor;
    uint8_t *end;
} LZFSEIBootBuffer;

size_t lzfse_decode_buffer_with_scratch(
    uint8_t *destination,
    size_t destination_size,
    const uint8_t *source,
    size_t source_size,
    void *scratch
);
AVP_COMPRESSION_PUBLIC size_t lzvn_decode_buffer(
    void *destination,
    size_t destination_size,
    const void *source,
    size_t source_size
);

AVP_COMPRESSION_PUBLIC int lzvnDecode(lzvn_decoder_state *state);
AVP_COMPRESSION_PUBLIC int lzfseDecode(lzfse_decoder_state *state);
AVP_COMPRESSION_PUBLIC int lzfseDecodeV1(
    lzfse_compressed_block_header_v1 *output,
    const lzfse_compressed_block_header_v2 *input
);

AVP_COMPRESSION_PUBLIC size_t lzfse_decode_buffer_iboot(
    uint8_t *destination,
    size_t destination_size,
    const uint8_t *source,
    size_t source_size,
    void *scratch
);
AVP_COMPRESSION_PUBLIC int lzfse_decode_lzvn_block_iboot(
    lzvn_decoder_state *state
);

AVP_COMPRESSION_PUBLIC int LZFSEIBootBufferPushN(
    LZFSEIBootBuffer *buffer,
    const uint8_t *bytes,
    size_t count
);
AVP_COMPRESSION_PUBLIC int LZFSEIBootBufferPush(
    LZFSEIBootBuffer *buffer,
    uint8_t byte
);
AVP_COMPRESSION_PUBLIC int LZFSEIBootBufferCopy(
    LZFSEIBootBuffer *buffer,
    size_t distance,
    size_t count
);

AVP_COMPRESSION_PUBLIC size_t compression_decode_scratch_buffer_size(
    uint32_t algorithm
);
AVP_COMPRESSION_PUBLIC size_t compression_decode_buffer(
    uint8_t *destination,
    size_t destination_size,
    const uint8_t *source,
    size_t source_size,
    void *scratch,
    uint32_t algorithm
);

#endif
