#include "avp/runtime/ipc/runtime_capability.h"
#include "avp/runtime/support/recovered_symbol.h"
#include "avp/runtime/support/runtime_diagnostics.h"
#include "avp/runtime/support/runtime_state.h"
#include "avp/runtime/ipc/runtime_svc.h"


enum {
    RUNTIME_CAPABILITY_OBJECT_ID_MASK = 0x000003ff,
    RUNTIME_CAPABILITY_AUDIENCE_MASK = 0x01fffc00,
    RUNTIME_CAPABILITY_RIGHTS_MASK = 0x0e000000,
    RUNTIME_CAPABILITY_TYPE_MASK = 0xf0000000,
    RUNTIME_CAPABILITY_COMPATIBLE_MASK = 0xfe0003ff,
};

#if defined(__ELF__)
#define RUNTIME_CAPABILITY_SECTION \
    __attribute__((section(".runtime_task.state")))
#else
#define RUNTIME_CAPABILITY_SECTION
#endif

static RuntimeObjectRecord object_table[RUNTIME_OBJECT_CAPACITY]
    RUNTIME_CAPABILITY_SECTION;
static uint32_t capability_table[RUNTIME_CAPABILITY_CAPACITY]
    RUNTIME_CAPABILITY_SECTION;
static uint16_t object_count_value RUNTIME_CAPABILITY_SECTION;
static uint16_t capability_count_value RUNTIME_CAPABILITY_SECTION;

static const RuntimeFirebloomType runtime_object_record_type = {
    .identity = UINT64_C(0x6f626a726563),
    .element_size = sizeof(RuntimeObjectRecord),
    .bytewise_copyable = false,
};

static const RuntimeFirebloomType runtime_capability_record_type = {
    .identity = UINT64_C(0x636170726563),
    .element_size = sizeof(uint32_t),
    .bytewise_copyable = false,
};

const RuntimeFirebloomType runtime_memory_mapping_type = {
    .identity = UINT64_C(0x6d617070696e67),
    .element_size = 0x20,
    .bytewise_copyable = false,
};

static void runtime_capability_trap(void) {
    __builtin_trap();
}

/* Host-only owner tests omit the platform panic transport. The production
 * diagnostic owner is strong and replaces this observable trap fallback. */
_Noreturn __attribute__((weak)) void runtime_fatal_at(
    uint64_t source_token,
    uint32_t line
) {
    (void)source_token;
    (void)line;
    __builtin_trap();
}

static bool runtime_capability_rights_are_valid(uint8_t rights) {
    return rights != 0 && (rights & ~UINT8_C(7)) == 0;
}

static RuntimeObjectRecord *runtime_object_record_if_valid(uint16_t object_id) {
    if (object_id == 0 || object_id >= object_count_value) {
        return 0;
    }
    RuntimeObjectRecord *record = &object_table[object_id];
    return record->object != 0 && record->type != 0 ? record : 0;
}

static uint32_t *runtime_capability_entry_if_valid(uint16_t handle) {
    if (handle == 0 || handle >= capability_count_value) {
        return 0;
    }
    return &capability_table[handle];
}

AVP_RECOVERED_ENTRY static uint16_t runtime_capability_find_compatible(
    uint32_t candidate
) {
    for (uint16_t handle = 1;
         handle < capability_count_value;
         ++handle) {
        if (((capability_table[handle] ^ candidate) &
                RUNTIME_CAPABILITY_COMPATIBLE_MASK) == 0) {
            return handle;
        }
    }
    return 0;
}

static bool runtime_capability_entry_allows(
    uint32_t entry,
    uint8_t type,
    uint8_t required_rights,
    uint8_t domain
) {
    const uint8_t entry_type = (uint8_t)(entry >> 28);
    const uint8_t entry_rights = (uint8_t)((entry >> 25) & 7);
    const uint16_t packed_audience =
        (uint16_t)((entry & RUNTIME_CAPABILITY_AUDIENCE_MASK) >> 10);
    return (type == RUNTIME_CAPABILITY_TYPE_ANY || entry_type == type) &&
        (entry_rights & required_rights) == required_rights &&
        domain != 0 && domain <= 15 &&
        (packed_audience & (UINT16_C(1) << (domain - 1))) != 0;
}

bool runtime_capability_address_is_canonical(uintptr_t address) {
    const uintptr_t upper = address >> 42;
    return upper == 0 || upper == UINT32_C(0x3fffff);
}

void runtime_capability_validate_address(uintptr_t address) {
    if (!runtime_capability_address_is_canonical(address)) {
        runtime_capability_address_failure();
    }
}

_Noreturn AVP_RECOVERED_ENTRY void runtime_capability_address_failure(void) {
    runtime_fatal_at(UINT64_C(0x01e87eca99c7dc29), UINT32_C(0x15e));
}

void runtime_capability_initialize(void) {
    for (uint16_t index = 0; index < RUNTIME_OBJECT_CAPACITY; ++index) {
        object_table[index] = (RuntimeObjectRecord){0};
    }
    for (uint16_t index = 0; index < RUNTIME_CAPABILITY_CAPACITY; ++index) {
        capability_table[index] = 0;
    }
    object_count_value = 1;
    capability_count_value = 1;
    runtime_state_publish_registry_counts(
        object_count_value,
        capability_count_value
    );
}

AVP_RECOVERED_ENTRY void runtime_capability_clear_tuple(void *storage) {
    uint64_t *words = storage;
    if (words == 0) {
        runtime_capability_trap();
    }
    words[0] = 0;
    words[1] = 0;
    words[2] = 0;
    words[3] = 0;
}

RuntimeFirebloomSpan runtime_object_registry_span(void) {
    return (RuntimeFirebloomSpan){
        .current = object_table,
        .lower_bound = object_table,
        .upper_bound = object_table + RUNTIME_OBJECT_CAPACITY,
        .type = &runtime_object_record_type,
    };
}

RuntimeFirebloomSpan runtime_capability_registry_span(void) {
    return (RuntimeFirebloomSpan){
        .current = capability_table,
        .lower_bound = capability_table,
        .upper_bound = capability_table + RUNTIME_CAPABILITY_CAPACITY,
        .type = &runtime_capability_record_type,
    };
}

uint16_t runtime_capability_audience_for_domain(uint8_t domain) {
    if (domain == 0 || domain > 15) {
        runtime_capability_trap();
    }
    return (uint16_t)(UINT16_C(1) << domain);
}

AVP_RECOVERED_ENTRY uint16_t runtime_capability_normalize_audience(
    uint16_t audience
) {
    if (audience == 0 || (audience & UINT16_C(1)) != 0) {
        runtime_capability_trap();
    }
    return (uint16_t)(audience >> 1);
}

uint16_t runtime_object_publish(
    RuntimeFirebloomSpan object,
    size_t minimum_size
) {
    const uintptr_t current = (uintptr_t)object.current;
    const uintptr_t lower = (uintptr_t)object.lower_bound;
    const uintptr_t upper = (uintptr_t)object.upper_bound;
    if (object.current == 0 || object.type == 0 || minimum_size == 0 ||
        object.type->element_size < minimum_size || current < lower ||
        current >= upper || upper - current < object.type->element_size) {
        runtime_capability_trap();
    }
    runtime_capability_validate_address(current);
    for (uint16_t object_id = 1;
         object_id < object_count_value;
         ++object_id) {
        RuntimeObjectRecord *record = &object_table[object_id];
        if (record->object == object.current) {
            return object_id;
        }
    }
    if (object_count_value == RUNTIME_OBJECT_CAPACITY) {
        runtime_capability_trap();
    }
    const uint16_t object_id = object_count_value++;
    object_table[object_id] = (RuntimeObjectRecord){
        .object = object.current,
        .type = object.type,
    };
    runtime_state_publish_registry_counts(
        object_count_value,
        capability_count_value
    );
    return object_id;
}

RuntimeFirebloomSpan runtime_object_resolve(
    uint16_t object_id,
    size_t minimum_size
) {
    RuntimeObjectRecord *record = runtime_object_record_if_valid(object_id);
    if (record == 0 || minimum_size == 0 ||
        record->type->element_size < minimum_size) {
        runtime_capability_trap();
    }
    return (RuntimeFirebloomSpan){
        .current = record->object,
        .lower_bound = record->object,
        .upper_bound = (uint8_t *)record->object + record->type->element_size,
        .type = record->type,
    };
}

uint16_t runtime_capability_publish_object_id(
    uint8_t type,
    uint16_t audience,
    uint8_t rights,
    uint16_t object_id
) {
    if (type == 0 || type > 15 ||
        !runtime_capability_rights_are_valid(rights) ||
        runtime_object_record_if_valid(object_id) == 0) {
        runtime_capability_trap();
    }
    const volatile RuntimeGlobalState *state = runtime_state_get();
    if (type != RUNTIME_CAPABILITY_TYPE_MEMORY_MAPPING &&
        (state->lifecycle_flags & UINT8_C(1)) != 0) {
        runtime_capability_trap();
    }
    const uint16_t normalized_audience =
        runtime_capability_normalize_audience(audience);
    const uint32_t candidate =
        ((uint32_t)type << 28) |
        ((uint32_t)rights << 25) |
        ((uint32_t)normalized_audience << 10) |
        object_id;
    const uint16_t compatible =
        runtime_capability_find_compatible(candidate);
    if (compatible != 0) {
        capability_table[compatible] |=
            (uint32_t)normalized_audience << 10;
        return compatible;
    }
    if (capability_count_value == RUNTIME_CAPABILITY_CAPACITY) {
        runtime_capability_trap();
    }
    const uint16_t handle = capability_count_value++;
    capability_table[handle] = candidate;
    runtime_state_publish_registry_counts(
        object_count_value,
        capability_count_value
    );
    return handle;
}

uint16_t runtime_capability_publish(
    void *object,
    const RuntimeFirebloomType *object_type,
    uint8_t type,
    uint8_t rights,
    uint16_t audience
) {
    if (object_type == 0) {
        runtime_capability_trap();
    }
    const uint16_t object_id = runtime_object_publish(
        (RuntimeFirebloomSpan){
            .current = object,
            .lower_bound = object,
            .upper_bound = (uint8_t *)object + object_type->element_size,
            .type = object_type,
        },
        object_type->element_size
    );
    return runtime_capability_publish_object_id(
        type,
        audience,
        rights,
        object_id
    );
}

AVP_RECOVERED_ENTRY const uint32_t *runtime_capability_entry_lookup(
    uint16_t handle
) {
    uint32_t *entry = runtime_capability_entry_if_valid(handle);
    if (entry == 0) {
        runtime_capability_trap();
    }
    return entry;
}

AVP_RECOVERED_ENTRY const uint32_t *runtime_capability_require_entry(
    uint16_t handle,
    uint8_t type,
    uint8_t required_rights
) {
    uint32_t *entry = runtime_capability_entry_if_valid(handle);
    if (entry == 0 || !runtime_capability_rights_are_valid(required_rights) ||
        (type != RUNTIME_CAPABILITY_TYPE_ANY && type > 15) ||
        !runtime_capability_entry_allows(
            *entry,
            type,
            required_rights,
            runtime_current_domain()
        )) {
        runtime_capability_trap();
    }
    return entry;
}

AVP_RECOVERED_ENTRY uint16_t runtime_capability_derive(
    uint16_t source_handle,
    uint8_t rights,
    uint16_t audience
) {
    if (!runtime_capability_rights_are_valid(rights)) {
        runtime_capability_trap();
    }
    const uint32_t source = *runtime_capability_require_entry(
        source_handle,
        RUNTIME_CAPABILITY_TYPE_ANY,
        rights | RUNTIME_CAPABILITY_RIGHT_DELEGATE
    );
    return runtime_capability_publish_object_id(
        (uint8_t)(source >> 28),
        audience,
        rights,
        runtime_capability_object_id(source)
    );
}

void runtime_capability_revoke_rights(uint16_t handle, uint8_t rights) {
    if (!runtime_capability_rights_are_valid(rights)) {
        runtime_capability_trap();
    }
    (void)runtime_capability_require_entry(
        handle,
        RUNTIME_CAPABILITY_TYPE_ANY,
        rights
    );
    capability_table[handle] &= ~((uint32_t)rights << 25);
}

AVP_RECOVERED_ENTRY uint16_t runtime_capability_object_id(uint32_t entry) {
    return (uint16_t)(entry & RUNTIME_CAPABILITY_OBJECT_ID_MASK);
}

uint16_t runtime_capability_object_count(void) {
    return object_count_value;
}

uint16_t runtime_capability_count(void) {
    return capability_count_value;
}

RuntimeFirebloomSpan runtime_capability_resolve_span(
    uint16_t handle,
    uint8_t type,
    uint8_t required_rights,
    size_t minimum_size
) {
    uint32_t *entry = runtime_capability_entry_if_valid(handle);
    if (entry == 0 || !runtime_capability_rights_are_valid(required_rights) ||
        (type != RUNTIME_CAPABILITY_TYPE_ANY && type > 15) ||
        !runtime_capability_entry_allows(
            *entry,
            type,
            required_rights,
            runtime_current_domain()
        )) {
        return (RuntimeFirebloomSpan){0};
    }
    return runtime_object_resolve(
        runtime_capability_object_id(*entry),
        minimum_size
    );
}

void *runtime_capability_resolve(
    uint16_t handle,
    uint8_t type,
    uint8_t required_rights
) {
    return runtime_capability_resolve_span(
        handle,
        type,
        required_rights,
        1
    ).current;
}

AVP_RECOVERED_ENTRY void *runtime_domain_resolve_capability(
    uint16_t handle,
    uint8_t required_rights
) {
    return runtime_capability_resolve_span(
        handle,
        RUNTIME_CAPABILITY_TYPE_DOMAIN,
        required_rights,
        1u
    ).current;
}

AVP_RECOVERED_ENTRY RuntimeFirebloomSpan runtime_event_resolve_capability(
    uint16_t handle,
    uint8_t required_rights
) {
    return runtime_capability_resolve_span(
        handle,
        RUNTIME_CAPABILITY_TYPE_EVENT,
        required_rights,
        0x48u
    );
}

AVP_RECOVERED_ENTRY RuntimeFirebloomSpan runtime_endpoint_resolve_capability(
    uint16_t handle,
    uint8_t required_rights
) {
    return runtime_capability_resolve_span(
        handle,
        RUNTIME_CAPABILITY_TYPE_SERVICE_ENDPOINT,
        required_rights,
        0x90u
    );
}

AVP_RECOVERED_ENTRY RuntimeFirebloomSpan runtime_type6_resolve_capability(
    uint16_t handle,
    uint8_t required_rights
) {
    return runtime_capability_resolve_span(
        handle,
        RUNTIME_CAPABILITY_TYPE_OBJECT,
        required_rights,
        0x90u
    );
}
