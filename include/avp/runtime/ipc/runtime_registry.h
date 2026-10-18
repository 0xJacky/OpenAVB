#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


enum {
    AVP_RUNTIME_KEY_SEP_PREFLIGHT_DEADLINE = 0x2000,
    AVP_RUNTIME_TYPE_U32 = 0x10000,
    AVP_RUNTIME_TYPE_S32 = 0x20000,
    AVP_RUNTIME_TYPE_U64 = 0x30000,
    AVP_RUNTIME_TYPE_S64 = 0x40000,
    AVP_RUNTIME_TYPE_U8 = 0x50000,
    AVP_RUNTIME_TYPE_BYTES_32 = 0x60000,
    AVP_RUNTIME_TYPE_RANGE_32 = 0x70000,
    AVP_RUNTIME_TYPE_ABSOLUTE_MICROSECOND_DEADLINE = AVP_RUNTIME_TYPE_U64,
};

enum {
    AVP_RUNTIME_REGISTRY_REPRESENTATION_INLINE = 0,
    AVP_RUNTIME_REGISTRY_REPRESENTATION_INDIRECT = 1,
    AVP_RUNTIME_REGISTRY_REPRESENTATION_CALLBACK = 2,
    AVP_RUNTIME_REGISTRY_REPRESENTATION_ALIAS = 3,
    AVP_RUNTIME_REGISTRY_FLAG_ALIAS = 0x10,
    AVP_RUNTIME_REGISTRY_FLAG_WIDE = 0x20,
};

typedef struct AVPRuntimeRegistryRecord AVPRuntimeRegistryRecord;

typedef void (*AVPRuntimeRegistryMaterializer)(
    uint32_t key,
    uintptr_t context,
    void *output
);

typedef union {
    uint64_t scalar;
    const void *indirect;
    const AVPRuntimeRegistryRecord *alias;
    AVPRuntimeRegistryMaterializer callback;
} AVPRuntimeRegistryPayload;

struct AVPRuntimeRegistryRecord {
    uint32_t key;
    uint8_t reserved_004;
    uint8_t reserved_005;
    uint8_t type;
    uint8_t flags;
    AVPRuntimeRegistryPayload payload;
    uint64_t words[5];
};

_Static_assert(
    offsetof(AVPRuntimeRegistryRecord, type) == 0x6,
    "runtime registry type offset"
);
_Static_assert(
    offsetof(AVPRuntimeRegistryRecord, flags) == 0x7,
    "runtime registry flags offset"
);
_Static_assert(
    offsetof(AVPRuntimeRegistryRecord, payload) == 0x8,
    "runtime registry payload offset"
);
_Static_assert(
    offsetof(AVPRuntimeRegistryRecord, words[1]) == 0x18,
    "runtime registry wide value offset"
);
_Static_assert(
    sizeof(AVPRuntimeRegistryRecord) == 0x38,
    "runtime registry record size"
);

void runtime_registry_initialize(
    const AVPRuntimeRegistryRecord *const *records,
    size_t record_count
);
const AVPRuntimeRegistryRecord *runtime_registry_lookup_entry(uint32_t key);
bool runtime_registry_materialize_value(
    const AVPRuntimeRegistryRecord *record,
    void *output
);
bool runtime_registry_read_typed_optional(
    uint32_t key,
    uint32_t expected_type,
    void *output
);
void runtime_registry_read_typed_required(
    uint32_t key,
    uint32_t expected_type,
    void *output
);
bool runtime_registry_read_u64(
    uint32_t key,
    uint32_t type,
    uint64_t *value
);
