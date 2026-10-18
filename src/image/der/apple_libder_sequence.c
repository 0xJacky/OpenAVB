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
 * Modified by owngoal-dev, 2026: sequence iteration, item-spec sequence
 * parsing and DEROidCompare split out of DER_Decode.c/libDER and adapted
 * to the freestanding ROM (local zeroing helper, fixed-width types).
 *
 * Upstream: apple-oss-distributions/Security, tag Security-57740.60.18,
 *   OSX/libsecurity_keychain/libDER/libDER/DER_Decode.c
 * SPDX-License-Identifier: APSL-2.0 (see LICENSES/APSL-2.0.txt)
 */

#include "avp/image/der/apple_libder.h"

#include <stdint.h>

static void der_zero(void *destination, DERSize length) {
    DERByte *bytes = destination;
    for (DERSize index = 0; index < length; ++index) {
        bytes[index] = 0;
    }
}

DERReturn DERDecodeSeqInit(
    const DERItem *der,
    DERTag *tag,
    DERSequence *derSeq
) {
    DERDecodedInfo decoded;
    const DERReturn result = DERDecodeItem(der, &decoded);
    if (result != DR_Success) {
        return result;
    }
    *tag = decoded.tag;
    if (decoded.tag != ASN1_CONSTR_SEQUENCE && decoded.tag != ASN1_CONSTR_SET) {
        return DR_UnexpectedTag;
    }
    derSeq->nextItem = decoded.content.data;
    derSeq->end = decoded.content.data + decoded.content.length;
    return DR_Success;
}

DERReturn DERDecodeSeqContentInit(
    const DERItem *content,
    DERSequence *derSeq
) {
    derSeq->nextItem = content->data;
    derSeq->end = content->data + content->length;
    return DR_Success;
}

DERReturn DERDecodeSeqNext(
    DERSequence *derSeq,
    DERDecodedInfo *decoded
) {
    if (derSeq->nextItem >= derSeq->end) {
        return DR_EndOfSequence;
    }
    DERItem item = {
        derSeq->nextItem,
        (DERSize)((uintptr_t)derSeq->end - (uintptr_t)derSeq->nextItem),
    };
    const DERReturn result = DERDecodeItem(&item, decoded);
    if (result != DR_Success) {
        return result;
    }
    derSeq->nextItem = decoded->content.data + decoded->content.length;
    return DR_Success;
}

DERReturn DERParseSequence(
    const DERItem *der,
    DERShort numItems,
    const DERItemSpec *itemSpecs,
    void *dest,
    DERSize sizeToZero
) {
    DERDecodedInfo decoded;
    const DERReturn result = DERDecodeItem(der, &decoded);
    if (result != DR_Success) {
        return result;
    }
    if (decoded.tag != ASN1_CONSTR_SEQUENCE) {
        return DR_UnexpectedTag;
    }
    return DERParseSequenceContent(
        &decoded.content,
        numItems,
        itemSpecs,
        dest,
        sizeToZero
    );
}

DERReturn DERParseSequenceContent(
    const DERItem *content,
    DERShort numItems,
    const DERItemSpec *itemSpecs,
    void *dest,
    DERSize sizeToZero
) {
    der_zero(dest, sizeToZero);
    DERSequence sequence;
    DERDecodeSeqContentInit(content, &sequence);

    DERShort spec_index = 0;
    while (spec_index < numItems) {
        DERByte *encoded_item = sequence.nextItem;
        DERDecodedInfo decoded;
        const DERReturn decode_result = DERDecodeSeqNext(&sequence, &decoded);
        if (decode_result == DR_EndOfSequence) {
            for (; spec_index < numItems; ++spec_index) {
                if ((itemSpecs[spec_index].options & DER_DEC_OPTIONAL) == 0) {
                    return DR_IncompleteSeq;
                }
            }
            return DR_Success;
        }
        if (decode_result != DR_Success) {
            return decode_result;
        }

        for (;;) {
            if (spec_index >= numItems) {
                return DR_UnexpectedTag;
            }
            const DERItemSpec *spec = &itemSpecs[spec_index];
            if ((spec->options & DER_DEC_ASN_ANY) != 0 ||
                decoded.tag == spec->tag) {
                break;
            }
            if ((spec->options & DER_DEC_OPTIONAL) == 0) {
                return DR_UnexpectedTag;
            }
            ++spec_index;
        }

        const DERItemSpec *spec = &itemSpecs[spec_index];
        if ((spec->options & DER_DEC_SKIP) == 0) {
            DERItem *output = (DERItem *)((DERByte *)dest + spec->offset);
            *output = decoded.content;
            if ((spec->options & DER_DEC_SAVE_DER) != 0) {
                output->data = encoded_item;
                output->length += (DERSize)(decoded.content.data - encoded_item);
            }
        }

        ++spec_index;
        if (spec_index == numItems) {
            return sequence.nextItem == sequence.end
                ? DR_Success
                : DR_DecodeError;
        }
    }
    return sequence.nextItem == sequence.end ? DR_Success : DR_DecodeError;
}

bool DEROidCompare(const DERItem *oid1, const DERItem *oid2) {
    if (oid1 == NULL || oid2 == NULL || oid1->length != oid2->length) {
        return false;
    }
    DERByte difference = 0;
    for (DERSize index = 0; index < oid1->length; ++index) {
        difference |= oid1->data[index] ^ oid2->data[index];
    }
    return difference == 0;
}
