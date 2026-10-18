/*
 * Copyright (c) 2015-2016, Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 *
 * 1.  Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 *
 * 2.  Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer
 *     in the documentation and/or other materials provided with the distribution.
 *
 * 3.  Neither the name of the copyright holder(s) nor the names of any contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Modified by owngoal-dev, 2026: the V2-to-V1 block-header conversion
 * (lzfse_decode_v1 and its frequency-table decoder) from
 * src/lzfse_decode_base.c, adapted as lzfseDecodeV1 for the ROM's
 * compression interface. Upstream: github.com/lzfse/lzfse, commit
 * e634ca58b4821d9f3d560cdc6df5dec02ffc93fd.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "avp/image/compression/apple_compression.h"

#include <string.h>

static int decode_frequency_value(uint32_t bits, int *bit_count) {
    static const int8_t bit_count_table[32] = {
        2, 3, 2, 5, 2, 3, 2, 8, 2, 3, 2, 5, 2, 3, 2, 14,
        2, 3, 2, 5, 2, 3, 2, 8, 2, 3, 2, 5, 2, 3, 2, 14,
    };
    static const int8_t value_table[32] = {
        0, 2, 1, 4, 0, 3, 1, -1, 0, 2, 1, 5, 0, 3, 1, -1,
        0, 2, 1, 6, 0, 3, 1, -1, 0, 2, 1, 7, 0, 3, 1, -1,
    };

    uint32_t index = bits & 31;
    int count = bit_count_table[index];
    *bit_count = count;
    if (count == 8) {
        return 8 + (int)((bits >> 4) & 0xf);
    }
    if (count == 14) {
        return 24 + (int)((bits >> 4) & 0x3ff);
    }
    return value_table[index];
}

static uint32_t packed_field(uint64_t value, int offset, int bit_count) {
    if (bit_count == 32) {
        return (uint32_t)(value >> offset);
    }
    return (uint32_t)((value >> offset) & ((UINT64_C(1) << bit_count) - 1));
}

int lzfseDecodeV1(
    lzfse_compressed_block_header_v1 *output,
    const lzfse_compressed_block_header_v2 *input
) {
    memset(output, 0, sizeof(*output));

    uint64_t packed0 = input->packed_fields[0];
    uint64_t packed1 = input->packed_fields[1];
    uint64_t packed2 = input->packed_fields[2];
    output->magic = LZFSE_COMPRESSEDV1_BLOCK_MAGIC;
    output->n_raw_bytes = input->n_raw_bytes;

    output->n_literals = packed_field(packed0, 0, 20);
    output->n_literal_payload_bytes = packed_field(packed0, 20, 20);
    output->literal_bits = (int32_t)packed_field(packed0, 60, 3) - 7;
    output->literal_state[0] = (uint16_t)packed_field(packed1, 0, 10);
    output->literal_state[1] = (uint16_t)packed_field(packed1, 10, 10);
    output->literal_state[2] = (uint16_t)packed_field(packed1, 20, 10);
    output->literal_state[3] = (uint16_t)packed_field(packed1, 30, 10);

    output->n_matches = packed_field(packed0, 40, 20);
    output->n_lmd_payload_bytes = packed_field(packed1, 40, 20);
    output->lmd_bits = (int32_t)packed_field(packed1, 60, 3) - 7;
    output->l_state = (uint16_t)packed_field(packed2, 32, 10);
    output->m_state = (uint16_t)packed_field(packed2, 42, 10);
    output->d_state = (uint16_t)packed_field(packed2, 52, 10);
    output->n_payload_bytes =
        output->n_literal_payload_bytes + output->n_lmd_payload_bytes;

    uint16_t *destination = &output->l_freq[0];
    const uint8_t *source = &input->freq[0];
    const uint8_t *source_end =
        (const uint8_t *)input + packed_field(packed2, 0, 32);
    uint32_t accumulator = 0;
    int accumulator_bits = 0;

    if (source_end == source) {
        return LZFSE_STATUS_OK;
    }

    const int symbol_count =
        LZFSE_ENCODE_L_SYMBOLS + LZFSE_ENCODE_M_SYMBOLS +
        LZFSE_ENCODE_D_SYMBOLS + LZFSE_ENCODE_LITERAL_SYMBOLS;
    for (int index = 0; index < symbol_count; ++index) {
        while (source < source_end && accumulator_bits + 8 <= 32) {
            accumulator |= (uint32_t)(*source) << accumulator_bits;
            accumulator_bits += 8;
            ++source;
        }

        int consumed_bits = 0;
        destination[index] = (uint16_t)decode_frequency_value(
            accumulator,
            &consumed_bits
        );
        if (consumed_bits > accumulator_bits) {
            return LZFSE_STATUS_SRC_EMPTY;
        }
        accumulator >>= consumed_bits;
        accumulator_bits -= consumed_bits;
    }

    if (accumulator_bits >= 8 || source != source_end) {
        return LZFSE_STATUS_ERROR;
    }
    return LZFSE_STATUS_OK;
}
