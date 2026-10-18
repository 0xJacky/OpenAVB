/*
 * Copyright (c) 2016-2016 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 *
 * Modified by owngoal-dev, 2026: lz4_decode rewritten as a portable,
 * bounded scalar decoder with little-endian load helpers and early
 * returns on truncated input; it also serves as the lz4_decode_asm
 * entry (no Arm64 assembly fast path). Added lz4raw_decode_buffer and
 * an lz4_decode_buffer adapter for the bv41/bv4-/bv4$ frame records.
 *
 * Upstream: apple-oss-distributions/xnu, commit f6217f891ac0bb64f3d375211650a4c1ff8ca1ea,
 *   osfmk/vm/lz4.c
 * SPDX-License-Identifier: APSL-2.0 (see LICENSES/APSL-2.0.txt)
 */

#include "avp/image/compression/apple_deflate_lz4.h"

#include <string.h>

static uint32_t load32le(const uint8_t *bytes) {
    return (uint32_t)bytes[0] |
        ((uint32_t)bytes[1] << 8) |
        ((uint32_t)bytes[2] << 16) |
        ((uint32_t)bytes[3] << 24);
}

static uint16_t load16le(const uint8_t *bytes) {
    return (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8));
}

int lz4_decode(
    uint8_t **destination,
    uint8_t *destination_begin,
    uint8_t *destination_end,
    const uint8_t **source,
    const uint8_t *source_end
) {
    uint8_t *output = *destination;
    const uint8_t *input = *source;
    if (destination_end <= output) {
        return 0;
    }

    while (input < source_end) {
        *source = input;
        *destination = output;

        uint8_t command = *input++;
        size_t literal_count = command >> 4;
        size_t match_count = 4 + (command & 15);
        if (literal_count == 15) {
            uint8_t extension;
            do {
                if (input >= source_end) {
                    return 1;
                }
                extension = *input++;
                literal_count += extension;
            } while (extension == 255);
        }

        if (literal_count > (size_t)(source_end - input)) {
            return 1;
        }
        if (literal_count > (size_t)(destination_end - output)) {
            size_t available = (size_t)(destination_end - output);
            memcpy(output, input, available);
            output += available;
            *destination = output;
            *source = input;
            return 0;
        }
        memcpy(output, input, literal_count);
        input += literal_count;
        output += literal_count;

        if (input >= source_end) {
            *destination = output;
            *source = input;
            return 0;
        }
        if ((size_t)(source_end - input) < 2) {
            return 1;
        }

        size_t distance = load16le(input);
        input += 2;
        if (distance == 0 || distance > (size_t)(output - destination_begin)) {
            return 1;
        }
        if (match_count == 19) {
            uint8_t extension;
            do {
                if (input >= source_end) {
                    return 1;
                }
                extension = *input++;
                match_count += extension;
            } while (extension == 255);
        }

        size_t available = (size_t)(destination_end - output);
        size_t copied = match_count < available ? match_count : available;
        uint8_t *match = output - distance;
        for (size_t index = 0; index < copied; ++index) {
            output[index] = match[index];
        }
        output += copied;
        if (copied != match_count) {
            *destination = output;
            *source = input;
            return 0;
        }
    }

    *destination = output;
    *source = input;
    return 0;
}

int lz4_decode_asm(
    uint8_t **destination,
    uint8_t *destination_begin,
    uint8_t *destination_end,
    const uint8_t **source,
    const uint8_t *source_end
) {
    return lz4_decode(
        destination,
        destination_begin,
        destination_end,
        source,
        source_end
    );
}

size_t lz4raw_decode_buffer(
    uint8_t *destination,
    size_t destination_size,
    const uint8_t *source,
    size_t source_size,
    void *scratch
) {
    (void)scratch;
    uint8_t *output = destination;
    const uint8_t *input = source;
    if (lz4_decode(
        &output,
        destination,
        destination + destination_size,
        &input,
        source + source_size
    ) != 0) {
        return 0;
    }
    return (size_t)(output - destination);
}

size_t lz4_decode_buffer(
    uint8_t *destination,
    size_t destination_size,
    const uint8_t *source,
    size_t source_size,
    void *scratch
) {
    (void)scratch;
    uint8_t *output = destination;
    const uint8_t *input = source;
    const uint8_t *source_end = source + source_size;
    uint8_t *destination_end = destination + destination_size;

    while ((size_t)(source_end - input) >= 4) {
        uint32_t magic = load32le(input);
        if (magic == AVP_LZ4_END_MAGIC) {
            return (size_t)(output - destination);
        }

        if (magic == AVP_LZ4_RAW_MAGIC) {
            if ((size_t)(source_end - input) < 8) {
                return 0;
            }
            size_t raw_size = load32le(input + 4);
            input += 8;
            if (raw_size > (size_t)(source_end - input) ||
                raw_size > (size_t)(destination_end - output)) {
                return 0;
            }
            memcpy(output, input, raw_size);
            input += raw_size;
            output += raw_size;
            continue;
        }

        if (magic == AVP_LZ4_COMPRESSED_MAGIC) {
            if ((size_t)(source_end - input) < 12) {
                return 0;
            }
            size_t raw_size = load32le(input + 4);
            size_t payload_size = load32le(input + 8);
            input += 12;
            if (payload_size > (size_t)(source_end - input) ||
                raw_size > (size_t)(destination_end - output)) {
                return 0;
            }

            const uint8_t *payload_end = input + payload_size;
            uint8_t *block_end = output + raw_size;
            if (lz4_decode(
                &output,
                destination,
                block_end,
                &input,
                payload_end
            ) != 0 || input != payload_end || output != block_end) {
                return 0;
            }
            continue;
        }
        return 0;
    }
    return 0;
}
