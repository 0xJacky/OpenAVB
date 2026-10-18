/*
 * pongoOS - https://checkra.in
 *
 * Copyright (C) 2021 checkra1n team
 *
 * This file is part of pongoOS.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Modified by owngoal-dev, 2026: the IMG4/IM4P/IM4M/IM4R schema and strict
 * outer-length checks of src/lib/img4/img4.c, rewritten on top of
 * OpenAVB's libDER-compatible decoder (apple_libder.h) with the output
 * layouts the ROM's callers expect. Upstream: checkra1n/PongoOS, commit
 * 4c9b7541629234147fcc778f0ce4162482aaccef.
 * SPDX-License-Identifier: MIT
 */

#include "avp/image/img4/img4_decode_compat.h"

#include <stddef.h>

enum {
    ASN1_BOOLEAN = 1,
    ASN1_INTEGER = 2,
    ASN1_OCTET_STRING = 4,
    ASN1_IA5_STRING = 22,
};

#define ASN1_CONSTR_CONTEXT(number)                                           \
    ((DERTag)((UINT64_C(5) << 61) | (number)))
#define ASN1_CONSTR_PRIVATE(number)                                           \
    ((DERTag)((UINT64_C(7) << 61) | (number)))

static const DERItemSpec container_specs[] = {
    {offsetof(Img4DecodedContainer, magic), ASN1_IA5_STRING,
     DER_DEC_NO_OPTS},
    {offsetof(Img4DecodedContainer, payload), ASN1_CONSTR_SEQUENCE,
     DER_DEC_SAVE_DER},
    {offsetof(Img4DecodedContainer, manifest), ASN1_CONSTR_CONTEXT(0),
     DER_DEC_OPTIONAL},
    {offsetof(Img4DecodedContainer, restore_info), ASN1_CONSTR_CONTEXT(1),
     DER_DEC_OPTIONAL},
};

static const DERItemSpec payload_specs[] = {
    {offsetof(Img4DecodedPayloadWithProperties, magic), ASN1_IA5_STRING,
     DER_DEC_NO_OPTS},
    {offsetof(Img4DecodedPayloadWithProperties, type), ASN1_IA5_STRING,
     DER_DEC_NO_OPTS},
    {offsetof(Img4DecodedPayloadWithProperties, version), ASN1_IA5_STRING,
     DER_DEC_NO_OPTS},
    {offsetof(Img4DecodedPayloadWithProperties, payload), ASN1_OCTET_STRING,
     DER_DEC_NO_OPTS},
    {offsetof(Img4DecodedPayloadWithProperties, keybag), ASN1_OCTET_STRING,
     DER_DEC_OPTIONAL},
    {offsetof(Img4DecodedPayloadWithProperties, compression),
     ASN1_CONSTR_SEQUENCE,
     DER_DEC_OPTIONAL | DER_DEC_SAVE_DER},
    {offsetof(Img4DecodedPayloadWithProperties, properties),
     ASN1_CONSTR_CONTEXT(0),
     DER_DEC_OPTIONAL},
};

static const DERItemSpec payload_properties_specs[] = {
    {offsetof(Img4DecodedPayloadProperties, magic), ASN1_IA5_STRING,
     DER_DEC_NO_OPTS},
    {offsetof(Img4DecodedPayloadProperties, properties), ASN1_CONSTR_SET,
     DER_DEC_NO_OPTS | DER_DEC_SAVE_DER},
};

static const DERItemSpec manifest_specs[] = {
    {offsetof(Img4DecodedManifest, magic), ASN1_IA5_STRING, DER_DEC_NO_OPTS},
    {offsetof(Img4DecodedManifest, version), ASN1_INTEGER, DER_DEC_NO_OPTS},
    {offsetof(Img4DecodedManifest, properties), ASN1_CONSTR_SET,
     DER_DEC_NO_OPTS | DER_DEC_SAVE_DER},
    {offsetof(Img4DecodedManifest, signature), ASN1_OCTET_STRING,
     DER_DEC_NO_OPTS},
    {offsetof(Img4DecodedManifest, certificates), ASN1_CONSTR_SEQUENCE,
     DER_DEC_NO_OPTS | DER_DEC_SAVE_DER},
};

static const DERItemSpec restore_specs[] = {
    {offsetof(Img4DecodedRestoreInfo, magic), ASN1_IA5_STRING,
     DER_DEC_NO_OPTS},
    {offsetof(Img4DecodedRestoreInfo, content), ASN1_CONSTR_SET,
     DER_DEC_NO_OPTS | DER_DEC_SAVE_DER},
};

static DERReturn parse_fourcc(const DERItem *item, uint32_t *fourcc) {
    if (item == NULL || fourcc == NULL || item->data == NULL ||
        item->length != 4) {
        return DR_DecodeError;
    }
    *fourcc = ((uint32_t)item->data[0] << 24) |
              ((uint32_t)item->data[1] << 16) |
              ((uint32_t)item->data[2] << 8) | item->data[3];
    return DR_Success;
}

static DERReturn require_fourcc(const DERItem *item, uint32_t expected) {
    uint32_t actual;
    const DERReturn result = parse_fourcc(item, &actual);
    if (result != DR_Success) {
        return result;
    }
    return actual == expected ? DR_Success : DR_UnexpectedTag;
}

static DERReturn require_complete_item(const DERItem *encoded,
                                       DERTag expected_tag,
                                       DERDecodedInfo *decoded) {
    if (encoded == NULL || decoded == NULL ||
        (encoded->data == NULL && encoded->length != 0)) {
        return DR_ParamErr;
    }
    const DERReturn result = DERDecodeItem(encoded, decoded);
    if (result != DR_Success) {
        return result;
    }
    if (decoded->tag != expected_tag) {
        return DR_UnexpectedTag;
    }
    if (decoded->content.data + decoded->content.length !=
        encoded->data + encoded->length) {
        return DR_BufOverflow;
    }
    return DR_Success;
}

static DERReturn parse_sequence_fields(const DERItem *encoded,
                                       DERShort item_count,
                                       const DERItemSpec *specs,
                                       void *output, size_t output_size) {
    return DERParseSequence(encoded, item_count, specs, output, output_size);
}

DERTag IMG4_DECODE_PUBLIC Img4DecodePrivateTag(uint32_t fourcc) {
    return ASN1_CONSTR_PRIVATE(fourcc);
}

DERReturn IMG4_DECODE_PUBLIC DERImg4Decode(
    const DERItem *encoded, Img4DecodedContainer *container) {
    if (container == NULL) {
        return DR_ParamErr;
    }
    DERDecodedInfo decoded;
    DERReturn result =
        require_complete_item(encoded, ASN1_CONSTR_SEQUENCE, &decoded);
    if (result == DR_Success) {
        result = DERParseSequenceContent(
            &decoded.content,
            (DERShort)(sizeof(container_specs) / sizeof(container_specs[0])),
            container_specs, container, sizeof(*container));
    }
    if (result != DR_Success) {
        return result;
    }
    return require_fourcc(&container->magic, IMG4_MAGIC);
}

DERReturn IMG4_DECODE_PUBLIC DERImg4DecodePayloadWithProperties(
    const DERItem *encoded, Img4DecodedPayloadWithProperties *payload) {
    if (payload == NULL) {
        return DR_ParamErr;
    }
    DERReturn result = parse_sequence_fields(
        encoded, (DERShort)(sizeof(payload_specs) / sizeof(payload_specs[0])),
        payload_specs, payload, sizeof(*payload));
    if (result != DR_Success) {
        return result;
    }
    result = require_fourcc(&payload->magic, IMG4_PAYLOAD_MAGIC);
    if (result != DR_Success) {
        return result;
    }
    if (payload->compression.data != NULL) {
        DERItem compression[2];
        result =
            DERImg4DecodePayloadCompression(&payload->compression, compression);
        if (result != DR_Success) {
            return result;
        }
    }
    if (payload->properties.data != NULL) {
        Img4DecodedPayloadProperties properties;
        result = DERImg4DecodePayloadProperties(&payload->properties,
                                                &properties);
    }
    return result;
}

DERReturn IMG4_DECODE_PUBLIC DERImg4DecodePayloadProperties(
    const DERItem *encoded, Img4DecodedPayloadProperties *properties) {
    if (encoded == NULL || properties == NULL) {
        return DR_ParamErr;
    }
    *properties = (Img4DecodedPayloadProperties){0};
    if (encoded->data == NULL && encoded->length == 0) {
        return DR_Success;
    }
    DERReturn result = parse_sequence_fields(
        encoded,
        (DERShort)(sizeof(payload_properties_specs) /
                   sizeof(payload_properties_specs[0])),
        payload_properties_specs, properties, sizeof(*properties));
    if (result != DR_Success) {
        return result;
    }
    return require_fourcc(&properties->magic, IMG4_PAYLOAD_PROPERTIES_MAGIC);
}

DERReturn IMG4_DECODE_PUBLIC DERImg4DecodePayload(
    const DERItem *encoded, Img4DecodedPayload *payload) {
    if (payload == NULL) {
        return DR_ParamErr;
    }
    Img4DecodedPayloadWithProperties extended;
    const DERReturn result =
        DERImg4DecodePayloadWithProperties(encoded, &extended);
    if (result != DR_Success) {
        return result;
    }
    payload->magic = extended.magic;
    payload->type = extended.type;
    payload->version = extended.version;
    payload->payload = extended.payload;
    payload->keybag = extended.keybag;
    payload->compression = extended.compression;
    return DR_Success;
}

DERReturn IMG4_DECODE_PUBLIC DERImg4DecodeManifestCommon(
    const DERItem *encoded, DERSize item_count,
    const DERItemSpec *specifications, DERItem *decoded,
    uint32_t expected_magic) {
    if (decoded == NULL || specifications == NULL || item_count == 0 ||
        item_count > UINT16_MAX) {
        return DR_ParamErr;
    }
    if (encoded != NULL && encoded->data == NULL && encoded->length == 0) {
        return DR_Success;
    }
    DERReturn result = parse_sequence_fields(
        encoded, (DERShort)item_count, specifications, decoded,
        sizeof(Img4DecodedManifest));
    if (result != DR_Success) {
        return result;
    }
    result = require_fourcc(&decoded[0], expected_magic);
    if (result != DR_Success) {
        return result;
    }
    uint32_t version;
    result = DERParseInteger(&decoded[1], &version);
    if (result != DR_Success) {
        return result;
    }
    return version <= 2 ? DR_Success : DR_UnexpectedTag;
}

DERReturn IMG4_DECODE_PUBLIC DERImg4DecodeManifest(
    const DERItem *encoded, Img4DecodedManifest *manifest) {
    if (manifest == NULL) {
        return DR_ParamErr;
    }
    return DERImg4DecodeManifestCommon(
        encoded, sizeof(manifest_specs) / sizeof(manifest_specs[0]),
        manifest_specs, (DERItem *)manifest, IMG4_MANIFEST_MAGIC);
}

DERReturn IMG4_DECODE_PUBLIC DERImg4DecodeRestoreInfo(
    const DERItem *encoded, Img4DecodedRestoreInfo *restore_info) {
    if (encoded == NULL) {
        return DR_Success;
    }
    if (restore_info == NULL) {
        return DR_ParamErr;
    }
    *restore_info = (Img4DecodedRestoreInfo){0};
    if (encoded->data == NULL && encoded->length == 0) {
        return DR_Success;
    }
    DERReturn result = parse_sequence_fields(
        encoded, (DERShort)(sizeof(restore_specs) / sizeof(restore_specs[0])),
        restore_specs, restore_info, sizeof(*restore_info));
    if (result != DR_Success) {
        return result;
    }
    return require_fourcc(&restore_info->magic, IMG4_RESTORE_MAGIC);
}

static DERReturn decode_property(const DERItem *encoded,
                                 DERTag expected_private_tag,
                                 DERTag expected_value_tag,
                                 bool check_value_tag,
                                 Img4DecodedProperty *property) {
    if (encoded == NULL || property == NULL) {
        return DR_ParamErr;
    }
    DERDecodedInfo sequence;
    DERReturn result = DERDecodeItem(encoded, &sequence);
    if (result != DR_Success || sequence.tag != ASN1_CONSTR_SEQUENCE ||
        sequence.content.data + sequence.content.length !=
            encoded->data + encoded->length) {
        return result == DR_Success ? DR_DecodeError : result;
    }
    DERSequence fields;
    DERDecodeSeqContentInit(&sequence.content, &fields);
    DERDecodedInfo key;
    DERDecodedInfo value;
    result = DERDecodeSeqNext(&fields, &key);
    if (result != DR_Success || key.tag != ASN1_IA5_STRING) {
        return result == DR_Success ? DR_UnexpectedTag : result;
    }
    result = DERDecodeSeqNext(&fields, &value);
    if (result != DR_Success ||
        (check_value_tag && value.tag != expected_value_tag)) {
        return result == DR_Success ? DR_UnexpectedTag : result;
    }
    DERDecodedInfo trailing;
    if (DERDecodeSeqNext(&fields, &trailing) != DR_EndOfSequence) {
        return DR_DecodeError;
    }
    uint32_t key_fourcc;
    result = parse_fourcc(&key.content, &key_fourcc);
    if (result != DR_Success ||
        Img4DecodePrivateTag(key_fourcc) != expected_private_tag) {
        return result == DR_Success ? DR_UnexpectedTag : result;
    }
    property->key.item = key.content;
    property->key.tag = expected_private_tag;
    property->value.item = value.content;
    property->value.tag = value.tag;
    return DR_Success;
}

DERReturn IMG4_DECODE_PUBLIC DERImg4DecodeProperty(
    const DERItem *encoded, DERTag expected_private_tag,
    Img4DecodedProperty *property) {
    return decode_property(encoded, expected_private_tag, 0, false, property);
}

DERReturn IMG4_DECODE_PUBLIC DERImg4DecodeFindPropertyInSequence(
    DERSequence *sequence, DERTag expected_private_tag,
    DERTag expected_value_tag, Img4DecodedProperty *property) {
    if (sequence == NULL || property == NULL) {
        return DR_ParamErr;
    }
    for (;;) {
        DERDecodedInfo item;
        DERReturn result = DERDecodeSeqNext(sequence, &item);
        if (result != DR_Success) {
            return result;
        }
        if (item.tag == expected_private_tag) {
            return DERImg4DecodePropertyWithItem(&item.content,
                                                 expected_private_tag,
                                                 expected_value_tag, property);
        }
    }
}

DERReturn IMG4_DECODE_PUBLIC DERImg4DecodeContentFindItemWithTag(
    const DERItem *sequence_content, DERTag tag, DERItem *content) {
    if (sequence_content == NULL || content == NULL) {
        return DR_ParamErr;
    }
    DERSequence sequence;
    DERDecodeSeqContentInit(sequence_content, &sequence);
    for (;;) {
        DERDecodedInfo item;
        DERReturn result = DERDecodeSeqNext(&sequence, &item);
        if (result != DR_Success) {
            return result;
        }
        if (item.tag == tag) {
            *content = item.content;
            return DR_Success;
        }
    }
}

DERReturn IMG4_DECODE_PUBLIC DERImg4DecodePayloadCompression(
    const DERItem *encoded, DERItem decoded[2]) {
    if (encoded == NULL || decoded == NULL) {
        return DR_ParamErr;
    }
    static const DERItemSpec specs[] = {
        {0, ASN1_INTEGER, DER_DEC_NO_OPTS},
        {sizeof(DERItem), ASN1_INTEGER, DER_DEC_NO_OPTS},
    };
    DERReturn result = parse_sequence_fields(
        encoded, (DERShort)(sizeof(specs) / sizeof(specs[0])), specs, decoded,
        2 * sizeof(*decoded));
    if (result != DR_Success) {
        return result;
    }
    uint32_t algorithm;
    result = DERParseInteger(&decoded[0], &algorithm);
    if (result != DR_Success) {
        return result;
    }
    return algorithm < 2 ? DR_Success : DR_ParamErr;
}

DERReturn IMG4_DECODE_PUBLIC DERImg4DecodePropertyWithItem(
    const DERItem *encoded, DERTag expected_private_tag,
    DERTag expected_value_tag, Img4DecodedProperty *property) {
    return decode_property(encoded, expected_private_tag, expected_value_tag,
                           true, property);
}
