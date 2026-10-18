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
 * Modified by owngoal-dev, 2026: declarations merged from libDER.h,
 * DER_Decode.h and asn1Types.h into one header; reduced to the decoding
 * subset the ROM uses; fixed-width DERTag/DERSize types; enum values
 * spelled out; functions marked used/retain for the freestanding build.
 *
 * Upstream: apple-oss-distributions/Security, tag Security-57740.60.18,
 *   OSX/libsecurity_keychain/libDER/libDER/{libDER.h,DER_Decode.h,asn1Types.h}
 * SPDX-License-Identifier: APSL-2.0 (see LICENSES/APSL-2.0.txt)
 */

#ifndef AVP_APPLE_LIBDER_H
#define AVP_APPLE_LIBDER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef uint8_t DERByte;
typedef size_t DERSize;
typedef uint16_t DERShort;
typedef uint64_t DERTag;

typedef struct {
    DERByte *data;
    DERSize length;
} DERItem;

typedef struct {
    DERTag tag;
    DERItem content;
} DERDecodedInfo;

typedef struct {
    DERByte *nextItem;
    DERByte *end;
} DERSequence;

typedef struct {
    DERSize offset;
    DERTag tag;
    DERShort options;
} DERItemSpec;

typedef enum {
    DR_Success = 0,
    DR_EndOfSequence = 1,
    DR_UnexpectedTag = 2,
    DR_DecodeError = 3,
    DR_Unimplemented = 4,
    DR_IncompleteSeq = 5,
    DR_ParamErr = 6,
    DR_BufOverflow = 7,
} DERReturn;

enum {
    DER_DEC_NO_OPTS = 0,
    DER_DEC_OPTIONAL = 1 << 0,
    DER_DEC_ASN_ANY = 1 << 1,
    DER_DEC_SKIP = 1 << 2,
    DER_DEC_SAVE_DER = 1 << 3,
};

#define ASN1_CONSTR_SEQUENCE ((DERTag)(UINT64_C(1) << 61 | 0x10))
#define ASN1_CONSTR_SET ((DERTag)(UINT64_C(1) << 61 | 0x11))

#define AVP_LIBDER_PUBLIC __attribute__((used, retain))

AVP_LIBDER_PUBLIC DERReturn DERDecodeItem(
    const DERItem *der,
    DERDecodedInfo *decoded
);
AVP_LIBDER_PUBLIC DERReturn DERDecodeItemPartialBufferGetLength(
    const DERItem *der,
    DERDecodedInfo *decoded,
    DERSize *encodedLength
);
AVP_LIBDER_PUBLIC DERReturn DERParseBitString(
    const DERItem *contents,
    DERItem *bitStringBytes,
    DERByte *numUnusedBits
);
AVP_LIBDER_PUBLIC DERReturn DERParseBoolean(
    const DERItem *contents,
    bool *value
);
AVP_LIBDER_PUBLIC DERReturn DERParseBooleanWithDefault(
    const DERItem *contents,
    bool defaultValue,
    bool *value
);
AVP_LIBDER_PUBLIC DERReturn DERParseInteger(
    const DERItem *contents,
    uint32_t *value
);
AVP_LIBDER_PUBLIC DERReturn DERParseInteger64(
    const DERItem *contents,
    uint64_t *value
);
AVP_LIBDER_PUBLIC DERReturn DERDecodeSeqInit(
    const DERItem *der,
    DERTag *tag,
    DERSequence *derSeq
);
AVP_LIBDER_PUBLIC DERReturn DERDecodeSeqContentInit(
    const DERItem *content,
    DERSequence *derSeq
);
AVP_LIBDER_PUBLIC DERReturn DERDecodeSeqNext(
    DERSequence *derSeq,
    DERDecodedInfo *decoded
);
AVP_LIBDER_PUBLIC DERReturn DERParseSequence(
    const DERItem *der,
    DERShort numItems,
    const DERItemSpec *itemSpecs,
    void *dest,
    DERSize sizeToZero
);
AVP_LIBDER_PUBLIC DERReturn DERParseSequenceContent(
    const DERItem *content,
    DERShort numItems,
    const DERItemSpec *itemSpecs,
    void *dest,
    DERSize sizeToZero
);
AVP_LIBDER_PUBLIC bool DEROidCompare(
    const DERItem *oid1,
    const DERItem *oid2
);

#endif
