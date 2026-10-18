#pragma once

#include <stddef.h>
#include <stdint.h>

#define APPLE_LZSS_PUBLIC __attribute__((used, retain))

APPLE_LZSS_PUBLIC size_t runtime_lzss_decompress(
    uint8_t *destination,
    size_t destination_size,
    const uint8_t *source,
    size_t source_size
);
