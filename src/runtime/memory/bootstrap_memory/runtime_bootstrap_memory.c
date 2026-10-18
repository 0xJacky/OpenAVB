#include "avp/runtime/memory/runtime_bootstrap_memory.h"

#if defined(__ELF__)
#define AVP_BOOTSTRAP_MEMORY_RECOVERED \
    __attribute__((noinline, section(".text.runtime_bootstrap_memory_recovered")))
#else
#define AVP_BOOTSTRAP_MEMORY_RECOVERED __attribute__((noinline))
#endif

enum {
    RUNTIME_MEMORY_MAP_FLAG_REPLICATED = 1u << 1,
    RUNTIME_MEMORY_MAP_FLAG_EXCLUDED = 1u << 2,
    RUNTIME_MEMORY_SPLIT_CAPACITY = 32,
};

static RuntimeBootstrapMemoryState matching_state;

_Noreturn static void runtime_bootstrap_memory_trap(void) {
    __builtin_trap();
}

static bool runtime_memory_span_is_valid(RuntimeMemorySpan span) {
    return span.lower_bound != 0 && span.upper_bound > span.lower_bound;
}

static bool runtime_memory_region_is_empty(RuntimeMemoryRegion region) {
    return region.base == 0 && region.size == 0;
}

static RuntimeMemorySpan runtime_memory_region_span(RuntimeMemoryRegion region) {
    if (region.base == 0 || region.size == 0 ||
        region.base > UINTPTR_MAX - region.size) {
        runtime_bootstrap_memory_trap();
    }
    return (RuntimeMemorySpan){
        .lower_bound = region.base,
        .upper_bound = region.base + region.size,
    };
}

static void runtime_memory_clear(void *storage, size_t size) {
    uint8_t *bytes = storage;
    for (size_t index = 0; index < size; ++index) {
        bytes[index] = 0;
    }
}

AVP_BOOTSTRAP_MEMORY_RECOVERED
bool runtime_memory_bitmap_test(
    const RuntimeBootstrapMemoryBitmap *bitmap,
    uint16_t page_id
) {
    if (bitmap == 0 || page_id > RUNTIME_BOOTSTRAP_MEMORY_PAGE_ID_MAX) {
        runtime_bootstrap_memory_trap();
    }
    return ((bitmap->words[page_id >> 6] >> (page_id & 63)) & 1u) != 0;
}

AVP_BOOTSTRAP_MEMORY_RECOVERED
void runtime_memory_bitmap_set(
    RuntimeBootstrapMemoryBitmap *bitmap,
    uint16_t page_id
) {
    if (bitmap == 0 || page_id > RUNTIME_BOOTSTRAP_MEMORY_PAGE_ID_MAX) {
        runtime_bootstrap_memory_trap();
    }
    bitmap->words[page_id >> 6] |= UINT64_C(1) << (page_id & 63);
}

static void runtime_memory_append_span(
    RuntimeMemorySpan *output,
    size_t output_capacity,
    size_t *output_count,
    RuntimeMemorySpan span
) {
    if (*output_count >= output_capacity ||
        span.upper_bound <= span.lower_bound) {
        runtime_bootstrap_memory_trap();
    }
    output[*output_count] = span;
    ++*output_count;
}

AVP_BOOTSTRAP_MEMORY_RECOVERED
size_t runtime_memory_subtract_reserved_ranges(
    const RuntimeMemorySpan *input,
    size_t input_count,
    const uintptr_t *reserved_ranges,
    size_t reserved_range_count,
    RuntimeMemorySpan *output,
    size_t output_capacity
) {
    if ((input_count != 0 && input == 0) ||
        (reserved_range_count != 0 && reserved_ranges == 0) ||
        (output_capacity != 0 && output == 0)) {
        runtime_bootstrap_memory_trap();
    }
    size_t output_count = 0;
    for (size_t input_index = 0; input_index < input_count; ++input_index) {
        const RuntimeMemorySpan source = input[input_index];
        if (!runtime_memory_span_is_valid(source)) {
            runtime_bootstrap_memory_trap();
        }
        uintptr_t cursor = source.lower_bound;
        for (size_t reserved_index = 0;
             reserved_index < reserved_range_count;
             ++reserved_index) {
            const uintptr_t encoded = reserved_ranges[reserved_index];
            const uintptr_t reserved_lower = encoded & ~(uintptr_t)0x0f;
            const uintptr_t reserved_pages = encoded & 0x0f;
            const uintptr_t reserved_size =
                reserved_pages * RUNTIME_BOOTSTRAP_MEMORY_PAGE_SIZE;
            if (reserved_lower > UINTPTR_MAX - reserved_size) {
                runtime_bootstrap_memory_trap();
            }
            const uintptr_t reserved_upper = reserved_lower + reserved_size;
            if (reserved_upper > cursor && reserved_lower < source.upper_bound) {
                if (reserved_lower > cursor) {
                    runtime_memory_append_span(
                        output,
                        output_capacity,
                        &output_count,
                        (RuntimeMemorySpan){cursor, reserved_lower}
                    );
                }
                cursor = reserved_upper;
                if (cursor >= source.upper_bound) {
                    break;
                }
            }
        }
        if (cursor < source.upper_bound) {
            runtime_memory_append_span(
                output,
                output_capacity,
                &output_count,
                (RuntimeMemorySpan){cursor, source.upper_bound}
            );
        }
    }
    return output_count;
}

static uintptr_t runtime_bootstrap_memory_map_optional(
    const RuntimeBootstrapMemoryProfile *profile,
    RuntimeBootstrapMemoryRegionKind kind,
    RuntimeMemoryRegion region
) {
    if (runtime_memory_region_is_empty(region)) {
        return 0;
    }
    const RuntimeMemorySpan span = runtime_memory_region_span(region);
    if (
        !profile->operations.region_is_usable(
            profile->operations.context,
            span
        )) {
        runtime_bootstrap_memory_trap();
    }
    const uintptr_t mapping = profile->operations.map_region(
        profile->operations.context,
        kind,
        span
    );
    if (mapping == 0) {
        runtime_bootstrap_memory_trap();
    }
    return mapping;
}

static void runtime_bootstrap_memory_validate_page_table(
    RuntimeMemoryRegion storage,
    RuntimeBootstrapMemoryState *state
) {
    if (runtime_memory_region_is_empty(storage)) {
        return;
    }
    (void)runtime_memory_region_span(storage);
    if ((storage.size & (RUNTIME_BOOTSTRAP_MEMORY_PAGE_SIZE - 1)) != 0 ||
        storage.size > RUNTIME_BOOTSTRAP_MEMORY_PAGE_TABLE_MAX) {
        runtime_bootstrap_memory_trap();
    }
    state->page_table_page_count =
        (uint8_t)(storage.size / RUNTIME_BOOTSTRAP_MEMORY_PAGE_SIZE);
}

static void runtime_bootstrap_memory_publish_bitmap_records(
    const RuntimeBootstrapMemoryDescriptor *descriptor,
    const RuntimeBootstrapMemoryProfile *profile,
    RuntimeBootstrapMemoryState *state
) {
    if (runtime_memory_region_is_empty(descriptor->bitmap_records)) {
        return;
    }
    (void)runtime_memory_region_span(descriptor->bitmap_records);
    const size_t size = descriptor->bitmap_records.size;
    if (size < RUNTIME_BOOTSTRAP_MEMORY_BITMAP_RECORD_SIZE ||
        (size % RUNTIME_BOOTSTRAP_MEMORY_BITMAP_RECORD_SIZE) != 0) {
        runtime_bootstrap_memory_trap();
    }
    const RuntimeBootstrapMemoryBitmapRecord *records =
        (const RuntimeBootstrapMemoryBitmapRecord *)
            descriptor->bitmap_records.base;
    const size_t count = size / sizeof(*records);
    for (size_t index = 0; index < count; ++index) {
        const uint16_t page_id = records[index].page_id;
        if (page_id == RUNTIME_BOOTSTRAP_MEMORY_PAGE_ID_SENTINEL) {
            continue;
        }
        if (page_id == 0 ||
            page_id > RUNTIME_BOOTSTRAP_MEMORY_PAGE_ID_MAX ||
            runtime_memory_bitmap_test(profile->forbidden_page_ids, page_id) ||
            runtime_memory_bitmap_test(profile->allocated_page_ids, page_id)) {
            runtime_bootstrap_memory_trap();
        }
        runtime_memory_bitmap_set(&state->published_page_ids, page_id);
    }
}

static void runtime_bootstrap_memory_publish_map(
    const RuntimeBootstrapMemoryDescriptor *descriptor,
    const RuntimeBootstrapMemoryProfile *profile
) {
    if (descriptor->memory_map_count == 0) {
        return;
    }
    if (descriptor->memory_map == 0 || descriptor->replica_count == 0 ||
        (descriptor->replica_count != 1 && descriptor->replica_stride == 0)) {
        runtime_bootstrap_memory_trap();
    }
    for (size_t entry_index = 0;
         entry_index < descriptor->memory_map_count;
         ++entry_index) {
        const RuntimeBootstrapMemoryMapEntry *entry =
            &descriptor->memory_map[entry_index];
        if ((entry->flags & RUNTIME_MEMORY_MAP_FLAG_EXCLUDED) != 0) {
            continue;
        }
        for (size_t replica = 0;
             replica < descriptor->replica_count;
             ++replica) {
            if (replica != 0 &&
                (entry->flags & RUNTIME_MEMORY_MAP_FLAG_REPLICATED) == 0) {
                continue;
            }
            if (descriptor->replica_stride != 0 &&
                replica > UINTPTR_MAX / descriptor->replica_stride) {
                runtime_bootstrap_memory_trap();
            }
            const uintptr_t replica_offset =
                replica * descriptor->replica_stride;
            const uintptr_t lower =
                (uintptr_t)entry->lower_page << 14;
            const uintptr_t upper =
                (uintptr_t)entry->upper_page << 14;
            if (lower > UINTPTR_MAX - replica_offset ||
                upper > UINTPTR_MAX - replica_offset || upper < lower) {
                runtime_bootstrap_memory_trap();
            }
            const RuntimeMemorySpan input = {
                .lower_bound = lower + replica_offset,
                .upper_bound = upper + replica_offset,
            };
            RuntimeMemorySpan available[RUNTIME_MEMORY_SPLIT_CAPACITY];
            const size_t available_count =
                runtime_memory_subtract_reserved_ranges(
                    &input,
                    1,
                    profile->reserved_ranges,
                    profile->reserved_range_count,
                    available,
                    RUNTIME_MEMORY_SPLIT_CAPACITY
                );
            for (size_t available_index = 0;
                 available_index < available_count;
                 ++available_index) {
                profile->operations.publish_available_region(
                    profile->operations.context,
                    available[available_index],
                    entry->memory_type,
                    entry->permissions
                );
            }
        }
    }
}

AVP_BOOTSTRAP_MEMORY_RECOVERED
void runtime_bootstrap_memory_initialize(
    const RuntimeBootstrapMemoryDescriptor *descriptor,
    size_t descriptor_size,
    const RuntimeBootstrapMemoryProfile *profile,
    RuntimeBootstrapMemoryState *state
) {
    if (profile == 0 || state == 0 ||
        profile->operations.region_is_usable == 0 ||
        profile->operations.map_region == 0 ||
        profile->operations.initialize_platform_layout == 0 ||
        profile->operations.publish_available_region == 0 ||
        profile->forbidden_page_ids == 0 ||
        profile->allocated_page_ids == 0 ||
        (descriptor == 0) != (descriptor_size == 0) ||
        (!profile->descriptor_enabled && descriptor != 0)) {
        runtime_bootstrap_memory_trap();
    }
    runtime_memory_clear(state, sizeof(*state));
    state->image_region = profile->image_region;
    state->runtime_static_region = profile->runtime_static_region;
    if (descriptor == 0) {
        state->initialized = true;
        return;
    }
    if (descriptor_size != sizeof(*descriptor) ||
        descriptor->reserved_pair.base != 0 ||
        descriptor->reserved_pair.size != 0) {
        runtime_bootstrap_memory_trap();
    }
    state->primary_storage_mapping = runtime_bootstrap_memory_map_optional(
        profile,
        RUNTIME_BOOTSTRAP_MEMORY_PRIMARY_STORAGE,
        descriptor->primary_storage
    );
    runtime_bootstrap_memory_validate_page_table(
        descriptor->page_table_storage,
        state
    );
    state->page_table_storage_mapping = runtime_bootstrap_memory_map_optional(
        profile,
        RUNTIME_BOOTSTRAP_MEMORY_PAGE_TABLE_STORAGE,
        descriptor->page_table_storage
    );
    state->auxiliary_storage_mapping = runtime_bootstrap_memory_map_optional(
        profile,
        RUNTIME_BOOTSTRAP_MEMORY_AUXILIARY_STORAGE,
        descriptor->auxiliary_storage
    );
    runtime_bootstrap_memory_publish_bitmap_records(
        descriptor,
        profile,
        state
    );
    state->platform_high_watermark =
        profile->operations.initialize_platform_layout(
            profile->operations.context,
            descriptor
        );
    runtime_bootstrap_memory_publish_map(descriptor, profile);
    state->initialized = true;
}

static bool runtime_matching_region_is_usable(
    void *context,
    RuntimeMemorySpan region
) {
    (void)context;
    return runtime_memory_span_is_valid(region) &&
        region.lower_bound >= UINT64_C(0x70000000) &&
        region.upper_bound <= UINT64_C(0x70400000);
}

static uintptr_t runtime_matching_map_region(
    void *context,
    RuntimeBootstrapMemoryRegionKind kind,
    RuntimeMemorySpan region
) {
    (void)context;
    (void)kind;
    return region.lower_bound;
}

static uintptr_t runtime_matching_initialize_platform_layout(
    void *context,
    const RuntimeBootstrapMemoryDescriptor *descriptor
) {
    (void)context;
    (void)descriptor;
    return 0;
}

static void runtime_matching_publish_available_region(
    void *context,
    RuntimeMemorySpan region,
    uint8_t memory_type,
    uint8_t permissions
) {
    /* The matching fixed profile has no dynamic memory-map descriptor. */
    (void)context;
    (void)region;
    (void)memory_type;
    (void)permissions;
}

void runtime_bootstrap_memory_initialize_matching(void) {
    static const RuntimeBootstrapMemoryBitmap empty_bitmap;
    const RuntimeBootstrapMemoryProfile profile = {
        .descriptor_enabled = false,
        .image_region = {0, 0},
        .runtime_static_region = {
            UINT64_C(0x70058000),
            UINT64_C(0x70064000),
        },
        .forbidden_page_ids = &empty_bitmap,
        .allocated_page_ids = &empty_bitmap,
        .operations = {
            .region_is_usable = runtime_matching_region_is_usable,
            .map_region = runtime_matching_map_region,
            .initialize_platform_layout =
                runtime_matching_initialize_platform_layout,
            .publish_available_region =
                runtime_matching_publish_available_region,
        },
    };
    runtime_bootstrap_memory_initialize(0, 0, &profile, &matching_state);
}

const RuntimeBootstrapMemoryState *runtime_bootstrap_memory_state(void) {
    return &matching_state;
}
