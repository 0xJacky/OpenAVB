/*
 * Copyright (c) 2005-2017 Apple Inc. All Rights Reserved.
 * 
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 *
 * Modified by owngoal-dev, 2026: item-header decoding, scalar parsers
 * (BOOLEAN, INTEGER, BIT STRING) and DERDecodeItemPartialBufferGetLength
 * rewritten for the freestanding ROM; definite-length only; canonical-DER
 * length checks and BIT STRING padding validation added; split from the
 * sequence routines (apple_libder_sequence.c).
 *
 * Upstream: apple-oss-distributions/Security, tag Security-57740.60.18,
 *   OSX/libsecurity_keychain/libDER/libDER/DER_Decode.c
 * SPDX-License-Identifier: APSL-2.0 (see LICENSES/APSL-2.0.txt)
 */

#include "avp/image/der/apple_libder.h"

#include <limits.h>

typedef struct {
    DERTag tag;
    DERSize content_offset;
    DERSize content_length;
} DERHeader;

static DERReturn decode_header(const DERItem *der, DERHeader *header) {
    if (der->length < 2) {
        return DR_DecodeError;
    }

    DERSize cursor = 0;
    const DERByte first_tag = der->data[cursor++];
    DERTag tag_number = first_tag & 0x1fU;
    if (tag_number == 0x1fU) {
        if (der->data[cursor] == 0x80U || der->data[cursor] < 0x1fU) {
            return DR_DecodeError;
        }
        tag_number = 0;
        DERByte tag_byte;
        do {
            if (der->length - cursor < 2 || (tag_number >> 57) != 0) {
                return DR_DecodeError;
            }
            tag_byte = der->data[cursor++];
            tag_number = (tag_number << 7) | (tag_byte & 0x7fU);
        } while ((tag_byte & 0x80U) != 0);
        if ((tag_number >> 61) != 0) {
            return DR_DecodeError;
        }
    }
    header->tag = tag_number | ((DERTag)(first_tag & 0xe0U) << 56);

    const DERByte first_length = der->data[cursor++];
    if ((first_length & 0x80U) == 0) {
        header->content_offset = cursor;
        header->content_length = first_length;
        return DR_Success;
    }

    const DERSize length_bytes = first_length & 0x7fU;
    if (length_bytes == 0 || length_bytes > sizeof(DERSize) ||
        der->length - cursor < length_bytes || der->data[cursor] == 0) {
        return DR_DecodeError;
    }

    DERSize content_length = 0;
    for (DERSize index = 0; index < length_bytes; ++index) {
        content_length = (content_length << 8) | der->data[cursor++];
    }
    if (content_length < 0x80) {
        return DR_DecodeError;
    }
    header->content_offset = cursor;
    header->content_length = content_length;
    return DR_Success;
}

DERReturn DERDecodeItemPartialBufferGetLength(
    const DERItem *der,
    DERDecodedInfo *decoded,
    DERSize *encodedLength
) {
    DERHeader header;
    const DERReturn result = decode_header(der, &header);
    if (result != DR_Success) {
        return result;
    }

    const DERSize available = der->length - header.content_offset;
    if (encodedLength == NULL && header.content_length > available) {
        return DR_DecodeError;
    }
    decoded->tag = header.tag;
    decoded->content.data = der->data + header.content_offset;
    decoded->content.length = header.content_length < available
        ? header.content_length
        : available;
    if (encodedLength != NULL) {
        *encodedLength = header.content_length;
    }
    return DR_Success;
}

DERReturn DERDecodeItem(const DERItem *der, DERDecodedInfo *decoded) {
    return DERDecodeItemPartialBufferGetLength(der, decoded, NULL);
}

DERReturn DERParseBitString(
    const DERItem *contents,
    DERItem *bitStringBytes,
    DERByte *numUnusedBits
) {
    *numUnusedBits = 0;
    bitStringBytes->data = NULL;
    bitStringBytes->length = 0;
    if (contents->length == 0) {
        return DR_DecodeError;
    }

    const DERByte unused = contents->data[0];
    *numUnusedBits = unused;
    if (contents->length == 1) {
        return unused == 0 ? DR_Success : DR_DecodeError;
    }
    if (unused > 7) {
        return DR_DecodeError;
    }
    const DERByte trailing_mask = (DERByte)(0xffU >> (8U - unused));
    if ((contents->data[contents->length - 1] & trailing_mask) != 0) {
        return DR_DecodeError;
    }

    bitStringBytes->data = contents->data + 1;
    bitStringBytes->length = contents->length - 1;
    return DR_Success;
}

DERReturn DERParseBoolean(const DERItem *contents, bool *value) {
    if (contents->length != 1 ||
        (contents->data[0] != 0 && contents->data[0] != 0xffU)) {
        return DR_DecodeError;
    }
    *value = contents->data[0] != 0;
    return DR_Success;
}

DERReturn DERParseBooleanWithDefault(
    const DERItem *contents,
    bool defaultValue,
    bool *value
) {
    if (contents->length == 0) {
        *value = defaultValue;
        return DR_Success;
    }
    return DERParseBoolean(contents, value);
}

DERReturn DERParseInteger64(const DERItem *contents, uint64_t *value) {
    const DERByte *data = contents->data;
    const DERSize length = contents->length;
    if (length == 0 || (data[0] & 0x80U) != 0) {
        return DR_DecodeError;
    }
    if (data[0] == 0) {
        if (length > 1 && (data[1] & 0x80U) == 0) {
            return DR_DecodeError;
        }
        if (length > sizeof(*value) + 1) {
            return DR_BufOverflow;
        }
    } else if (length > sizeof(*value)) {
        return DR_BufOverflow;
    }

    uint64_t decoded = 0;
    for (DERSize index = 0; index < length; ++index) {
        decoded = (decoded << 8) | data[index];
    }
    *value = decoded;
    return DR_Success;
}

DERReturn DERParseInteger(const DERItem *contents, uint32_t *value) {
    uint64_t decoded;
    const DERReturn result = DERParseInteger64(contents, &decoded);
    if (result != DR_Success) {
        return result;
    }
    if (decoded > UINT32_MAX) {
        return DR_BufOverflow;
    }
    *value = (uint32_t)decoded;
    return DR_Success;
}
