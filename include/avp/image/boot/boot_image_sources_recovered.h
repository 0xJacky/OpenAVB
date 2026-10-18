#pragma once

#include "avp/image/boot/boot_image.h"
#include "avp/image/img4/img4_context_compat.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(__ELF__) && __has_attribute(retain)
#define BOOT_IMAGE_RECOVERED_PUBLIC \
    __attribute__((used, retain, noinline, section(".text.runtime_support_recovered")))
#elif defined(__ELF__)
#define BOOT_IMAGE_RECOVERED_PUBLIC \
    __attribute__((used, noinline, section(".text.runtime_support_recovered")))
#else
#define BOOT_IMAGE_RECOVERED_PUBLIC __attribute__((used, noinline))
#endif

enum {
    BOOT_IMAGE_RECOVERED_SOURCE_LIMIT = 16,
    BOOT_IMAGE_RECOVERED_PROPERTY_KEY_LIMIT = 4,
};

typedef enum {
    BOOT_IMAGE_RECOVERED_OK = 0,
    BOOT_IMAGE_RECOVERED_INVALID_ARGUMENT = 1,
    BOOT_IMAGE_RECOVERED_BOUNDS = 2,
    BOOT_IMAGE_RECOVERED_IO = 3,
    BOOT_IMAGE_RECOVERED_FORMAT = 4,
    BOOT_IMAGE_RECOVERED_FULL = 5,
    BOOT_IMAGE_RECOVERED_NOT_FOUND = 6,
} BootImageRecoveredStatus;

typedef BootImageRecoveredStatus (*BootImageRecoveredRead)(
    void *context,
    size_t offset,
    void *destination,
    size_t size
);

typedef struct BootImageRecoveredSource {
    uint32_t image_type;
    uint32_t container;
    size_t source_offset;
    size_t source_size;
    const uint8_t *memory;
    uint8_t *scratch;
    size_t scratch_size;
    BootImageRecoveredRead read;
    void *read_context;
    AVPBootImageView view;
    bool loaded;
    bool active;
} BootImageRecoveredSource;

typedef struct {
    BootImageRecoveredSource sources[BOOT_IMAGE_RECOVERED_SOURCE_LIMIT];
    size_t count;
} BootImageRecoveredRegistry;

typedef enum {
    BOOT_IMAGE_RECOVERED_PROPERTY_INTEGER = 1,
    BOOT_IMAGE_RECOVERED_PROPERTY_BYTES = 2,
} BootImageRecoveredPropertyKind;

typedef struct {
    uint32_t key;
    BootImageRecoveredPropertyKind kind;
    const uint8_t *bytes;
    size_t size;
    uint64_t integer;
} BootImageRecoveredProperty;

typedef BootImageRecoveredStatus (*BootImageRecoveredPropertyObserver)(
    void *context,
    const BootImageRecoveredProperty *property
);

void boot_image_recovered_registry_initialize(BootImageRecoveredRegistry *registry);

BOOT_IMAGE_RECOVERED_PUBLIC BootImageRecoveredStatus
boot_image_register_provider_record_from_header(
    BootImageRecoveredRegistry *registry,
    BootImageRecoveredRead read,
    void *read_context,
    size_t source_offset,
    size_t source_size,
    uint8_t *scratch,
    size_t scratch_size,
    BootImageRecoveredSource **result
);
BOOT_IMAGE_RECOVERED_PUBLIC BootImageRecoveredSource *
boot_image_source_lookup_by_type(BootImageRecoveredRegistry *registry,
                                 uint32_t image_type);
BOOT_IMAGE_RECOVERED_PUBLIC BootImageRecoveredStatus
boot_img4_evaluate_image_payload_properties(
    const img4_decode_context_t *image,
    BootImageRecoveredPropertyObserver observer,
    void *observer_context
);
BOOT_IMAGE_RECOVERED_PUBLIC BootImageRecoveredStatus
boot_image_forward_integer_payload_property(
    uint32_t key,
    const uint8_t *integer_bytes,
    size_t integer_size,
    BootImageRecoveredPropertyObserver observer,
    void *observer_context
);
BOOT_IMAGE_RECOVERED_PUBLIC BootImageRecoveredStatus
boot_img4_evaluate_payload_properties(
    const uint8_t *encoded,
    size_t encoded_size,
    BootImageRecoveredPropertyObserver observer,
    void *observer_context
);
BOOT_IMAGE_RECOVERED_PUBLIC BootImageRecoveredStatus
boot_image_property_callback(
    const uint8_t *key,
    size_t key_size,
    uint8_t value_tag,
    const uint8_t *value,
    size_t value_size,
    BootImageRecoveredPropertyObserver observer,
    void *observer_context
);
BOOT_IMAGE_RECOVERED_PUBLIC RuntimeFirebloomSpan
boot_image_resolve_physical_region_slice(
    void *base,
    size_t region_size,
    size_t offset,
    size_t length,
    const RuntimeFirebloomType *type
);
BOOT_IMAGE_RECOVERED_PUBLIC BootImageRecoveredStatus
boot_image_register_provider_source_at_offset(
    BootImageRecoveredRegistry *registry,
    BootImageRecoveredRead read,
    void *read_context,
    size_t source_offset,
    size_t source_size,
    uint8_t *scratch,
    size_t scratch_size,
    BootImageRecoveredSource **result
);
BOOT_IMAGE_RECOVERED_PUBLIC BootImageRecoveredStatus
boot_image_load_source_record(BootImageRecoveredSource *source);
BOOT_IMAGE_RECOVERED_PUBLIC BootImageRecoveredSource *
boot_image_source_lookup_by_type_thunk(BootImageRecoveredRegistry *registry,
                                       uint32_t image_type);
BOOT_IMAGE_RECOVERED_PUBLIC BootImageRecoveredStatus
boot_image_create_memory_source(
    BootImageRecoveredRegistry *registry,
    const uint8_t *memory,
    size_t size,
    uint32_t container,
    uint32_t manifestless_image_type,
    BootImageRecoveredSource **result
);
BOOT_IMAGE_RECOVERED_PUBLIC void
boot_image_release_memory_source(BootImageRecoveredRegistry *registry,
                                 BootImageRecoveredSource *source);
BOOT_IMAGE_RECOVERED_PUBLIC _Noreturn void
boot_image_release_memory_source_bounds_panic(void);
