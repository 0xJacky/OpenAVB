#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "avp/runtime/firebloom/runtime_firebloom.h"


enum {
    RUNTIME_OBJECT_CAPACITY = 512,
    RUNTIME_CAPABILITY_CAPACITY = 1024,
    RUNTIME_CAPABILITY_TYPE_DOMAIN_DEFINITION = 1,
    RUNTIME_CAPABILITY_TYPE_DOMAIN = 2,
    RUNTIME_CAPABILITY_TYPE_TASK = 3,
    RUNTIME_CAPABILITY_TYPE_SERVICE_ENDPOINT = 4,
    RUNTIME_CAPABILITY_TYPE_EVENT = 5,
    RUNTIME_CAPABILITY_TYPE_OBJECT = 6,
    RUNTIME_CAPABILITY_TYPE_MEMORY_MAPPING = 7,
    RUNTIME_CAPABILITY_TYPE_ANY = 0xff,
    RUNTIME_CAPABILITY_RIGHT_WAIT = 1,
    RUNTIME_CAPABILITY_RIGHT_SIGNAL = 2,
    RUNTIME_CAPABILITY_RIGHT_DELEGATE = 4,
    RUNTIME_CAPABILITY_AUDIENCE_ALL = 0xfffe,
};

typedef struct {
    void *object;
    const RuntimeFirebloomType *type;
} RuntimeObjectRecord;

_Static_assert(sizeof(RuntimeObjectRecord) == 0x10, "runtime object size");

extern const RuntimeFirebloomType runtime_memory_mapping_type;

bool runtime_capability_address_is_canonical(uintptr_t address);
void runtime_capability_validate_address(uintptr_t address);
_Noreturn void runtime_capability_address_failure(void);
void runtime_capability_initialize(void);
void runtime_capability_clear_tuple(void *storage);
RuntimeFirebloomSpan runtime_object_registry_span(void);
RuntimeFirebloomSpan runtime_capability_registry_span(void);
uint16_t runtime_capability_audience_for_domain(uint8_t domain);
uint16_t runtime_capability_normalize_audience(uint16_t audience);
uint16_t runtime_object_publish(
    RuntimeFirebloomSpan object,
    size_t minimum_size
);
RuntimeFirebloomSpan runtime_object_resolve(
    uint16_t object_id,
    size_t minimum_size
);
uint16_t runtime_capability_publish_object_id(
    uint8_t type,
    uint16_t audience,
    uint8_t rights,
    uint16_t object_id
);
uint16_t runtime_capability_publish(
    void *object,
    const RuntimeFirebloomType *object_type,
    uint8_t type,
    uint8_t rights,
    uint16_t audience
);
const uint32_t *runtime_capability_entry_lookup(uint16_t handle);
const uint32_t *runtime_capability_require_entry(
    uint16_t handle,
    uint8_t type,
    uint8_t required_rights
);
uint16_t runtime_capability_derive(
    uint16_t source_handle,
    uint8_t rights,
    uint16_t audience
);
void runtime_capability_revoke_rights(uint16_t handle, uint8_t rights);
uint16_t runtime_capability_object_id(uint32_t entry);
uint16_t runtime_capability_object_count(void);
uint16_t runtime_capability_count(void);
RuntimeFirebloomSpan runtime_capability_resolve_span(
    uint16_t handle,
    uint8_t type,
    uint8_t required_rights,
    size_t minimum_size
);
void *runtime_capability_resolve(
    uint16_t handle,
    uint8_t type,
    uint8_t required_rights
);
void *runtime_domain_resolve_capability(
    uint16_t handle,
    uint8_t required_rights
);
RuntimeFirebloomSpan runtime_event_resolve_capability(
    uint16_t handle,
    uint8_t required_rights
);
RuntimeFirebloomSpan runtime_endpoint_resolve_capability(
    uint16_t handle,
    uint8_t required_rights
);
RuntimeFirebloomSpan runtime_type6_resolve_capability(
    uint16_t handle,
    uint8_t required_rights
);
