#pragma once

#include "avp/runtime/firebloom/runtime_firebloom.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


#define AVP_BOOT_IMAGE_FOURCC(a, b, c, d) \
    (((uint32_t)(uint8_t)(a) << 24) | \
     ((uint32_t)(uint8_t)(b) << 16) | \
     ((uint32_t)(uint8_t)(c) << 8) | \
     (uint32_t)(uint8_t)(d))

enum {
    AVP_BOOT_IMAGE_CONTAINER_MEMZ = AVP_BOOT_IMAGE_FOURCC('M', 'e', 'm', 'z'),
    AVP_BOOT_IMAGE_CONTAINER_IMG4 = AVP_BOOT_IMAGE_FOURCC('i', 'm', 'g', '4'),
};

typedef struct {
    uint32_t file_size;
    uint32_t image_size;
    uint32_t container;
    uint32_t flags;
    RuntimeFirebloomSpan data;
} AVPBootImageDescriptor;

_Static_assert(
    sizeof(AVPBootImageDescriptor) == 0x30,
    "AVPBootImageDescriptor must match the recovered boot-image descriptor"
);

typedef enum {
    AVP_BOOT_IMAGE_OK = 0,
    AVP_BOOT_IMAGE_INVALID_ARGUMENT,
    AVP_BOOT_IMAGE_INVALID_BOUNDS,
    AVP_BOOT_IMAGE_INVALID_CONTAINER,
    AVP_BOOT_IMAGE_INVALID_DER,
    AVP_BOOT_IMAGE_INVALID_IMG4,
    AVP_BOOT_IMAGE_TYPE_REJECTED,
    AVP_BOOT_IMAGE_MANIFEST_REQUIRED,
    AVP_BOOT_IMAGE_POLICY_REJECTED,
} AVPBootImageStatus;

typedef struct {
    uint32_t container;
    uint32_t image_type;
    uint32_t flags;
    uint32_t declared_image_size;
    const uint8_t *container_bytes;
    size_t container_size;
    const uint8_t *payload_bytes;
    size_t payload_size;
    const uint8_t *description_bytes;
    size_t description_size;
    const uint8_t *manifest_bytes;
    size_t manifest_size;
    const uint8_t *encoded_manifest_bytes;
    size_t encoded_manifest_size;
    bool payload_dpro;
    bool payload_manm;
} AVPBootImageView;

typedef AVPBootImageStatus (*AVPBootImagePolicyCallback)(
    const AVPBootImageView *image,
    void *context
);

typedef struct {
    const uint32_t *accepted_image_types;
    size_t accepted_image_type_count;
    uint32_t manifestless_image_type;
    bool require_manifest;
    AVPBootImagePolicyCallback evaluate;
    void *context;
    const uint8_t *expected_manifest_digest;
    size_t expected_manifest_digest_size;
} AVPBootImagePolicy;

AVPBootImageStatus avp_boot_image_measure_der(
    const uint8_t *prefix,
    size_t prefix_size,
    size_t *encoded_size
);
AVPBootImageStatus avp_boot_image_evaluate(
    const AVPBootImageDescriptor *descriptor,
    const AVPBootImagePolicy *policy,
    AVPBootImageView *result
);
