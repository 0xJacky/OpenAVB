#include "avp/image/compression/apple_compression.h"

#include <string.h>

size_t lzvn_decode_buffer(
    void *destination,
    size_t destination_size,
    const void *source,
    size_t source_size
) {
    lzvn_decoder_state state = {
        .src = source,
        .src_end = (const uint8_t *)source + source_size,
        .dst = destination,
        .dst_begin = destination,
        .dst_end = (uint8_t *)destination + destination_size,
        .dst_current = destination,
    };
    lzvn_decode(&state);
    return (size_t)(state.dst - (uint8_t *)destination);
}

int lzvnDecode(lzvn_decoder_state *state) {
    lzvn_decode(state);
    if (state->end_of_stream) {
        return LZFSE_STATUS_OK;
    }
    if (state->dst == state->dst_end) {
        return LZFSE_STATUS_DST_FULL;
    }
    if (state->src == state->src_end) {
        return LZFSE_STATUS_SRC_EMPTY;
    }
    return LZFSE_STATUS_ERROR;
}

int lzfseDecode(lzfse_decoder_state *state) {
    return lzfse_decode(state);
}

size_t lzfse_decode_buffer_iboot(
    uint8_t *destination,
    size_t destination_size,
    const uint8_t *source,
    size_t source_size,
    void *scratch
) {
    if (scratch == NULL) {
        return 0;
    }
    return lzfse_decode_buffer_with_scratch(
        destination,
        destination_size,
        source,
        source_size,
        scratch
    );
}

int lzfse_decode_lzvn_block_iboot(lzvn_decoder_state *state) {
    return lzvnDecode(state);
}

int LZFSEIBootBufferPushN(
    LZFSEIBootBuffer *buffer,
    const uint8_t *bytes,
    size_t count
) {
    if ((size_t)(buffer->end - buffer->cursor) < count) {
        return LZFSE_STATUS_DST_FULL;
    }
    memcpy(buffer->cursor, bytes, count);
    buffer->cursor += count;
    return LZFSE_STATUS_OK;
}

int LZFSEIBootBufferPush(LZFSEIBootBuffer *buffer, uint8_t byte) {
    return LZFSEIBootBufferPushN(buffer, &byte, 1);
}

int LZFSEIBootBufferCopy(
    LZFSEIBootBuffer *buffer,
    size_t distance,
    size_t count
) {
    size_t produced = (size_t)(buffer->cursor - buffer->begin);
    if (distance == 0 || distance > produced) {
        return LZFSE_STATUS_ERROR;
    }
    if ((size_t)(buffer->end - buffer->cursor) < count) {
        return LZFSE_STATUS_DST_FULL;
    }

    uint8_t *source = buffer->cursor - distance;
    for (size_t index = 0; index < count; ++index) {
        buffer->cursor[index] = source[index];
    }
    buffer->cursor += count;
    return LZFSE_STATUS_OK;
}

size_t compression_decode_scratch_buffer_size(uint32_t algorithm) {
    switch (algorithm) {
        case AVP_COMPRESSION_LZFSE:
        case AVP_COMPRESSION_LZFSE_IBOOT:
            return lzfse_decode_scratch_size();
        default:
            return 0;
    }
}

size_t compression_decode_buffer(
    uint8_t *destination,
    size_t destination_size,
    const uint8_t *source,
    size_t source_size,
    void *scratch,
    uint32_t algorithm
) {
    switch (algorithm) {
        case AVP_COMPRESSION_LZFSE:
        case AVP_COMPRESSION_LZFSE_IBOOT:
            return lzfse_decode_buffer_iboot(
                destination,
                destination_size,
                source,
                source_size,
                scratch
            );
        default:
            return 0;
    }
}
