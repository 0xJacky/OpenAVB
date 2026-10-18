#include "avp/image/boot/boot_image.h"


enum {
    DER_TAG_BOOLEAN = 0x01,
    DER_TAG_OCTET_STRING = 0x04,
    DER_TAG_SEQUENCE = 0x30,
    DER_TAG_SET = 0x31,
    DER_TAG_IA5_STRING = 0x16,
    DER_TAG_CONTEXT_0 = 0xa0,
    DER_TAG_PRIVATE_CONSTRUCTED_LONG = 0xff,
};

typedef struct {
    uint8_t tag;
    uint64_t tag_number;
    const uint8_t *encoded;
    size_t encoded_size;
    const uint8_t *value;
    size_t value_size;
} BootImageTLV;

static bool boot_image_bytes_equal(
    const uint8_t *left,
    const uint8_t *right,
    size_t size
);
static bool boot_image_string(
    const BootImageTLV *tlv,
    const char *text,
    size_t text_size
);

enum {
    BOOT_IMAGE_PROPERTY_DPRO = AVP_BOOT_IMAGE_FOURCC('D', 'P', 'R', 'O'),
    BOOT_IMAGE_PROPERTY_MANM = AVP_BOOT_IMAGE_FOURCC('M', 'A', 'N', 'M'),
};

static bool boot_image_add_size(size_t left, size_t right, size_t *result) {
    if (right > SIZE_MAX - left) {
        return false;
    }
    *result = left + right;
    return true;
}

static bool boot_image_read_length(
    const uint8_t *bytes,
    size_t size,
    size_t *cursor,
    size_t *value_size
) {
    if (*cursor >= size) {
        return false;
    }
    const uint8_t initial = bytes[(*cursor)++];
    if ((initial & 0x80U) == 0) {
        *value_size = initial;
        return true;
    }
    const size_t count = initial & 0x7fU;
    if (count == 0 || count > sizeof(size_t) || count > size - *cursor ||
        bytes[*cursor] == 0) {
        return false;
    }
    size_t length = 0;
    for (size_t index = 0; index < count; ++index) {
        if (length > (SIZE_MAX >> 8)) {
            return false;
        }
        length = (length << 8) | bytes[(*cursor)++];
    }
    if (length < 0x80) {
        return false;
    }
    *value_size = length;
    return true;
}

static bool boot_image_read_tlv(
    const uint8_t *bytes,
    size_t size,
    size_t *cursor,
    BootImageTLV *result
) {
    if (bytes == 0 || cursor == 0 || result == 0 || *cursor >= size) {
        return false;
    }
    const size_t start = *cursor;
    const uint8_t tag = bytes[(*cursor)++];
    uint64_t tag_number = tag & 0x1fU;
    if (tag_number == 0x1fU) {
        tag_number = 0;
        bool complete = false;
        for (size_t index = 0; index < 10 && *cursor < size; ++index) {
            const uint8_t byte = bytes[(*cursor)++];
            if ((index == 0 && (byte & 0x7fU) == 0) ||
                tag_number > (UINT64_MAX >> 7)) {
                return false;
            }
            tag_number = (tag_number << 7) | (byte & 0x7fU);
            if ((byte & 0x80U) == 0) {
                complete = true;
                break;
            }
        }
        if (!complete) {
            return false;
        }
    }
    size_t value_size = 0;
    if (!boot_image_read_length(bytes, size, cursor, &value_size) ||
        value_size > size - *cursor) {
        return false;
    }
    size_t end = 0;
    if (!boot_image_add_size(*cursor, value_size, &end)) {
        return false;
    }
    result->tag = tag;
    result->tag_number = tag_number;
    result->encoded = bytes + start;
    result->encoded_size = end - start;
    result->value = bytes + *cursor;
    result->value_size = value_size;
    *cursor = end;
    return true;
}

static bool boot_image_parse_boolean_property(
    const BootImageTLV *property,
    uint32_t key,
    bool *value
) {
    if (property->tag != DER_TAG_PRIVATE_CONSTRUCTED_LONG ||
        property->tag_number != key) {
        return false;
    }
    size_t cursor = 0;
    BootImageTLV sequence;
    BootImageTLV encoded_key;
    BootImageTLV encoded_value;
    if (!boot_image_read_tlv(property->value, property->value_size, &cursor,
                             &sequence) ||
        cursor != property->value_size || sequence.tag != DER_TAG_SEQUENCE) {
        return false;
    }
    cursor = 0;
    if (!boot_image_read_tlv(sequence.value, sequence.value_size, &cursor,
                             &encoded_key) ||
        !boot_image_read_tlv(sequence.value, sequence.value_size, &cursor,
                             &encoded_value) ||
        cursor != sequence.value_size) {
        return false;
    }
    const uint8_t expected_key[4] = {
        (uint8_t)(key >> 24), (uint8_t)(key >> 16),
        (uint8_t)(key >> 8), (uint8_t)key,
    };
    if (encoded_key.tag != DER_TAG_IA5_STRING ||
        encoded_key.value_size != sizeof(expected_key) ||
        !boot_image_bytes_equal(encoded_key.value, expected_key,
                                sizeof(expected_key)) ||
        encoded_value.tag != DER_TAG_BOOLEAN || encoded_value.value_size != 1 ||
        (encoded_value.value[0] != 0 && encoded_value.value[0] != UINT8_MAX)) {
        return false;
    }
    *value = encoded_value.value[0] != 0;
    return true;
}

static bool boot_image_parse_payload_policy(
    const BootImageTLV *encoded,
    AVPBootImageView *result
) {
    if (encoded->tag != DER_TAG_CONTEXT_0) {
        return true;
    }
    size_t cursor = 0;
    BootImageTLV sequence;
    if (!boot_image_read_tlv(encoded->value, encoded->value_size, &cursor,
                             &sequence) ||
        cursor != encoded->value_size || sequence.tag != DER_TAG_SEQUENCE) {
        return false;
    }
    cursor = 0;
    BootImageTLV magic;
    BootImageTLV properties;
    if (!boot_image_read_tlv(sequence.value, sequence.value_size, &cursor,
                             &magic) ||
        !boot_image_string(&magic, "PAYP", 4) ||
        !boot_image_read_tlv(sequence.value, sequence.value_size, &cursor,
                             &properties) ||
        cursor != sequence.value_size || properties.tag != DER_TAG_SET) {
        return false;
    }
    cursor = 0;
    while (cursor < properties.value_size) {
        BootImageTLV property;
        if (!boot_image_read_tlv(properties.value, properties.value_size,
                                 &cursor, &property)) {
            return false;
        }
        bool value = false;
        if (property.tag == DER_TAG_PRIVATE_CONSTRUCTED_LONG &&
            property.tag_number == BOOT_IMAGE_PROPERTY_DPRO) {
            if (!boot_image_parse_boolean_property(
                    &property, BOOT_IMAGE_PROPERTY_DPRO, &value)) {
                return false;
            }
            result->payload_dpro = value;
        } else if (property.tag == DER_TAG_PRIVATE_CONSTRUCTED_LONG &&
                   property.tag_number == BOOT_IMAGE_PROPERTY_MANM) {
            if (!boot_image_parse_boolean_property(
                    &property, BOOT_IMAGE_PROPERTY_MANM, &value)) {
                return false;
            }
            result->payload_manm = value;
        }
    }
    return true;
}

static bool boot_image_bytes_equal(
    const uint8_t *left,
    const uint8_t *right,
    size_t size
) {
    for (size_t index = 0; index < size; ++index) {
        if (left[index] != right[index]) {
            return false;
        }
    }
    return true;
}

static bool boot_image_string(
    const BootImageTLV *tlv,
    const char *text,
    size_t text_size
) {
    return tlv->tag == DER_TAG_IA5_STRING &&
        tlv->value_size == text_size &&
        boot_image_bytes_equal(
            tlv->value,
            (const uint8_t *)text,
            text_size
        );
}

static uint32_t boot_image_fourcc(const BootImageTLV *tlv) {
    return ((uint32_t)tlv->value[0] << 24) |
        ((uint32_t)tlv->value[1] << 16) |
        ((uint32_t)tlv->value[2] << 8) |
        (uint32_t)tlv->value[3];
}

static AVPBootImageStatus boot_image_parse_im4p(
    const BootImageTLV *encoded_im4p,
    AVPBootImageView *result
) {
    if (encoded_im4p->tag != DER_TAG_SEQUENCE) {
        return AVP_BOOT_IMAGE_INVALID_IMG4;
    }
    size_t cursor = 0;
    BootImageTLV magic;
    BootImageTLV image_type;
    BootImageTLV description;
    BootImageTLV payload;
    if (!boot_image_read_tlv(
            encoded_im4p->value,
            encoded_im4p->value_size,
            &cursor,
            &magic
        ) ||
        !boot_image_string(&magic, "IM4P", 4) ||
        !boot_image_read_tlv(
            encoded_im4p->value,
            encoded_im4p->value_size,
            &cursor,
            &image_type
        ) ||
        image_type.tag != DER_TAG_IA5_STRING ||
        image_type.value_size != 4 ||
        !boot_image_read_tlv(
            encoded_im4p->value,
            encoded_im4p->value_size,
            &cursor,
            &description
        ) ||
        description.tag != DER_TAG_IA5_STRING ||
        !boot_image_read_tlv(
            encoded_im4p->value,
            encoded_im4p->value_size,
            &cursor,
            &payload
        ) ||
        payload.tag != DER_TAG_OCTET_STRING) {
        return AVP_BOOT_IMAGE_INVALID_IMG4;
    }

    /* Optional keybag, compression, and payload-property fields stay encoded. */
    while (cursor < encoded_im4p->value_size) {
        BootImageTLV optional;
        if (!boot_image_read_tlv(
                encoded_im4p->value,
                encoded_im4p->value_size,
                &cursor,
                &optional
            ) || !boot_image_parse_payload_policy(&optional, result)) {
            return AVP_BOOT_IMAGE_INVALID_IMG4;
        }
    }

    result->image_type = boot_image_fourcc(&image_type);
    result->description_bytes = description.value;
    result->description_size = description.value_size;
    result->payload_bytes = payload.value;
    result->payload_size = payload.value_size;
    return AVP_BOOT_IMAGE_OK;
}

static bool boot_image_is_manifest(const BootImageTLV *tlv) {
    if (tlv->tag != DER_TAG_SEQUENCE) {
        return false;
    }
    size_t cursor = 0;
    BootImageTLV magic;
    return boot_image_read_tlv(
            tlv->value,
            tlv->value_size,
            &cursor,
            &magic
        ) && boot_image_string(&magic, "IM4M", 4);
}

static bool boot_image_unwrap_manifest(
    const BootImageTLV *tlv,
    const uint8_t **encoded,
    size_t *encoded_size
) {
    if (tlv->tag == DER_TAG_CONTEXT_0) {
        size_t cursor = 0;
        BootImageTLV wrapped;
        return boot_image_read_tlv(tlv->value, tlv->value_size, &cursor,
                                   &wrapped) &&
               cursor == tlv->value_size &&
               boot_image_unwrap_manifest(&wrapped, encoded, encoded_size);
    }
    if (!boot_image_is_manifest(tlv)) {
        return false;
    }
    *encoded = tlv->encoded;
    *encoded_size = tlv->encoded_size;
    return true;
}

static AVPBootImageStatus boot_image_parse_img4(
    const uint8_t *bytes,
    size_t size,
    AVPBootImageView *result
) {
    size_t cursor = 0;
    BootImageTLV outer;
    if (!boot_image_read_tlv(bytes, size, &cursor, &outer) ||
        outer.tag != DER_TAG_SEQUENCE || cursor != size) {
        return AVP_BOOT_IMAGE_INVALID_DER;
    }

    size_t content_cursor = 0;
    BootImageTLV magic;
    if (!boot_image_read_tlv(
            outer.value,
            outer.value_size,
            &content_cursor,
            &magic
        )) {
        return AVP_BOOT_IMAGE_INVALID_IMG4;
    }

    BootImageTLV im4p;
    if (boot_image_string(&magic, "IMG4", 4)) {
        if (!boot_image_read_tlv(
                outer.value,
                outer.value_size,
                &content_cursor,
                &im4p
            )) {
            return AVP_BOOT_IMAGE_INVALID_IMG4;
        }
        const AVPBootImageStatus status = boot_image_parse_im4p(
            &im4p,
            result
        );
        if (status != AVP_BOOT_IMAGE_OK) {
            return status;
        }
        while (content_cursor < outer.value_size) {
            BootImageTLV optional;
            if (!boot_image_read_tlv(
                    outer.value,
                    outer.value_size,
                    &content_cursor,
                    &optional
                )) {
                return AVP_BOOT_IMAGE_INVALID_IMG4;
            }
            if (result->manifest_bytes == 0 &&
                boot_image_unwrap_manifest(
                    &optional, &result->encoded_manifest_bytes,
                    &result->encoded_manifest_size)) {
                result->manifest_bytes = optional.encoded;
                result->manifest_size = optional.encoded_size;
            }
        }
        return AVP_BOOT_IMAGE_OK;
    }

    if (boot_image_string(&magic, "IM4P", 4)) {
        return boot_image_parse_im4p(&outer, result);
    }
    return AVP_BOOT_IMAGE_INVALID_IMG4;
}

static bool boot_image_type_is_accepted(
    const AVPBootImagePolicy *policy,
    uint32_t image_type
) {
    if (policy->accepted_image_type_count == 0) {
        return true;
    }
    if (policy->accepted_image_types == 0) {
        return false;
    }
    for (size_t index = 0;
         index < policy->accepted_image_type_count;
         ++index) {
        if (policy->accepted_image_types[index] == image_type) {
            return true;
        }
    }
    return false;
}

AVPBootImageStatus avp_boot_image_measure_der(
    const uint8_t *prefix,
    size_t prefix_size,
    size_t *encoded_size
) {
    if (prefix == 0 || encoded_size == 0 || prefix_size < 2) {
        return AVP_BOOT_IMAGE_INVALID_ARGUMENT;
    }
    if (prefix[0] != DER_TAG_SEQUENCE) {
        return AVP_BOOT_IMAGE_INVALID_DER;
    }
    size_t cursor = 1;
    size_t value_size = 0;
    if (!boot_image_read_length(
            prefix,
            prefix_size,
            &cursor,
            &value_size
        ) ||
        !boot_image_add_size(cursor, value_size, encoded_size)) {
        return AVP_BOOT_IMAGE_INVALID_DER;
    }
    return AVP_BOOT_IMAGE_OK;
}

AVPBootImageStatus avp_boot_image_evaluate(
    const AVPBootImageDescriptor *descriptor,
    const AVPBootImagePolicy *policy,
    AVPBootImageView *result
) {
    if (descriptor == 0 || policy == 0 || result == 0 ||
        descriptor->data.current == 0 || descriptor->data.type == 0 ||
        descriptor->file_size == 0) {
        return AVP_BOOT_IMAGE_INVALID_ARGUMENT;
    }
    const uintptr_t current = (uintptr_t)descriptor->data.current;
    const uintptr_t lower = (uintptr_t)descriptor->data.lower_bound;
    const uintptr_t upper = (uintptr_t)descriptor->data.upper_bound;
    if (current < lower || current > upper ||
        descriptor->file_size > upper - current) {
        return AVP_BOOT_IMAGE_INVALID_BOUNDS;
    }

    *result = (AVPBootImageView){
        .container = descriptor->container,
        .flags = descriptor->flags,
        .declared_image_size = descriptor->image_size,
        .container_bytes = descriptor->data.current,
        .container_size = descriptor->file_size,
    };

    AVPBootImageStatus status;
    if (descriptor->container == AVP_BOOT_IMAGE_CONTAINER_IMG4) {
        status = boot_image_parse_img4(
            result->container_bytes,
            result->container_size,
            result
        );
    } else if (descriptor->container == AVP_BOOT_IMAGE_CONTAINER_MEMZ) {
        if (policy->manifestless_image_type == 0) {
            return AVP_BOOT_IMAGE_TYPE_REJECTED;
        }
        result->image_type = policy->manifestless_image_type;
        result->payload_bytes = result->container_bytes;
        result->payload_size = result->container_size;
        status = AVP_BOOT_IMAGE_OK;
    } else {
        return AVP_BOOT_IMAGE_INVALID_CONTAINER;
    }
    if (status != AVP_BOOT_IMAGE_OK) {
        return status;
    }
    if (!boot_image_type_is_accepted(policy, result->image_type)) {
        return AVP_BOOT_IMAGE_TYPE_REJECTED;
    }
    if (policy->require_manifest && result->manifest_bytes == 0) {
        return AVP_BOOT_IMAGE_MANIFEST_REQUIRED;
    }
    if (policy->evaluate == 0) {
        return AVP_BOOT_IMAGE_OK;
    }
    return policy->evaluate(result, policy->context);
}
