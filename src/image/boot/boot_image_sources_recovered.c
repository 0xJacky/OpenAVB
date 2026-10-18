#include "avp/image/boot/boot_image_sources_recovered.h"

#include <string.h>

enum {
    DER_TAG_INTEGER = 0x02,
    DER_TAG_OCTET_STRING = 0x04,
    DER_TAG_IA5_STRING = 0x16,
};

static BootImageRecoveredSource *boot_image_recovered_allocate(
    BootImageRecoveredRegistry *registry
) {
    if (registry == NULL) {
        return NULL;
    }
    for (size_t index = 0; index < BOOT_IMAGE_RECOVERED_SOURCE_LIMIT; ++index) {
        if (!registry->sources[index].active) {
            BootImageRecoveredSource *source = &registry->sources[index];
            *source = (BootImageRecoveredSource){0};
            source->active = true;
            ++registry->count;
            return source;
        }
    }
    return NULL;
}

static BootImageRecoveredStatus boot_image_recovered_evaluate(
    BootImageRecoveredSource *source,
    const uint8_t *memory,
    size_t size,
    uint32_t container,
    uint32_t manifestless_image_type
) {
    AVPBootImageDescriptor descriptor = {
        .file_size = (uint32_t)size,
        .image_size = (uint32_t)size,
        .container = container,
        .data = {
            .current = (void *)memory,
            .lower_bound = (void *)memory,
            .upper_bound = (void *)(memory + size),
            .type = &(const RuntimeFirebloomType){
                .identity = 1u,
                .element_size = 1u,
                .bytewise_copyable = true,
            },
        },
    };
    const AVPBootImagePolicy policy = {
        .manifestless_image_type = manifestless_image_type,
    };
    if (size > UINT32_MAX) {
        return BOOT_IMAGE_RECOVERED_BOUNDS;
    }
    const AVPBootImageStatus status =
        avp_boot_image_evaluate(&descriptor, &policy, &source->view);
    if (status != AVP_BOOT_IMAGE_OK) {
        return BOOT_IMAGE_RECOVERED_FORMAT;
    }
    source->memory = memory;
    source->source_size = size;
    source->container = container;
    source->image_type = source->view.image_type;
    source->loaded = true;
    return BOOT_IMAGE_RECOVERED_OK;
}

void boot_image_recovered_registry_initialize(BootImageRecoveredRegistry *registry) {
    if (registry == NULL) {
        __builtin_trap();
    }
    memset(registry, 0, sizeof(*registry));
}

BootImageRecoveredStatus boot_image_register_provider_source_at_offset(
    BootImageRecoveredRegistry *registry,
    BootImageRecoveredRead read,
    void *read_context,
    size_t source_offset,
    size_t source_size,
    uint8_t *scratch,
    size_t scratch_size,
    BootImageRecoveredSource **result
) {
    if (registry == NULL || read == NULL || scratch == NULL || result == NULL ||
        source_size == 0u || scratch_size < source_size) {
        return BOOT_IMAGE_RECOVERED_INVALID_ARGUMENT;
    }
    BootImageRecoveredSource *source = boot_image_recovered_allocate(registry);
    if (source == NULL) {
        return BOOT_IMAGE_RECOVERED_FULL;
    }
    source->read = read;
    source->read_context = read_context;
    source->source_offset = source_offset;
    source->source_size = source_size;
    source->scratch = scratch;
    source->scratch_size = scratch_size;
    *result = source;
    return BOOT_IMAGE_RECOVERED_OK;
}

BootImageRecoveredStatus boot_image_load_source_record(
    BootImageRecoveredSource *source
) {
    if (source == NULL || !source->active || source->read == NULL ||
        source->scratch == NULL || source->scratch_size < source->source_size) {
        return BOOT_IMAGE_RECOVERED_INVALID_ARGUMENT;
    }
    if (source->read(source->read_context, source->source_offset,
                     source->scratch, source->source_size) !=
        BOOT_IMAGE_RECOVERED_OK) {
        return BOOT_IMAGE_RECOVERED_IO;
    }
    return boot_image_recovered_evaluate(
        source,
        source->scratch,
        source->source_size,
        AVP_BOOT_IMAGE_CONTAINER_IMG4,
        0u
    );
}

BootImageRecoveredStatus boot_image_register_provider_record_from_header(
    BootImageRecoveredRegistry *registry,
    BootImageRecoveredRead read,
    void *read_context,
    size_t source_offset,
    size_t source_size,
    uint8_t *scratch,
    size_t scratch_size,
    BootImageRecoveredSource **result
) {
    if (read == NULL || scratch == NULL || source_size < 2u ||
        scratch_size < source_size) {
        return BOOT_IMAGE_RECOVERED_INVALID_ARGUMENT;
    }
    const size_t prefix_size = source_size < 30u ? source_size : 30u;
    if (read(read_context, source_offset, scratch, prefix_size) !=
        BOOT_IMAGE_RECOVERED_OK) {
        return BOOT_IMAGE_RECOVERED_IO;
    }
    size_t encoded_size = 0;
    if (avp_boot_image_measure_der(scratch, prefix_size, &encoded_size) !=
            AVP_BOOT_IMAGE_OK ||
        encoded_size > source_size) {
        return BOOT_IMAGE_RECOVERED_FORMAT;
    }
    BootImageRecoveredStatus status = boot_image_register_provider_source_at_offset(
        registry,
        read,
        read_context,
        source_offset,
        encoded_size,
        scratch,
        scratch_size,
        result
    );
    if (status != BOOT_IMAGE_RECOVERED_OK) {
        return status;
    }
    status = boot_image_load_source_record(*result);
    if (status != BOOT_IMAGE_RECOVERED_OK) {
        boot_image_release_memory_source(registry, *result);
        *result = NULL;
    }
    return status;
}

BootImageRecoveredSource *boot_image_source_lookup_by_type(
    BootImageRecoveredRegistry *registry,
    uint32_t image_type
) {
    if (registry == NULL) {
        return NULL;
    }
    for (size_t index = 0; index < BOOT_IMAGE_RECOVERED_SOURCE_LIMIT; ++index) {
        BootImageRecoveredSource *source = &registry->sources[index];
        if (source->active && source->loaded && source->image_type == image_type) {
            return source;
        }
    }
    return NULL;
}

BootImageRecoveredSource *boot_image_source_lookup_by_type_thunk(
    BootImageRecoveredRegistry *registry,
    uint32_t image_type
) {
    return boot_image_source_lookup_by_type(registry, image_type);
}

BootImageRecoveredStatus boot_image_create_memory_source(
    BootImageRecoveredRegistry *registry,
    const uint8_t *memory,
    size_t size,
    uint32_t container,
    uint32_t manifestless_image_type,
    BootImageRecoveredSource **result
) {
    if (registry == NULL || memory == NULL || result == NULL || size == 0u) {
        return BOOT_IMAGE_RECOVERED_INVALID_ARGUMENT;
    }
    BootImageRecoveredSource *source = boot_image_recovered_allocate(registry);
    if (source == NULL) {
        return BOOT_IMAGE_RECOVERED_FULL;
    }
    BootImageRecoveredStatus status = boot_image_recovered_evaluate(
        source,
        memory,
        size,
        container,
        manifestless_image_type
    );
    if (status != BOOT_IMAGE_RECOVERED_OK) {
        boot_image_release_memory_source(registry, source);
        return status;
    }
    *result = source;
    return BOOT_IMAGE_RECOVERED_OK;
}

void boot_image_release_memory_source(
    BootImageRecoveredRegistry *registry,
    BootImageRecoveredSource *source
) {
    if (registry == NULL || source == NULL ||
        source < &registry->sources[0] ||
        source >= &registry->sources[BOOT_IMAGE_RECOVERED_SOURCE_LIMIT] ||
        !source->active) {
        boot_image_release_memory_source_bounds_panic();
    }
    *source = (BootImageRecoveredSource){0};
    --registry->count;
}

_Noreturn void boot_image_release_memory_source_bounds_panic(void) {
    __builtin_trap();
}

RuntimeFirebloomSpan boot_image_resolve_physical_region_slice(
    void *base,
    size_t region_size,
    size_t offset,
    size_t length,
    const RuntimeFirebloomType *type
) {
    if (base == NULL || type == NULL || offset > region_size ||
        length > region_size - offset) {
        __builtin_trap();
    }
    uint8_t *lower = base;
    return (RuntimeFirebloomSpan){
        .current = lower + offset,
        .lower_bound = lower,
        .upper_bound = lower + region_size,
        .type = type,
    };
}

BootImageRecoveredStatus boot_image_forward_integer_payload_property(
    uint32_t key,
    const uint8_t *integer_bytes,
    size_t integer_size,
    BootImageRecoveredPropertyObserver observer,
    void *observer_context
) {
    if (integer_bytes == NULL || observer == NULL || integer_size == 0u ||
        integer_size > sizeof(uint64_t) || (integer_bytes[0] & 0x80u) != 0u) {
        return BOOT_IMAGE_RECOVERED_FORMAT;
    }
    uint64_t integer = 0;
    for (size_t index = 0; index < integer_size; ++index) {
        integer = (integer << 8u) | integer_bytes[index];
    }
    const BootImageRecoveredProperty property = {
        .key = key,
        .kind = BOOT_IMAGE_RECOVERED_PROPERTY_INTEGER,
        .bytes = integer_bytes,
        .size = integer_size,
        .integer = integer,
    };
    return observer(observer_context, &property);
}

BootImageRecoveredStatus boot_image_property_callback(
    const uint8_t *key,
    size_t key_size,
    uint8_t value_tag,
    const uint8_t *value,
    size_t value_size,
    BootImageRecoveredPropertyObserver observer,
    void *observer_context
) {
    if (key == NULL || key_size != BOOT_IMAGE_RECOVERED_PROPERTY_KEY_LIMIT ||
        value == NULL || observer == NULL) {
        return BOOT_IMAGE_RECOVERED_FORMAT;
    }
    const uint32_t property_key =
        ((uint32_t)key[0] << 24u) | ((uint32_t)key[1] << 16u) |
        ((uint32_t)key[2] << 8u) | key[3];
    if (value_tag == DER_TAG_INTEGER) {
        return boot_image_forward_integer_payload_property(
            property_key,
            value,
            value_size,
            observer,
            observer_context
        );
    }
    if (value_tag != DER_TAG_OCTET_STRING) {
        return BOOT_IMAGE_RECOVERED_FORMAT;
    }
    const BootImageRecoveredProperty property = {
        .key = property_key,
        .kind = BOOT_IMAGE_RECOVERED_PROPERTY_BYTES,
        .bytes = value,
        .size = value_size,
    };
    return observer(observer_context, &property);
}

BootImageRecoveredStatus boot_img4_evaluate_payload_properties(
    const uint8_t *encoded,
    size_t encoded_size,
    BootImageRecoveredPropertyObserver observer,
    void *observer_context
) {
    if (encoded == NULL || observer == NULL) {
        return BOOT_IMAGE_RECOVERED_INVALID_ARGUMENT;
    }
    const DERItem encoded_properties = {
        .data = (DERByte *)(uintptr_t)encoded,
        .length = encoded_size,
    };
    Img4DecodedPayloadProperties payload_properties;
    if (DERImg4DecodePayloadProperties(
            &encoded_properties, &payload_properties) != DR_Success) {
        return BOOT_IMAGE_RECOVERED_FORMAT;
    }
    const DERItem dictionary = payload_properties.properties;
    DERDecodedInfo decoded_dictionary;
    if (DERDecodeItem(&dictionary, &decoded_dictionary) != DR_Success ||
        decoded_dictionary.tag != ASN1_CONSTR_SET ||
        decoded_dictionary.content.data + decoded_dictionary.content.length !=
            dictionary.data + dictionary.length) {
        return BOOT_IMAGE_RECOVERED_FORMAT;
    }

    DERSequence properties;
    DERDecodeSeqContentInit(&decoded_dictionary.content, &properties);
    for (;;) {
        DERDecodedInfo wrapped_property;
        const DERReturn next = DERDecodeSeqNext(&properties, &wrapped_property);
        if (next == DR_EndOfSequence) {
            return BOOT_IMAGE_RECOVERED_OK;
        }
        if (next != DR_Success) {
            return BOOT_IMAGE_RECOVERED_FORMAT;
        }

        DERDecodedInfo property_sequence;
        if (DERDecodeItem(&wrapped_property.content, &property_sequence) !=
                DR_Success ||
            property_sequence.tag != ASN1_CONSTR_SEQUENCE ||
            property_sequence.content.data + property_sequence.content.length !=
                wrapped_property.content.data + wrapped_property.content.length) {
            return BOOT_IMAGE_RECOVERED_FORMAT;
        }

        DERSequence fields;
        DERDecodeSeqContentInit(&property_sequence.content, &fields);
        DERDecodedInfo key;
        DERDecodedInfo value;
        DERDecodedInfo trailing;
        if (DERDecodeSeqNext(&fields, &key) != DR_Success ||
            key.tag != DER_TAG_IA5_STRING || key.content.length != 4u ||
            DERDecodeSeqNext(&fields, &value) != DR_Success ||
            DERDecodeSeqNext(&fields, &trailing) != DR_EndOfSequence) {
            return BOOT_IMAGE_RECOVERED_FORMAT;
        }
        const uint32_t key_value =
            ((uint32_t)key.content.data[0] << 24u) |
            ((uint32_t)key.content.data[1] << 16u) |
            ((uint32_t)key.content.data[2] << 8u) |
            key.content.data[3];
        if (wrapped_property.tag != Img4DecodePrivateTag(key_value) ||
            (value.tag != DER_TAG_INTEGER &&
             value.tag != DER_TAG_OCTET_STRING)) {
            return BOOT_IMAGE_RECOVERED_FORMAT;
        }
        const BootImageRecoveredStatus status = boot_image_property_callback(
            key.content.data,
            key.content.length,
            (uint8_t)value.tag,
            value.content.data,
            value.content.length,
            observer,
            observer_context
        );
        if (status != BOOT_IMAGE_RECOVERED_OK) {
            return status;
        }
    }
}

BootImageRecoveredStatus boot_img4_evaluate_image_payload_properties(
    const img4_decode_context_t *image,
    BootImageRecoveredPropertyObserver observer,
    void *observer_context
) {
    if (image == NULL || observer == NULL) {
        return BOOT_IMAGE_RECOVERED_INVALID_ARGUMENT;
    }
    DERItem properties;
    if (Img4DecodeGetPayloadProperties(image, &properties) != DR_Success) {
        return BOOT_IMAGE_RECOVERED_FORMAT;
    }
    return boot_img4_evaluate_payload_properties(
        properties.data,
        properties.length,
        observer,
        observer_context
    );
}
