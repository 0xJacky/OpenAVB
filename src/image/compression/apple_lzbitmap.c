#include "avp/image/compression/apple_lzbitmap.h"

#include "libzbitmap/libzbitmap.h"

size_t lzbitmap_decode_rle(
    uint8_t *destination,
    size_t destination_size,
    const uint8_t *source
) {
    if (destination_size == 0 || source[0] == 0x0f) {
        return 0;
    }

    size_t output_offset = 0;
    size_t input_offset = 0;
    while (output_offset < destination_size) {
        uint8_t value = source[input_offset++];
        if (value != 0x0f) {
            destination[output_offset++] = value;
            continue;
        }

        uint8_t repeated = destination[output_offset - 1];
        size_t run_length = 3;
        uint8_t extension;
        do {
            extension = source[input_offset++];
            run_length += extension;
        } while (extension == 0x0f && output_offset + run_length < destination_size);

        size_t available = destination_size - output_offset;
        size_t emitted = run_length < available ? run_length : available;
        for (size_t index = 0; index < emitted; ++index) {
            destination[output_offset + index] = repeated;
        }
        output_offset += emitted;
    }
    return destination_size;
}

size_t lzbitmap_decode_body(
    void *destination,
    uint32_t destination_size,
    const void *source,
    uint32_t source_size,
    void *scratch
) {
    (void)scratch;
    size_t output_size = 0;
    if (zbm_decompress(
            destination,
            destination_size,
            source,
            source_size,
            &output_size
        ) != 0) {
        return 0;
    }
    return output_size;
}

size_t lzbitmap_decode(
    void *destination,
    uint32_t destination_size,
    const void *source,
    uint32_t source_size,
    void *scratch
) {
    if (source_size < 4) {
        return 0;
    }
    return lzbitmap_decode_body(
        destination,
        destination_size,
        source,
        source_size,
        scratch
    );
}

size_t lzbitmap_decode_buffer(
    void *destination,
    size_t destination_size,
    const void *source,
    size_t source_size,
    void *scratch
) {
    if (destination_size > UINT32_MAX || source_size > UINT32_MAX) {
        return 0;
    }
    return lzbitmap_decode(
        destination,
        (uint32_t)destination_size,
        source,
        (uint32_t)source_size,
        scratch
    );
}

size_t lzbitmap_decode_scratch_size(uint32_t algorithm) {
    (void)algorithm;
    return APPLE_LZBITMAP_SCRATCH_SIZE;
}
