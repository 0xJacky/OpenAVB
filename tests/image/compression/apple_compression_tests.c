#include "avp/image/compression/apple_compression.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void store32le(uint8_t *destination, uint32_t value) {
    destination[0] = (uint8_t)value;
    destination[1] = (uint8_t)(value >> 8);
    destination[2] = (uint8_t)(value >> 16);
    destination[3] = (uint8_t)(value >> 24);
}

static size_t make_raw_stream(
    uint8_t *stream,
    const uint8_t *payload,
    size_t payload_size
) {
    store32le(stream, LZFSE_UNCOMPRESSED_BLOCK_MAGIC);
    store32le(stream + 4, (uint32_t)payload_size);
    memcpy(stream + 8, payload, payload_size);
    store32le(stream + 8 + payload_size, LZFSE_ENDOFSTREAM_BLOCK_MAGIC);
    return 12 + payload_size;
}

static void test_public_dispatchers(void) {
    static const uint8_t payload[] = "OpenBootROM LZFSE public binding";
    uint8_t stream[128] = {0};
    uint8_t output[128] = {0};
    uint8_t scratch[sizeof(lzfse_decoder_state)] = {0};
    size_t stream_size = make_raw_stream(stream, payload, sizeof(payload));

    assert(lzfse_decode_scratch_size() == sizeof(lzfse_decoder_state));
    assert(
        compression_decode_scratch_buffer_size(AVP_COMPRESSION_LZFSE) ==
        sizeof(lzfse_decoder_state)
    );
    assert(
        compression_decode_scratch_buffer_size(
            AVP_COMPRESSION_LZFSE_IBOOT
        ) == sizeof(lzfse_decoder_state)
    );
    assert(compression_decode_scratch_buffer_size(0) == 0);

    size_t count = compression_decode_buffer(
        output,
        sizeof(output),
        stream,
        stream_size,
        scratch,
        AVP_COMPRESSION_LZFSE
    );
    assert(count == sizeof(payload));
    assert(memcmp(output, payload, sizeof(payload)) == 0);

    memset(output, 0, sizeof(output));
    count = lzfse_decode_buffer(
        output,
        sizeof(output),
        stream,
        stream_size,
        scratch
    );
    assert(count == sizeof(payload));
    assert(memcmp(output, payload, sizeof(payload)) == 0);
    assert(lzfse_decode_buffer(
        output,
        sizeof(output),
        stream,
        stream_size,
        NULL
    ) == 0);

    memset(output, 0, sizeof(output));
    count = compression_decode_buffer(
        output,
        sizeof(output),
        stream,
        stream_size,
        scratch,
        AVP_COMPRESSION_LZFSE_IBOOT
    );
    assert(count == sizeof(payload));
    assert(memcmp(output, payload, sizeof(payload)) == 0);
    assert(
        compression_decode_buffer(
            output,
            sizeof(output),
            stream,
            stream_size,
            scratch,
            0
        ) == 0
    );
}

static void test_literal_decoder_rejects_partial_groups(void) {
    uint8_t output[4] = {0};
    int32_t decoder_table[1] = {0};
    fse_in_stream input = {0};
    const uint8_t encoded[1] = {0};
    const uint8_t *cursor = encoded;
    fse_state states[4] = {0};
    assert(lzfse_decode_literals(
        output,
        1,
        decoder_table,
        &input,
        &cursor,
        encoded,
        states
    ) == LZFSE_STATUS_ERROR);
}

static void test_resumable_decoder_contract(void) {
    static const uint8_t payload[] = "resumable";
    uint8_t stream[64] = {0};
    uint8_t output[64] = {0};
    size_t stream_size = make_raw_stream(stream, payload, sizeof(payload));
    lzfse_decoder_state state = {
        .src = stream,
        .src_begin = stream,
        .src_end = stream + stream_size,
        .dst = output,
        .dst_begin = output,
        .dst_end = output + sizeof(output),
    };

    assert(lzfseDecode(&state) == LZFSE_STATUS_OK);
    assert(state.end_of_stream == 1);
    assert((size_t)(state.dst - output) == sizeof(payload));
    assert(memcmp(output, payload, sizeof(payload)) == 0);

    uint8_t lzvn_end[] = {0x06, 0, 0, 0, 0, 0, 0, 0};
    lzvn_decoder_state lzvn = {
        .src = lzvn_end,
        .src_end = lzvn_end + sizeof(lzvn_end),
        .dst = output,
        .dst_begin = output,
        .dst_end = output + sizeof(output),
        .dst_current = output,
    };
    assert(lzvnDecode(&lzvn) == LZFSE_STATUS_OK);
    assert(lzvn.end_of_stream == 1);

    uint8_t lzvn_literal[] = {
        0xe3, 'a', 'b', 'c', 0x06, 0, 0, 0, 0, 0, 0, 0,
    };
    memset(output, 0, sizeof(output));
    assert(
        lzvn_decode_buffer(
            output,
            sizeof(output),
            lzvn_literal,
            sizeof(lzvn_literal)
        ) == 3
    );
    assert(memcmp(output, "abc", 3) == 0);
}

static void test_iboot_output_buffer(void) {
    uint8_t output[16] = {0};
    LZFSEIBootBuffer buffer = {
        .begin = output,
        .cursor = output,
        .end = output + sizeof(output),
    };
    const uint8_t seed[] = {'a', 'b', 'c'};

    assert(
        LZFSEIBootBufferPushN(&buffer, seed, sizeof(seed)) ==
        LZFSE_STATUS_OK
    );
    assert(LZFSEIBootBufferPush(&buffer, 'd') == LZFSE_STATUS_OK);
    assert(LZFSEIBootBufferCopy(&buffer, 4, 8) == LZFSE_STATUS_OK);
    assert(memcmp(output, "abcdabcdabcd", 12) == 0);
    assert(LZFSEIBootBufferCopy(&buffer, 0, 1) == LZFSE_STATUS_ERROR);
    assert(LZFSEIBootBufferPushN(&buffer, seed, 5) == LZFSE_STATUS_DST_FULL);
}

static void test_v2_header_conversion(void) {
    lzfse_compressed_block_header_v2 input = {0};
    lzfse_compressed_block_header_v1 output = {0};
    input.magic = LZFSE_COMPRESSEDV2_BLOCK_MAGIC;
    input.n_raw_bytes = 42;
    input.packed_fields[0] =
        UINT64_C(3) | (UINT64_C(5) << 20) | (UINT64_C(7) << 40) |
        (UINT64_C(6) << 60);
    input.packed_fields[1] =
        UINT64_C(1) | (UINT64_C(2) << 10) | (UINT64_C(3) << 20) |
        (UINT64_C(4) << 30) | (UINT64_C(9) << 40) |
        (UINT64_C(5) << 60);
    input.packed_fields[2] =
        (uint64_t)offsetof(lzfse_compressed_block_header_v2, freq) |
        (UINT64_C(8) << 32) | (UINT64_C(9) << 42) |
        (UINT64_C(10) << 52);

    assert(lzfseDecodeV1(&output, &input) == LZFSE_STATUS_OK);
    assert(output.magic == LZFSE_COMPRESSEDV1_BLOCK_MAGIC);
    assert(output.n_raw_bytes == 42);
    assert(output.n_literals == 3);
    assert(output.n_literal_payload_bytes == 5);
    assert(output.n_matches == 7);
    assert(output.n_lmd_payload_bytes == 9);
    assert(output.n_payload_bytes == 14);
    assert(output.literal_bits == -1);
    assert(output.lmd_bits == -2);
    assert(output.literal_state[3] == 4);
    assert(output.l_state == 8 && output.m_state == 9 && output.d_state == 10);
}

int main(void) {
    test_public_dispatchers();
    test_resumable_decoder_contract();
    test_literal_decoder_rejects_partial_groups();
    test_iboot_output_buffer();
    test_v2_header_conversion();
    puts("apple_compression_tests: ok");
}
