#pragma once

#include <stddef.h>
#include <stdint.h>

#define APPLE_LZBITMAP_PUBLIC __attribute__((used, retain))
#define APPLE_LZBITMAP_SCRATCH_SIZE 0x2080u

APPLE_LZBITMAP_PUBLIC size_t lzbitmap_decode_rle(
    uint8_t *destination,
    size_t destination_size,
    const uint8_t *source
);
APPLE_LZBITMAP_PUBLIC size_t lzbitmap_decode(
    void *destination,
    uint32_t destination_size,
    const void *source,
    uint32_t source_size,
    void *scratch
);
APPLE_LZBITMAP_PUBLIC size_t lzbitmap_decode_body(
    void *destination,
    uint32_t destination_size,
    const void *source,
    uint32_t source_size,
    void *scratch
);
APPLE_LZBITMAP_PUBLIC size_t lzbitmap_decode_buffer(
    void *destination,
    size_t destination_size,
    const void *source,
    size_t source_size,
    void *scratch
);
APPLE_LZBITMAP_PUBLIC size_t lzbitmap_decode_scratch_size(uint32_t algorithm);
