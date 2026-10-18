#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


enum {
    RUNTIME_BOOTSTRAP_MEMORY_DESCRIPTOR_SIZE = 0xb8,
    RUNTIME_BOOTSTRAP_MEMORY_PAGE_SIZE = 0x4000,
    RUNTIME_BOOTSTRAP_MEMORY_PAGE_TABLE_MAX = 0x7c000,
    RUNTIME_BOOTSTRAP_MEMORY_BITMAP_RECORD_SIZE = 0x40,
    RUNTIME_BOOTSTRAP_MEMORY_PAGE_ID_MAX = 637,
    RUNTIME_BOOTSTRAP_MEMORY_PAGE_ID_SENTINEL = 171,
    RUNTIME_BOOTSTRAP_MEMORY_BITMAP_WORDS = 10,
};

typedef struct {
    uintptr_t lower_bound;
    uintptr_t upper_bound;
} RuntimeMemorySpan;

_Static_assert(sizeof(RuntimeMemorySpan) == 0x10, "memory span size");

typedef struct {
    uintptr_t base;
    size_t size;
} RuntimeMemoryRegion;

_Static_assert(sizeof(RuntimeMemoryRegion) == 0x10, "memory region size");

typedef struct {
    uint32_t lower_page;
    uint32_t upper_page;
    uint8_t memory_type;
    uint8_t permissions;
    uint8_t reserved_0a;
    uint8_t flags;
} RuntimeBootstrapMemoryMapEntry;

_Static_assert(
    sizeof(RuntimeBootstrapMemoryMapEntry) == 0x0c,
    "bootstrap memory-map entry size"
);
_Static_assert(
    offsetof(RuntimeBootstrapMemoryMapEntry, flags) == 0x0b,
    "bootstrap memory-map flags offset"
);

typedef struct {
    uint8_t opaque_00[0x18];
    uint16_t page_id;
    uint8_t opaque_1a[0x26];
} RuntimeBootstrapMemoryBitmapRecord;

_Static_assert(
    sizeof(RuntimeBootstrapMemoryBitmapRecord) ==
        RUNTIME_BOOTSTRAP_MEMORY_BITMAP_RECORD_SIZE,
    "bootstrap bitmap record size"
);
_Static_assert(
    offsetof(RuntimeBootstrapMemoryBitmapRecord, page_id) == 0x18,
    "bootstrap bitmap page-id offset"
);

typedef struct {
    uint64_t options;
    RuntimeMemoryRegion fixed_regions[3];
    const RuntimeBootstrapMemoryMapEntry *memory_map;
    size_t memory_map_count;
    size_t replica_count;
    size_t replica_stride;
    RuntimeMemoryRegion primary_storage;
    RuntimeMemoryRegion platform_storage;
    RuntimeMemoryRegion page_table_storage;
    RuntimeMemoryRegion bitmap_records;
    RuntimeMemoryRegion reserved_pair;
    RuntimeMemoryRegion auxiliary_storage;
} RuntimeBootstrapMemoryDescriptor;

_Static_assert(
    sizeof(RuntimeBootstrapMemoryDescriptor) ==
        RUNTIME_BOOTSTRAP_MEMORY_DESCRIPTOR_SIZE,
    "bootstrap memory descriptor size"
);
_Static_assert(
    offsetof(RuntimeBootstrapMemoryDescriptor, memory_map) == 0x38,
    "bootstrap memory-map offset"
);
_Static_assert(
    offsetof(RuntimeBootstrapMemoryDescriptor, replica_count) == 0x48,
    "bootstrap replica-count offset"
);
_Static_assert(
    offsetof(RuntimeBootstrapMemoryDescriptor, primary_storage) == 0x58,
    "bootstrap primary-storage offset"
);
_Static_assert(
    offsetof(RuntimeBootstrapMemoryDescriptor, page_table_storage) == 0x78,
    "bootstrap page-table storage offset"
);
_Static_assert(
    offsetof(RuntimeBootstrapMemoryDescriptor, bitmap_records) == 0x88,
    "bootstrap bitmap-record offset"
);
_Static_assert(
    offsetof(RuntimeBootstrapMemoryDescriptor, reserved_pair) == 0x98,
    "bootstrap reserved-pair offset"
);
_Static_assert(
    offsetof(RuntimeBootstrapMemoryDescriptor, auxiliary_storage) == 0xa8,
    "bootstrap auxiliary-storage offset"
);

typedef struct {
    uint64_t words[RUNTIME_BOOTSTRAP_MEMORY_BITMAP_WORDS];
} RuntimeBootstrapMemoryBitmap;

typedef enum {
    RUNTIME_BOOTSTRAP_MEMORY_PRIMARY_STORAGE = 0,
    RUNTIME_BOOTSTRAP_MEMORY_PAGE_TABLE_STORAGE = 1,
    RUNTIME_BOOTSTRAP_MEMORY_AUXILIARY_STORAGE = 2,
} RuntimeBootstrapMemoryRegionKind;

typedef struct {
    bool (*region_is_usable)(void *context, RuntimeMemorySpan region);
    uintptr_t (*map_region)(
        void *context,
        RuntimeBootstrapMemoryRegionKind kind,
        RuntimeMemorySpan region
    );
    uintptr_t (*initialize_platform_layout)(
        void *context,
        const RuntimeBootstrapMemoryDescriptor *descriptor
    );
    void (*publish_available_region)(
        void *context,
        RuntimeMemorySpan region,
        uint8_t memory_type,
        uint8_t permissions
    );
    void *context;
} RuntimeBootstrapMemoryOperations;

typedef struct {
    bool descriptor_enabled;
    RuntimeMemorySpan image_region;
    RuntimeMemorySpan runtime_static_region;
    const uintptr_t *reserved_ranges;
    size_t reserved_range_count;
    const RuntimeBootstrapMemoryBitmap *forbidden_page_ids;
    const RuntimeBootstrapMemoryBitmap *allocated_page_ids;
    RuntimeBootstrapMemoryOperations operations;
} RuntimeBootstrapMemoryProfile;

typedef struct {
    bool initialized;
    uint8_t page_table_page_count;
    uintptr_t primary_storage_mapping;
    uintptr_t page_table_storage_mapping;
    uintptr_t auxiliary_storage_mapping;
    uintptr_t platform_high_watermark;
    RuntimeMemorySpan image_region;
    RuntimeMemorySpan runtime_static_region;
    RuntimeBootstrapMemoryBitmap published_page_ids;
} RuntimeBootstrapMemoryState;

bool runtime_memory_bitmap_test(
    const RuntimeBootstrapMemoryBitmap *bitmap,
    uint16_t page_id
);
void runtime_memory_bitmap_set(
    RuntimeBootstrapMemoryBitmap *bitmap,
    uint16_t page_id
);
size_t runtime_memory_subtract_reserved_ranges(
    const RuntimeMemorySpan *input,
    size_t input_count,
    const uintptr_t *reserved_ranges,
    size_t reserved_range_count,
    RuntimeMemorySpan *output,
    size_t output_capacity
);
bool runtime_bootstrap_memory_is_boot_owner(void);
bool runtime_bootstrap_memory_is_boot_task(void);
void runtime_bootstrap_memory_initialize(
    const RuntimeBootstrapMemoryDescriptor *descriptor,
    size_t descriptor_size,
    const RuntimeBootstrapMemoryProfile *profile,
    RuntimeBootstrapMemoryState *state
);
void runtime_bootstrap_memory_initialize_matching(void);
const RuntimeBootstrapMemoryState *runtime_bootstrap_memory_state(void);
