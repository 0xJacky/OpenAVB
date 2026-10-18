#include "avp/image/boot/boot_image.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


typedef struct {
    uint8_t bytes[1024];
    size_t size;
} TestBuffer;

static const RuntimeFirebloomType byte_type = {
    .identity = 1,
    .element_size = 1,
    .bytewise_copyable = true,
};

static void append_bytes(TestBuffer *buffer, const void *bytes, size_t size) {
    assert(size <= sizeof(buffer->bytes) - buffer->size);
    memcpy(buffer->bytes + buffer->size, bytes, size);
    buffer->size += size;
}

static void append_length(TestBuffer *buffer, size_t size) {
    if (size < 0x80) {
        const uint8_t encoded = (uint8_t)size;
        append_bytes(buffer, &encoded, 1);
        return;
    }
    uint8_t encoded[sizeof(size_t) + 1];
    size_t count = 0;
    size_t remaining = size;
    while (remaining != 0) {
        encoded[sizeof(encoded) - 1 - count] = (uint8_t)remaining;
        remaining >>= 8;
        ++count;
    }
    encoded[sizeof(encoded) - 1 - count] = (uint8_t)(0x80 | count);
    append_bytes(
        buffer,
        encoded + sizeof(encoded) - 1 - count,
        count + 1
    );
}

static TestBuffer encode_tlv(uint8_t tag, const void *bytes, size_t size) {
    TestBuffer result = {0};
    append_bytes(&result, &tag, 1);
    append_length(&result, size);
    append_bytes(&result, bytes, size);
    return result;
}

static TestBuffer encode_string(const char *text, size_t size) {
    return encode_tlv(0x16, text, size);
}

static TestBuffer encode_sequence(const TestBuffer *content) {
    return encode_tlv(0x30, content->bytes, content->size);
}

static TestBuffer encode_boolean_property(const char key[4], bool value) {
    TestBuffer fields = {0};
    TestBuffer encoded = encode_string(key, 4);
    append_bytes(&fields, encoded.bytes, encoded.size);
    const uint8_t boolean = value ? UINT8_MAX : 0;
    encoded = encode_tlv(0x01, &boolean, sizeof(boolean));
    append_bytes(&fields, encoded.bytes, encoded.size);
    const TestBuffer sequence = encode_sequence(&fields);

    uint32_t tag_number = AVP_BOOT_IMAGE_FOURCC(key[0], key[1], key[2], key[3]);
    uint8_t tag[6] = {0xff};
    for (size_t index = 0; index < 5; ++index) {
        const unsigned shift = (unsigned)((4 - index) * 7);
        tag[index + 1] = (uint8_t)((tag_number >> shift) & 0x7fU);
        if (index != 4) {
            tag[index + 1] |= 0x80U;
        }
    }
    TestBuffer result = {0};
    append_bytes(&result, tag, sizeof(tag));
    append_length(&result, sequence.size);
    append_bytes(&result, sequence.bytes, sequence.size);
    return result;
}

static TestBuffer build_payload_properties(bool dpro, bool manm) {
    TestBuffer property_set = {0};
    TestBuffer encoded = encode_boolean_property("DPRO", dpro);
    append_bytes(&property_set, encoded.bytes, encoded.size);
    encoded = encode_boolean_property("MANM", manm);
    append_bytes(&property_set, encoded.bytes, encoded.size);
    const TestBuffer set = encode_tlv(
        0x31,
        property_set.bytes,
        property_set.size
    );
    TestBuffer fields = {0};
    encoded = encode_string("PAYP", 4);
    append_bytes(&fields, encoded.bytes, encoded.size);
    append_bytes(&fields, set.bytes, set.size);
    const TestBuffer sequence = encode_sequence(&fields);
    return encode_tlv(0xa0, sequence.bytes, sequence.size);
}

static TestBuffer build_im4p(const char type[4], size_t payload_size) {
    TestBuffer content = {0};
    TestBuffer field = encode_string("IM4P", 4);
    append_bytes(&content, field.bytes, field.size);
    field = encode_string(type, 4);
    append_bytes(&content, field.bytes, field.size);
    field = encode_string("OpenBootROM test image", 22);
    append_bytes(&content, field.bytes, field.size);
    uint8_t payload[256];
    assert(payload_size <= sizeof(payload));
    for (size_t index = 0; index < payload_size; ++index) {
        payload[index] = (uint8_t)(index ^ 0xa5U);
    }
    field = encode_tlv(0x04, payload, payload_size);
    append_bytes(&content, field.bytes, field.size);

    const uint8_t optional_keybag[] = {0x01, 0x02, 0x03};
    field = encode_tlv(0x04, optional_keybag, sizeof(optional_keybag));
    append_bytes(&content, field.bytes, field.size);
    return encode_sequence(&content);
}

static TestBuffer build_im4p_with_policy(
    const char type[4],
    bool dpro,
    bool manm
) {
    TestBuffer content = {0};
    TestBuffer field = encode_string("IM4P", 4);
    append_bytes(&content, field.bytes, field.size);
    field = encode_string(type, 4);
    append_bytes(&content, field.bytes, field.size);
    field = encode_string("policy", 6);
    append_bytes(&content, field.bytes, field.size);
    const uint8_t payload[] = {1, 2, 3};
    field = encode_tlv(0x04, payload, sizeof(payload));
    append_bytes(&content, field.bytes, field.size);
    field = build_payload_properties(dpro, manm);
    append_bytes(&content, field.bytes, field.size);
    return encode_sequence(&content);
}

static TestBuffer build_img4(const char type[4], size_t payload_size) {
    TestBuffer content = {0};
    TestBuffer field = encode_string("IMG4", 4);
    append_bytes(&content, field.bytes, field.size);
    field = build_im4p(type, payload_size);
    append_bytes(&content, field.bytes, field.size);

    TestBuffer manifest_content = {0};
    field = encode_string("IM4M", 4);
    append_bytes(&manifest_content, field.bytes, field.size);
    field = encode_sequence(&manifest_content);
    field = encode_tlv(0xa0, field.bytes, field.size);
    append_bytes(&content, field.bytes, field.size);
    return encode_sequence(&content);
}

static AVPBootImageDescriptor descriptor_for(
    TestBuffer *buffer,
    uint32_t container
) {
    return (AVPBootImageDescriptor){
        .file_size = (uint32_t)buffer->size,
        .image_size = 0x1234,
        .container = container,
        .flags = 0xc,
        .data = {
            .current = buffer->bytes,
            .lower_bound = buffer->bytes,
            .upper_bound = buffer->bytes + buffer->size,
            .type = &byte_type,
        },
    };
}

static AVPBootImageStatus count_policy(
    const AVPBootImageView *image,
    void *context
) {
    size_t *calls = context;
    ++*calls;
    return image->flags == 0xc
        ? AVP_BOOT_IMAGE_OK
        : AVP_BOOT_IMAGE_POLICY_REJECTED;
}

static void test_img4_evaluation(void) {
    TestBuffer image = build_img4("ibss", 160);
    AVPBootImageDescriptor descriptor = descriptor_for(
        &image,
        AVP_BOOT_IMAGE_CONTAINER_IMG4
    );
    const uint32_t accepted[] = {
        AVP_BOOT_IMAGE_FOURCC('i', 'b', 's', 's'),
        AVP_BOOT_IMAGE_FOURCC('i', 'b', 'e', 'c'),
    };
    size_t callback_calls = 0;
    const AVPBootImagePolicy policy = {
        .accepted_image_types = accepted,
        .accepted_image_type_count = 2,
        .manifestless_image_type = 0,
        .require_manifest = true,
        .evaluate = count_policy,
        .context = &callback_calls,
    };
    AVPBootImageView result;
    const AVPBootImageStatus status = avp_boot_image_evaluate(
        &descriptor,
        &policy,
        &result
    );
    assert(status == AVP_BOOT_IMAGE_OK);
    assert(callback_calls == 1);
    assert(result.container == AVP_BOOT_IMAGE_CONTAINER_IMG4);
    assert(result.image_type == accepted[0]);
    assert(result.flags == 0xc);
    assert(result.declared_image_size == 0x1234);
    assert(result.payload_size == 160);
    assert(result.description_size == 22);
    assert(memcmp(
        result.description_bytes,
        "OpenBootROM test image",
        result.description_size
    ) == 0);
    assert(result.payload_bytes[0] == 0xa5);
    assert(result.payload_bytes[159] == (uint8_t)(159 ^ 0xa5));
    assert(result.manifest_bytes != NULL);
    assert(result.manifest_size >= 8);
    assert(result.manifest_bytes[0] == 0xa0);
    assert(result.encoded_manifest_bytes != NULL);
    assert(result.encoded_manifest_size + 2 == result.manifest_size);
    assert(result.encoded_manifest_bytes[0] == 0x30);

    size_t measured_size = 0;
    assert(avp_boot_image_measure_der(
        image.bytes,
        image.size,
        &measured_size
    ) == AVP_BOOT_IMAGE_OK);
    assert(measured_size == image.size);
}

static void test_payload_policy_metadata(void) {
    TestBuffer image = build_im4p_with_policy("illb", true, true);
    AVPBootImageDescriptor descriptor = descriptor_for(
        &image,
        AVP_BOOT_IMAGE_CONTAINER_IMG4
    );
    const uint32_t type = AVP_BOOT_IMAGE_FOURCC('i', 'l', 'l', 'b');
    const AVPBootImagePolicy policy = {
        .accepted_image_types = &type,
        .accepted_image_type_count = 1,
    };
    AVPBootImageView result;
    assert(avp_boot_image_evaluate(&descriptor, &policy, &result) ==
           AVP_BOOT_IMAGE_OK);
    assert(result.payload_dpro);
    assert(result.payload_manm);

    image = build_im4p_with_policy("illb", false, false);
    descriptor = descriptor_for(&image, AVP_BOOT_IMAGE_CONTAINER_IMG4);
    assert(avp_boot_image_evaluate(&descriptor, &policy, &result) ==
           AVP_BOOT_IMAGE_OK);
    assert(!result.payload_dpro);
    assert(!result.payload_manm);
}

static void test_standalone_im4p(void) {
    TestBuffer image = build_im4p("ibec", 12);
    AVPBootImageDescriptor descriptor = descriptor_for(
        &image,
        AVP_BOOT_IMAGE_CONTAINER_IMG4
    );
    const uint32_t type = AVP_BOOT_IMAGE_FOURCC('i', 'b', 'e', 'c');
    AVPBootImagePolicy policy = {
        .accepted_image_types = &type,
        .accepted_image_type_count = 1,
    };
    AVPBootImageView result;
    const AVPBootImageStatus status = avp_boot_image_evaluate(
        &descriptor,
        &policy,
        &result
    );
    assert(status == AVP_BOOT_IMAGE_OK);
    assert(result.image_type == type);
    assert(result.payload_size == 12);
    assert(result.manifest_bytes == NULL);

    policy.require_manifest = true;
    assert(avp_boot_image_evaluate(
        &descriptor,
        &policy,
        &result
    ) == AVP_BOOT_IMAGE_MANIFEST_REQUIRED);
}

static void test_memz_evaluation(void) {
    TestBuffer image = {0};
    const uint8_t raw[] = {0x7f, 'E', 'L', 'F', 1, 2, 3, 4};
    append_bytes(&image, raw, sizeof(raw));
    AVPBootImageDescriptor descriptor = descriptor_for(
        &image,
        AVP_BOOT_IMAGE_CONTAINER_MEMZ
    );
    const uint32_t type = AVP_BOOT_IMAGE_FOURCC('r', 'a', 'w', ' ');
    const AVPBootImagePolicy policy = {
        .accepted_image_types = &type,
        .accepted_image_type_count = 1,
        .manifestless_image_type = type,
    };
    AVPBootImageView result;
    assert(avp_boot_image_evaluate(
        &descriptor,
        &policy,
        &result
    ) == AVP_BOOT_IMAGE_OK);
    assert(result.container == AVP_BOOT_IMAGE_CONTAINER_MEMZ);
    assert(result.image_type == type);
    assert(result.payload_bytes == image.bytes);
    assert(result.payload_size == sizeof(raw));
}

static void test_rejections(void) {
    TestBuffer image = build_img4("ibss", 16);
    AVPBootImageDescriptor descriptor = descriptor_for(
        &image,
        AVP_BOOT_IMAGE_CONTAINER_IMG4
    );
    const uint32_t rejected = AVP_BOOT_IMAGE_FOURCC('i', 'b', 'e', 'c');
    AVPBootImagePolicy policy = {
        .accepted_image_types = &rejected,
        .accepted_image_type_count = 1,
    };
    AVPBootImageView result;
    assert(avp_boot_image_evaluate(
        &descriptor,
        &policy,
        &result
    ) == AVP_BOOT_IMAGE_TYPE_REJECTED);

    descriptor.container = 0;
    assert(avp_boot_image_evaluate(
        &descriptor,
        &policy,
        &result
    ) == AVP_BOOT_IMAGE_INVALID_CONTAINER);

    descriptor = descriptor_for(&image, AVP_BOOT_IMAGE_CONTAINER_IMG4);
    descriptor.file_size++;
    assert(avp_boot_image_evaluate(
        &descriptor,
        &policy,
        &result
    ) == AVP_BOOT_IMAGE_INVALID_BOUNDS);

    descriptor = descriptor_for(&image, AVP_BOOT_IMAGE_CONTAINER_IMG4);
    image.bytes[0] = 0x31;
    assert(avp_boot_image_evaluate(
        &descriptor,
        &policy,
        &result
    ) == AVP_BOOT_IMAGE_INVALID_DER);

    const uint8_t non_minimal[] = {0x30, 0x81, 0x01, 0x00};
    size_t measured = 0;
    assert(avp_boot_image_measure_der(
        non_minimal,
        sizeof(non_minimal),
        &measured
    ) == AVP_BOOT_IMAGE_INVALID_DER);
}

static void test_file_image(const char *path, bool require_manifest) {
    FILE *file = fopen(path, "rb");
    assert(file != NULL);
    assert(fseek(file, 0, SEEK_END) == 0);
    const long file_size = ftell(file);
    assert(file_size > 0 && (unsigned long)file_size <= UINT32_MAX);
    assert(fseek(file, 0, SEEK_SET) == 0);
    uint8_t *bytes = malloc((size_t)file_size);
    assert(bytes != NULL);
    assert(fread(bytes, 1, (size_t)file_size, file) == (size_t)file_size);
    assert(fclose(file) == 0);

    AVPBootImageDescriptor descriptor = {
        .file_size = (uint32_t)file_size,
        .image_size = 0,
        .container = AVP_BOOT_IMAGE_CONTAINER_IMG4,
        .data = {
            .current = bytes,
            .lower_bound = bytes,
            .upper_bound = bytes + file_size,
            .type = &byte_type,
        },
    };
    const uint32_t type = AVP_BOOT_IMAGE_FOURCC('i', 'b', 's', 's');
    const AVPBootImagePolicy policy = {
        .accepted_image_types = &type,
        .accepted_image_type_count = 1,
        .require_manifest = require_manifest,
    };
    AVPBootImageView result;
    const AVPBootImageStatus status = avp_boot_image_evaluate(
        &descriptor,
        &policy,
        &result
    );
    if (status != AVP_BOOT_IMAGE_OK) {
        fprintf(stderr, "file boot image status: %u\n", (unsigned)status);
    }
    assert(status == AVP_BOOT_IMAGE_OK);
    assert(result.image_type == type);
    assert(result.payload_size != 0);
    assert((result.manifest_bytes != NULL) == require_manifest);
    free(bytes);
}

int main(int argc, char **argv) {
    test_img4_evaluation();
    test_payload_policy_metadata();
    test_standalone_im4p();
    test_memz_evaluation();
    test_rejections();
    if (argc >= 2) {
        test_file_image(argv[1], argc >= 3);
    }
}
