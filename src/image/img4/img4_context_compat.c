/*
 * Context offsets and accessor status behavior match the libimg4 ABI that
 * AVPBooter's callers expect. DER traversal is delegated to the shared libDER-based parser.
 */
#include "avp/image/img4/img4_context_compat.h"

#include <limits.h>
#include <stddef.h>

_Static_assert(sizeof(img4_decode_context_t) == 0x1c8,
               "IMG4 decode context ABI size");
_Static_assert(offsetof(img4_decode_context_t, encoded_payload) == 0x08,
               "IMG4 encoded payload offset");
_Static_assert(offsetof(img4_decode_context_t, encoded_manifest) == 0x18,
               "IMG4 encoded manifest offset");
_Static_assert(offsetof(img4_decode_context_t, payload) == 0x58,
               "IMG4 payload offset");
_Static_assert(offsetof(img4_decode_context_t, payload_hash) == 0xb8,
               "IMG4 payload hash offset");
_Static_assert(offsetof(img4_decode_context_t, manifest) == 0xe8,
               "IMG4 manifest offset");
_Static_assert(offsetof(img4_decode_context_t, restore_info) == 0x1a8,
               "IMG4 restore offset");

static bool item_exists(const DERItem *item) {
    return item->data != NULL && item->length != 0;
}

static void zero_bytes(void *output, size_t length) {
    uint8_t *bytes = output;
    for (size_t index = 0; index < length; ++index) {
        bytes[index] = 0;
    }
}

static void normalize_collection(const DERItem *input, DERItem *content) {
    DERDecodedInfo decoded;
    const DERReturn result = DERDecodeItem(input, &decoded);
    if (result == DR_Success &&
        (decoded.tag == ASN1_CONSTR_SEQUENCE ||
         decoded.tag == ASN1_CONSTR_SET) &&
        decoded.content.data + decoded.content.length ==
            input->data + input->length) {
        *content = decoded.content;
    } else {
        *content = *input;
    }
}

DERReturn IMG4_CONTEXT_PUBLIC DERImg4DecodeFindProperty(
    const DERItem *content,
    DERTag expected_tag,
    DERTag value_tag,
    Img4DecodedProperty *property
) {
    if (content == NULL || property == NULL) {
        return DR_ParamErr;
    }
    DERItem normalized;
    normalize_collection(content, &normalized);
    DERItem property_sequence;
    DERReturn result = DERImg4DecodeContentFindItemWithTag(
        &normalized,
        expected_tag,
        &property_sequence
    );
    if (result != DR_Success) {
        return result;
    }
    return DERImg4DecodePropertyWithItem(
        &property_sequence,
        expected_tag,
        value_tag,
        property
    );
}

DERReturn IMG4_CONTEXT_PUBLIC DERImg4DecodeParseManifestPropertiesInternal(
    const img4_decode_context_t *image,
    DERItem *object_dictionary,
    DERItem *manifest_properties,
    uint32_t object_tag
) {
    if (image == NULL ||
        (object_dictionary == NULL && manifest_properties == NULL)) {
        return DR_ParamErr;
    }
    DERDecodedInfo properties_set;
    DERReturn result = DERDecodeItem(
        &image->manifest.properties,
        &properties_set
    );
    if (result != DR_Success) {
        return result;
    }
    if (properties_set.tag != ASN1_CONSTR_SET) {
        return (DERReturn)-1;
    }
    uint32_t version;
    result = DERParseInteger(&image->manifest.version, &version);
    if (result != DR_Success) {
        return result;
    }
    const DERTag body_tag =
        version == 2 ? ASN1_CONSTR_SEQUENCE : ASN1_CONSTR_SET;
    Img4DecodedProperty body;
    result = DERImg4DecodeFindProperty(
        &properties_set.content,
        Img4DecodePrivateTag(object_tag),
        body_tag,
        &body
    );
    if (result != DR_Success) {
        return result;
    }
    if (object_dictionary != NULL) {
        *object_dictionary = body.value.item;
    }
    Img4DecodedProperty nested;
    result = DERImg4DecodeFindProperty(
        &body.value.item,
        Img4DecodePrivateTag(IMG4_MANIFEST_PROPERTIES_MAGIC),
        ASN1_CONSTR_SET,
        &nested
    );
    if (result == DR_Success && manifest_properties != NULL) {
        *manifest_properties = nested.value.item;
    }
    return result;
}

DERReturn IMG4_CONTEXT_PUBLIC DERImg4DecodeParseManifestProperties(
    const img4_decode_context_t *image,
    DERItem *object_dictionary,
    DERItem *manifest_properties
) {
    return DERImg4DecodeParseManifestPropertiesInternal(
        image,
        object_dictionary,
        manifest_properties,
        IMG4_MANIFEST_BODY_MAGIC
    );
}

DERReturn IMG4_CONTEXT_PUBLIC Img4DecodeGetPayload(
    const img4_decode_context_t *image,
    DERItem *payload
) {
    if (image == NULL || payload == NULL) {
        return DR_ParamErr;
    }
    if (!item_exists(&image->payload.payload)) {
        *payload = (DERItem){0};
        return DR_EndOfSequence;
    }
    *payload = image->payload.payload;
    return DR_Success;
}

DERReturn IMG4_CONTEXT_PUBLIC Img4DecodeGetPayloadType(
    const img4_decode_context_t *image,
    uint32_t *payload_type
) {
    if (image == NULL || payload_type == NULL) {
        return DR_ParamErr;
    }
    if (!item_exists(&image->payload.payload)) {
        *payload_type = 0;
        return DR_EndOfSequence;
    }
    const DERReturn result =
        DERParseInteger(&image->payload.type, payload_type);
    if (result != DR_Success) {
        *payload_type = 0;
    }
    return result;
}

DERReturn IMG4_CONTEXT_PUBLIC Img4DecodePayloadCompressionInfoExists(
    const img4_decode_context_t *image,
    bool *exists
) {
    if (image == NULL || exists == NULL) {
        return DR_ParamErr;
    }
    *exists = item_exists(&image->payload.compression);
    return DR_Success;
}

DERReturn IMG4_CONTEXT_PUBLIC Img4DecodeGetPayloadCompressionInfo(
    const img4_decode_context_t *image,
    uint32_t *compression_algorithm,
    uint32_t *uncompressed_size
) {
    if (compression_algorithm != NULL && uncompressed_size != NULL) {
        *compression_algorithm = UINT32_MAX;
        *uncompressed_size = UINT32_MAX;
    }
    if (image == NULL || compression_algorithm == NULL ||
        uncompressed_size == NULL) {
        return DR_ParamErr;
    }
    DERReturn result = DR_EndOfSequence;
    if (item_exists(&image->payload.payload) &&
        item_exists(&image->payload.compression)) {
        DERItem decoded[2];
        result = DERImg4DecodePayloadCompression(
            &image->payload.compression,
            decoded
        );
        if (result == DR_Success) {
            result = DERParseInteger(&decoded[0], compression_algorithm);
        }
        if (result == DR_Success) {
            result = DERParseInteger(&decoded[1], uncompressed_size);
        }
    }
    if (result != DR_Success) {
        *compression_algorithm = UINT32_MAX;
        *uncompressed_size = UINT32_MAX;
    }
    return result;
}

DERReturn IMG4_CONTEXT_PUBLIC Img4DecodeManifestExists(
    const img4_decode_context_t *image,
    bool *exists
) {
    if (image == NULL || exists == NULL) {
        return DR_ParamErr;
    }
    *exists = image->encoded_manifest.data != NULL;
    return DR_Success;
}

DERReturn IMG4_CONTEXT_PUBLIC Img4DecodeSectionExists(
    const img4_decode_context_t *image,
    uint32_t section,
    bool *exists
) {
    if (image == NULL || exists == NULL || section > IMG4_SECTION_RESTORE_INFO) {
        return DR_ParamErr;
    }
    *exists = section == IMG4_SECTION_MANIFEST
                  ? item_exists(&image->manifest.properties)
                  : item_exists(&image->restore_info.content);
    return DR_Success;
}

DERReturn IMG4_CONTEXT_PUBLIC Img4DecodeGetPropertyFromSection(
    const img4_decode_context_t *image,
    uint32_t section,
    DERTag expected_tag,
    DERTag value_tag,
    Img4DecodedProperty *property
) {
    if (image == NULL || property == NULL ||
        section > IMG4_SECTION_RESTORE_INFO) {
        return DR_ParamErr;
    }
    bool exists;
    DERReturn result = Img4DecodeSectionExists(image, section, &exists);
    if (!exists) {
        zero_bytes(property, sizeof(*property));
        return DR_EndOfSequence;
    }
    DERItem properties;
    if (section == IMG4_SECTION_MANIFEST) {
        result = DERImg4DecodeParseManifestProperties(
            image,
            NULL,
            &properties
        );
    } else {
        properties = image->restore_info.content;
    }
    if (result == DR_Success) {
        result = DERImg4DecodeFindProperty(
            &properties,
            expected_tag,
            value_tag,
            property
        );
    }
    if (result != DR_Success) {
        zero_bytes(property, sizeof(*property));
    }
    return result;
}

DERReturn IMG4_CONTEXT_PUBLIC Img4DecodeGetDataFromSection(
    const img4_decode_context_t *image,
    uint32_t section,
    DERTag property_tag,
    const uint8_t **data,
    DERSize *length
) {
    if (image == NULL || data == NULL || length == NULL) {
        return DR_ParamErr;
    }
    Img4DecodedProperty property;
    const DERReturn result = Img4DecodeGetPropertyFromSection(
        image,
        section,
        Img4DecodePrivateTag((uint32_t)property_tag),
        4,
        &property
    );
    if (result == DR_Success) {
        *data = property.value.item.data;
        *length = property.value.item.length;
    } else {
        *data = NULL;
        *length = 0;
    }
    return result;
}

DERReturn IMG4_CONTEXT_PUBLIC Img4DecodeGetRestoreInfoData(
    const img4_decode_context_t *image,
    DERTag property_tag,
    const uint8_t **data,
    DERSize *length
) {
    return Img4DecodeGetDataFromSection(
        image,
        IMG4_SECTION_RESTORE_INFO,
        property_tag,
        data,
        length
    );
}

DERReturn IMG4_CONTEXT_PUBLIC Img4DecodePayloadPropertiesExists(
    const img4_decode_context_t *image,
    bool *exists
) {
    if (image == NULL || exists == NULL ||
        image->encoded_payload.data == NULL) {
        return DR_ParamErr;
    }
    Img4DecodedPayloadWithProperties decoded;
    const DERReturn result = DERImg4DecodePayloadWithProperties(
        &image->encoded_payload,
        &decoded
    );
    if (result == DR_Success) {
        *exists = decoded.properties.data != NULL;
    }
    return result;
}

DERReturn IMG4_CONTEXT_PUBLIC Img4DecodeGetPayloadProperties(
    const img4_decode_context_t *image,
    DERItem *properties
) {
    if (image == NULL || properties == NULL ||
        image->encoded_payload.data == NULL) {
        return DR_ParamErr;
    }
    Img4DecodedPayloadWithProperties decoded;
    DERReturn result = DERImg4DecodePayloadWithProperties(
        &image->encoded_payload,
        &decoded
    );
    if (result != DR_Success) {
        return result;
    }
    if (decoded.properties.data == NULL) {
        return DR_ParamErr;
    }
    *properties = decoded.properties;
    return DR_Success;
}

DERReturn IMG4_CONTEXT_PUBLIC Img4DecodeGetPropertyData(
    const DERItem *property,
    DERTag property_tag,
    const uint8_t **data,
    uint32_t *length
) {
    if (property == NULL || data == NULL || length == NULL) {
        return DR_ParamErr;
    }
    Img4DecodedProperty decoded;
    DERReturn result = DERImg4DecodeProperty(
        property,
        Img4DecodePrivateTag((uint32_t)property_tag),
        &decoded
    );
    if (result == DR_Success && decoded.value.tag != 4) {
        result = DR_UnexpectedTag;
    }
    if (result == DR_Success && decoded.value.item.length > UINT32_MAX) {
        result = DR_BufOverflow;
    }
    if (result == DR_Success) {
        *data = decoded.value.item.data;
        *length = (uint32_t)decoded.value.item.length;
    } else {
        *data = NULL;
        *length = 0;
    }
    return result;
}

DERReturn IMG4_CONTEXT_PUBLIC Img4DecodeGetPropertyInteger64(
    const DERItem *property,
    DERTag property_tag,
    uint64_t *value
) {
    if (value == NULL) {
        return DR_ParamErr;
    }
    Img4DecodedProperty decoded;
    DERReturn result = DERImg4DecodeProperty(
        property,
        Img4DecodePrivateTag((uint32_t)property_tag),
        &decoded
    );
    if (result == DR_Success && decoded.value.tag != 2) {
        result = DR_UnexpectedTag;
    }
    if (result == DR_Success) {
        result = DERParseInteger64(&decoded.value.item, value);
    }
    if (result != DR_Success) {
        *value = 0;
    }
    return result;
}

DERReturn IMG4_CONTEXT_PUBLIC Img4DecodeGetPropertyBoolean(
    const DERItem *property,
    DERTag property_tag,
    bool *value
) {
    if (value == NULL) {
        return DR_ParamErr;
    }
    Img4DecodedProperty decoded;
    DERReturn result = DERImg4DecodeProperty(
        property,
        Img4DecodePrivateTag((uint32_t)property_tag),
        &decoded
    );
    if (result == DR_Success && decoded.value.tag != 1) {
        result = DR_UnexpectedTag;
    }
    if (result == DR_Success) {
        result = DERParseBoolean(&decoded.value.item, value);
    }
    if (result != DR_Success) {
        *value = false;
    }
    return result;
}

DERReturn IMG4_CONTEXT_PUBLIC Img4DecodeInit(
    const uint8_t *data,
    DERSize length,
    img4_decode_context_t *image
) {
    if (data == NULL || image == NULL) {
        return DR_ParamErr;
    }
    zero_bytes(image, sizeof(*image));
    DERItem encoded = {(uint8_t *)data, length};
    Img4DecodedContainer container;
    DERReturn result = DERImg4Decode(&encoded, &container);
    if (result == DR_Success) {
        result = DERImg4DecodePayload(&container.payload, &image->payload);
    }
    if (result == DR_Success) {
        result = DERImg4DecodeManifest(&container.manifest, &image->manifest);
    }
    if (result == DR_Success) {
        result = DERImg4DecodeRestoreInfo(
            &container.restore_info,
            &image->restore_info
        );
    }
    if (result == DR_Success) {
        image->encoded_payload = container.payload;
        image->encoded_manifest = container.manifest;
    }
    return result;
}
