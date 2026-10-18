#pragma once

#include "avp/image/der/apple_libder.h"

#include <stdbool.h>
#include <stdint.h>

#define IMG4_FOURCC(a, b, c, d)                                              \
    (((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) |                         \
     ((uint32_t)(c) << 8) | (uint32_t)(d))

enum {
    IMG4_MAGIC = IMG4_FOURCC('I', 'M', 'G', '4'),
    IMG4_PAYLOAD_MAGIC = IMG4_FOURCC('I', 'M', '4', 'P'),
    IMG4_MANIFEST_MAGIC = IMG4_FOURCC('I', 'M', '4', 'M'),
    IMG4_RESTORE_MAGIC = IMG4_FOURCC('I', 'M', '4', 'R'),
    IMG4_PAYLOAD_PROPERTIES_MAGIC = IMG4_FOURCC('P', 'A', 'Y', 'P'),
    IMG4_MANIFEST_BODY_MAGIC = IMG4_FOURCC('M', 'A', 'N', 'B'),
};

typedef struct {
    DERItem magic;
    DERItem payload;
    DERItem manifest;
    DERItem restore_info;
} Img4DecodedContainer;

typedef struct {
    DERItem magic;
    DERItem properties;
} Img4DecodedPayloadProperties;

typedef struct {
    DERItem magic;
    DERItem type;
    DERItem version;
    DERItem payload;
    DERItem keybag;
    DERItem compression;
} Img4DecodedPayload;

typedef struct {
    DERItem magic;
    DERItem type;
    DERItem version;
    DERItem payload;
    DERItem keybag;
    DERItem compression;
    DERItem properties;
    uint8_t reserved[0x30];
} Img4DecodedPayloadWithProperties;

typedef struct {
    DERItem magic;
    DERItem version;
    DERItem properties;
    DERItem signature;
    DERItem certificates;
    uint8_t reserved[0x70];
} Img4DecodedManifest;

typedef struct {
    DERItem magic;
    DERItem content;
} Img4DecodedRestoreInfo;

typedef struct {
    DERItem item;
    DERTag tag;
} Img4Property;

typedef struct {
    Img4Property key;
    Img4Property value;
} Img4DecodedProperty;

#define IMG4_DECODE_PUBLIC __attribute__((used, retain))

IMG4_DECODE_PUBLIC DERReturn DERImg4Decode(
    const DERItem *encoded,
    Img4DecodedContainer *container
);
IMG4_DECODE_PUBLIC DERReturn DERImg4DecodePayloadWithProperties(
    const DERItem *encoded,
    Img4DecodedPayloadWithProperties *payload
);
IMG4_DECODE_PUBLIC DERReturn DERImg4DecodePayloadProperties(
    const DERItem *encoded,
    Img4DecodedPayloadProperties *properties
);
IMG4_DECODE_PUBLIC DERReturn DERImg4DecodePayload(
    const DERItem *encoded,
    Img4DecodedPayload *payload
);
IMG4_DECODE_PUBLIC DERReturn DERImg4DecodeManifest(
    const DERItem *encoded,
    Img4DecodedManifest *manifest
);
IMG4_DECODE_PUBLIC DERReturn DERImg4DecodeManifestCommon(
    const DERItem *encoded,
    DERSize item_count,
    const DERItemSpec *specifications,
    DERItem *decoded,
    uint32_t expected_magic
);
IMG4_DECODE_PUBLIC DERReturn DERImg4DecodeRestoreInfo(
    const DERItem *encoded,
    Img4DecodedRestoreInfo *restore_info
);
IMG4_DECODE_PUBLIC DERReturn DERImg4DecodeProperty(
    const DERItem *encoded,
    DERTag expected_private_tag,
    Img4DecodedProperty *property
);
IMG4_DECODE_PUBLIC DERReturn DERImg4DecodeFindPropertyInSequence(
    DERSequence *sequence,
    DERTag expected_private_tag,
    DERTag expected_value_tag,
    Img4DecodedProperty *property
);
IMG4_DECODE_PUBLIC DERReturn DERImg4DecodeContentFindItemWithTag(
    const DERItem *sequence_content,
    DERTag tag,
    DERItem *content
);
IMG4_DECODE_PUBLIC DERReturn DERImg4DecodePayloadCompression(
    const DERItem *encoded,
    DERItem decoded[2]
);
IMG4_DECODE_PUBLIC DERReturn DERImg4DecodePropertyWithItem(
    const DERItem *encoded,
    DERTag expected_private_tag,
    DERTag expected_value_tag,
    Img4DecodedProperty *property
);

IMG4_DECODE_PUBLIC DERTag Img4DecodePrivateTag(uint32_t fourcc);
