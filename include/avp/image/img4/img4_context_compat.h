#pragma once

#include "avp/image/img4/img4_decode_compat.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    IMG4_SECTION_MANIFEST = 0,
    IMG4_SECTION_RESTORE_INFO = 1,
    IMG4_MANIFEST_PROPERTIES_MAGIC = IMG4_FOURCC('M', 'A', 'N', 'P'),
};

typedef struct img4_decode_context_t {
    uint8_t state[8];
    DERItem encoded_payload;
    DERItem encoded_manifest;
    uint8_t reserved_28[0x30];
    Img4DecodedPayload payload;
    uint8_t payload_hash[0x30];
    Img4DecodedManifest manifest;
    Img4DecodedRestoreInfo restore_info;
} img4_decode_context_t;

#define IMG4_CONTEXT_PUBLIC __attribute__((used, retain))

IMG4_CONTEXT_PUBLIC DERReturn DERImg4DecodeParseManifestProperties(
    const img4_decode_context_t *image,
    DERItem *object_dictionary,
    DERItem *manifest_properties
);
IMG4_CONTEXT_PUBLIC DERReturn Img4DecodeGetPayload(
    const img4_decode_context_t *image,
    DERItem *payload
);
IMG4_CONTEXT_PUBLIC DERReturn Img4DecodeGetPayloadType(
    const img4_decode_context_t *image,
    uint32_t *payload_type
);
IMG4_CONTEXT_PUBLIC DERReturn Img4DecodePayloadCompressionInfoExists(
    const img4_decode_context_t *image,
    bool *exists
);
IMG4_CONTEXT_PUBLIC DERReturn Img4DecodeGetPayloadCompressionInfo(
    const img4_decode_context_t *image,
    uint32_t *compression_algorithm,
    uint32_t *uncompressed_size
);
IMG4_CONTEXT_PUBLIC DERReturn Img4DecodeManifestExists(
    const img4_decode_context_t *image,
    bool *exists
);
IMG4_CONTEXT_PUBLIC DERReturn Img4DecodeSectionExists(
    const img4_decode_context_t *image,
    uint32_t section,
    bool *exists
);
IMG4_CONTEXT_PUBLIC DERReturn Img4DecodeGetRestoreInfoData(
    const img4_decode_context_t *image,
    DERTag property_tag,
    const uint8_t **data,
    DERSize *length
);
IMG4_CONTEXT_PUBLIC DERReturn Img4DecodeGetDataFromSection(
    const img4_decode_context_t *image,
    uint32_t section,
    DERTag property_tag,
    const uint8_t **data,
    DERSize *length
);
IMG4_CONTEXT_PUBLIC DERReturn Img4DecodePayloadPropertiesExists(
    const img4_decode_context_t *image,
    bool *exists
);
IMG4_CONTEXT_PUBLIC DERReturn Img4DecodeGetPayloadProperties(
    const img4_decode_context_t *image,
    DERItem *properties
);
IMG4_CONTEXT_PUBLIC DERReturn Img4DecodeGetPropertyFromSection(
    const img4_decode_context_t *image,
    uint32_t section,
    DERTag expected_tag,
    DERTag value_tag,
    Img4DecodedProperty *property
);
IMG4_CONTEXT_PUBLIC DERReturn Img4DecodeGetPropertyData(
    const DERItem *property,
    DERTag property_tag,
    const uint8_t **data,
    uint32_t *length
);
IMG4_CONTEXT_PUBLIC DERReturn Img4DecodeInit(
    const uint8_t *data,
    DERSize length,
    img4_decode_context_t *image
);
IMG4_CONTEXT_PUBLIC DERReturn DERImg4DecodeFindProperty(
    const DERItem *content,
    DERTag expected_tag,
    DERTag value_tag,
    Img4DecodedProperty *property
);
IMG4_CONTEXT_PUBLIC DERReturn DERImg4DecodeParseManifestPropertiesInternal(
    const img4_decode_context_t *image,
    DERItem *object_dictionary,
    DERItem *manifest_properties,
    uint32_t object_tag
);
IMG4_CONTEXT_PUBLIC DERReturn Img4DecodeGetPropertyInteger64(
    const DERItem *property,
    DERTag property_tag,
    uint64_t *value
);
IMG4_CONTEXT_PUBLIC DERReturn Img4DecodeGetPropertyBoolean(
    const DERItem *property,
    DERTag property_tag,
    bool *value
);

