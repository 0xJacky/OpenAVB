#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "avp/runtime/ipc/runtime_capability.h"


enum {
    RUNTIME_CAPABILITY_ALLOWED_EXTERNAL_AUDIENCE_MASK = 0x001e,
    RUNTIME_TYPE6_OBJECT_SIZE = 0x90,
    RUNTIME_OBJECT_ADDRESS_CLASS_REJECTED = 0,
    RUNTIME_OBJECT_ADDRESS_CLASS_RUNTIME_PRIVATE = 2,
    RUNTIME_OBJECT_ADDRESS_CLASS_PLATFORM = 3,
};

typedef uint8_t (*RuntimeObjectAddressClassifier)(
    void *context,
    uintptr_t address,
    size_t size
);

typedef struct {
    void *context;
    RuntimeObjectAddressClassifier classify;
} RuntimeCapabilityHelpersPlatform;

#define RUNTIME_CAPABILITY_HELPERS_PUBLIC __attribute__((used, retain))

extern const RuntimeFirebloomType runtime_type6_object_type;

void runtime_capability_helpers_bind_platform(
    const RuntimeCapabilityHelpersPlatform *platform
);
void runtime_capability_helpers_reset_external_audience(void);
RUNTIME_CAPABILITY_HELPERS_PUBLIC RuntimeFirebloomSpan
runtime_capability_allowed_external_audience_mask_span(void);
RUNTIME_CAPABILITY_HELPERS_PUBLIC bool
runtime_capability_external_audience_bit_is_enabled(uint8_t bit);
RUNTIME_CAPABILITY_HELPERS_PUBLIC uint16_t runtime_object_publish_32(
    RuntimeFirebloomSpan object
);
RUNTIME_CAPABILITY_HELPERS_PUBLIC uintptr_t
runtime_capability_validate_address_identity(uintptr_t address);
RUNTIME_CAPABILITY_HELPERS_PUBLIC RuntimeFirebloomSpan
runtime_type6_object_tuple_validate(RuntimeFirebloomSpan object);
RUNTIME_CAPABILITY_HELPERS_PUBLIC bool runtime_object_address_is_admissible(
    uintptr_t address,
    size_t size
);
RUNTIME_CAPABILITY_HELPERS_PUBLIC bool
runtime_capability_enable_external_audience(uint8_t bit);
RUNTIME_CAPABILITY_HELPERS_PUBLIC _Noreturn void
runtime_capability_publish_zero_rights_fatal(void);
RUNTIME_CAPABILITY_HELPERS_PUBLIC void *
runtime_type6_resolve_wait_signal_capability_from_x23(uint16_t handle);
RUNTIME_CAPABILITY_HELPERS_PUBLIC bool
runtime_capability_outlined_compare_entry_type_to_7(
    const uint32_t *entry
);
