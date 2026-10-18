#include "avp/runtime/ipc/runtime_registry.h"
#include "avp/runtime/support/recovered_symbol.h"


enum {
    RUNTIME_REGISTRY_MAX_RECORDS = 64,
    RUNTIME_REGISTRY_REPRESENTATION_MASK = 0x0f,
};

static const AVPRuntimeRegistryRecord sep_preflight_deadline = {
    .key = AVP_RUNTIME_KEY_SEP_PREFLIGHT_DEADLINE,
    .type = AVP_RUNTIME_TYPE_U64 >> 16,
    .flags = AVP_RUNTIME_REGISTRY_REPRESENTATION_INLINE,
    .payload.scalar = 210000,
};

static const AVPRuntimeRegistryRecord *const builtin_records[] = {
    &sep_preflight_deadline,
};

static const AVPRuntimeRegistryRecord *const *registry_records =
    builtin_records;
static size_t registry_record_count =
    sizeof(builtin_records) / sizeof(builtin_records[0]);
static const AVPRuntimeRegistryRecord *registry_index[
    RUNTIME_REGISTRY_MAX_RECORDS
];
static size_t registry_index_count;
static bool registry_indexed;

static void runtime_registry_copy(
    void *destination,
    const void *source,
    size_t size
) {
    uint8_t *output = destination;
    const uint8_t *input = source;
    for (size_t index = 0; index < size; ++index) {
        output[index] = input[index];
    }
}

AVP_RECOVERED_ENTRY static const AVPRuntimeRegistryRecord **
runtime_registry_index_slot(
    uint32_t key
) {
    for (size_t index = 0; index < registry_index_count; ++index) {
        if (registry_index[index]->key == key) {
            return &registry_index[index];
        }
    }
    if (registry_index_count == RUNTIME_REGISTRY_MAX_RECORDS) {
        __builtin_trap();
    }
    return &registry_index[registry_index_count++];
}

static void runtime_registry_build_index(void) {
    if (registry_indexed) {
        return;
    }
    registry_indexed = true;
    registry_index_count = 0;
    /* Concrete records win independently of linker pointer-table order. */
    for (size_t index = 0; index < registry_record_count; ++index) {
        const AVPRuntimeRegistryRecord *record = registry_records[index];
        if (record == 0 || record->key == 0) {
            __builtin_trap();
        }
        if ((record->flags & AVP_RUNTIME_REGISTRY_FLAG_ALIAS) != 0) {
            continue;
        }
        const AVPRuntimeRegistryRecord **slot = runtime_registry_index_slot(
            record->key
        );
        if (*slot != 0) {
            __builtin_trap();
        }
        *slot = record;
    }
    for (size_t index = 0; index < registry_record_count; ++index) {
        const AVPRuntimeRegistryRecord *record = registry_records[index];
        if ((record->flags & AVP_RUNTIME_REGISTRY_FLAG_ALIAS) == 0) {
            continue;
        }
        const AVPRuntimeRegistryRecord **slot = runtime_registry_index_slot(
            record->key
        );
        if (*slot == 0) {
            *slot = record;
        } else if (((*slot)->flags & AVP_RUNTIME_REGISTRY_FLAG_ALIAS) != 0) {
            __builtin_trap();
        }
    }
}

static size_t runtime_registry_value_size(uint8_t type) {
    switch ((uint32_t)type << 16) {
        case AVP_RUNTIME_TYPE_U32:
        case AVP_RUNTIME_TYPE_S32:
            return sizeof(uint32_t);
        case AVP_RUNTIME_TYPE_U64:
        case AVP_RUNTIME_TYPE_S64:
            return sizeof(uint64_t);
        case AVP_RUNTIME_TYPE_U8:
            return sizeof(uint8_t);
        case AVP_RUNTIME_TYPE_BYTES_32:
        case AVP_RUNTIME_TYPE_RANGE_32:
            return 32;
    }
    __builtin_trap();
}

void runtime_registry_initialize(
    const AVPRuntimeRegistryRecord *const *records,
    size_t record_count
) {
    if ((records == 0) != (record_count == 0) ||
        record_count > RUNTIME_REGISTRY_MAX_RECORDS) {
        __builtin_trap();
    }
    for (size_t index = 0; index < RUNTIME_REGISTRY_MAX_RECORDS; ++index) {
        registry_index[index] = 0;
    }
    registry_records = records == 0 ? builtin_records : records;
    registry_record_count = records == 0 ?
        sizeof(builtin_records) / sizeof(builtin_records[0]) : record_count;
    registry_index_count = 0;
    registry_indexed = false;
}

const AVPRuntimeRegistryRecord *runtime_registry_lookup_entry(uint32_t key) {
    runtime_registry_build_index();
    for (size_t index = 0; index < registry_index_count; ++index) {
        if (registry_index[index]->key == key) {
            return registry_index[index];
        }
    }
    return 0;
}

static bool runtime_registry_materialize_value_depth(
    const AVPRuntimeRegistryRecord *record,
    void *output,
    size_t depth
) {
    if (record == 0) {
        return false;
    }
    if (output == 0) {
        __builtin_trap();
    }
    const uint8_t representation =
        record->flags & RUNTIME_REGISTRY_REPRESENTATION_MASK;
    if (representation == AVP_RUNTIME_REGISTRY_REPRESENTATION_ALIAS) {
        if (record->payload.alias == 0 ||
            depth == RUNTIME_REGISTRY_MAX_RECORDS) {
            __builtin_trap();
        }
        return runtime_registry_materialize_value_depth(
            record->payload.alias,
            output,
            depth + 1
        );
    }
    if ((record->flags & AVP_RUNTIME_REGISTRY_FLAG_WIDE) != 0) {
        runtime_registry_copy(output, &record->words[1], 32);
        return true;
    }
    if (representation == AVP_RUNTIME_REGISTRY_REPRESENTATION_CALLBACK) {
        if (record->payload.callback == 0) {
            __builtin_trap();
        }
        record->payload.callback(
            record->key,
            (uintptr_t)record->words[1],
            output
        );
        return true;
    }
    const void *source = &record->payload;
    if (representation == AVP_RUNTIME_REGISTRY_REPRESENTATION_INDIRECT) {
        source = record->payload.indirect;
        if (source == 0) {
            __builtin_trap();
        }
    } else if (representation !=
        AVP_RUNTIME_REGISTRY_REPRESENTATION_INLINE) {
        __builtin_trap();
    }
    runtime_registry_copy(
        output,
        source,
        runtime_registry_value_size(record->type)
    );
    return true;
}

bool runtime_registry_materialize_value(
    const AVPRuntimeRegistryRecord *record,
    void *output
) {
    return runtime_registry_materialize_value_depth(record, output, 0);
}

AVP_RECOVERED_ENTRY bool runtime_registry_read_typed_optional(
    uint32_t key,
    uint32_t expected_type,
    void *output
) {
    const AVPRuntimeRegistryRecord *record =
        runtime_registry_lookup_entry(key);
    if (record == 0) {
        return false;
    }
    if (expected_type != 0 &&
        expected_type != (uint32_t)record->type << 16) {
        __builtin_trap();
    }
    return runtime_registry_materialize_value(record, output);
}

AVP_RECOVERED_ENTRY void runtime_registry_read_typed_required(
    uint32_t key,
    uint32_t expected_type,
    void *output
) {
    if (!runtime_registry_read_typed_optional(key, expected_type, output)) {
        __builtin_trap();
    }
}

bool runtime_registry_read_u64(
    uint32_t key,
    uint32_t type,
    uint64_t *value
) {
    return runtime_registry_read_typed_optional(key, type, value);
}
