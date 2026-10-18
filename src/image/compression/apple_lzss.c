/*
 * Copyright (c) 2000-2016 Apple Computer, Inc. All rights reserved.
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
 * Modified by owngoal-dev, 2026: only the LZSS decoder is kept, as
 * runtime_lzss_decompress with size_t source and destination counts and
 * explicit bounds; the encoder and mkext/adler32 code are not included.
 *
 * Upstream: apple-oss-distributions/xnu, commit f6217f891ac0bb64f3d375211650a4c1ff8ca1ea,
 *   libkern/mkext.c
 * SPDX-License-Identifier: APSL-2.0 (see LICENSES/APSL-2.0.txt)
 */

/**************************************************************
*   LZSS.C -- A Data Compression Program
***************************************************************
*    4/6/1989 Haruhiko Okumura
*    Use, distribute, and modify this program freely.
*    Please send me your improved versions.
*        PC-VAN      SCIENCE
*        NIFTY-Serve PAF01022
*        CompuServe  74050,1022
*
**************************************************************/

#include "avp/image/compression/apple_lzss.h"

enum {
    LZSS_RING_SIZE = 4096,
    LZSS_MATCH_LIMIT = 18,
    LZSS_THRESHOLD = 2,
};

size_t runtime_lzss_decompress(
    uint8_t *destination,
    size_t destination_size,
    const uint8_t *source,
    size_t source_size
) {
    uint8_t ring[LZSS_RING_SIZE + LZSS_MATCH_LIMIT - 1];
    size_t output_offset = 0;
    size_t input_offset = 0;
    size_t ring_offset = LZSS_RING_SIZE - LZSS_MATCH_LIMIT;
    unsigned int flags = 0;

    for (size_t index = 0; index < ring_offset; ++index) {
        ring[index] = ' ';
    }

    while (output_offset < destination_size) {
        if (((flags >>= 1) & 0x100) == 0) {
            if (input_offset == source_size) {
                break;
            }
            flags = source[input_offset++] | 0xff00u;
        }

        if ((flags & 1) != 0) {
            if (input_offset == source_size) {
                break;
            }
            uint8_t value = source[input_offset++];
            destination[output_offset++] = value;
            ring[ring_offset] = value;
            ring_offset = (ring_offset + 1) & (LZSS_RING_SIZE - 1);
            continue;
        }

        if (source_size - input_offset < 2) {
            break;
        }
        size_t match_offset = source[input_offset++];
        uint8_t length_byte = source[input_offset++];
        match_offset |= (size_t)(length_byte & 0xf0) << 4;
        size_t match_length = (length_byte & 0x0f) + LZSS_THRESHOLD + 1;
        for (size_t index = 0;
             index < match_length && output_offset < destination_size;
             ++index) {
            uint8_t value = ring[
                (match_offset + index) & (LZSS_RING_SIZE - 1)
            ];
            destination[output_offset++] = value;
            ring[ring_offset] = value;
            ring_offset = (ring_offset + 1) & (LZSS_RING_SIZE - 1);
        }
    }
    return output_offset;
}
