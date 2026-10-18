#include "avp/runtime/memory/runtime_physical_memory.h"
#include "avp/runtime/memory/runtime_memory.h"


#if defined(__ELF__)
#define RUNTIME_PHYSICAL_MEMORY_RECOVERED \
    __attribute__((noinline, section(".text.runtime_support_recovered")))
#else
#define RUNTIME_PHYSICAL_MEMORY_RECOVERED __attribute__((noinline))
#endif

#define REGION_DESCRIPTOR(id, attributes) \
    ((((uint64_t)(attributes)) << 16) | (uint64_t)(id))

static const RuntimeFirebloomType region_record_type = {
    .identity = UINT64_C(0x5250474e),
    .element_size = 0x20,
    .bytewise_copyable = false,
};

static const RuntimeFirebloomType region_metadata_type = {
    .identity = UINT64_C(0x52504d44),
    .element_size = 1,
    .bytewise_copyable = true,
};

static uint8_t matching_metadata[RUNTIME_PHYSICAL_REGION_COUNT_MATCHING];
static RuntimePhysicalMemoryRegion matching_records[
    RUNTIME_PHYSICAL_REGION_COUNT_MATCHING
] = {
    {0x70064000, 0, 0x10000, REGION_DESCRIPTOR(0x00a6, 0x80000009), {0}},
    {0, 0, 0, REGION_DESCRIPTOR(0x0162, 0x80000009), {0}},
    {0, 0, 0, REGION_DESCRIPTOR(0x0163, 0x80000009), {0}},
    {0, 0, 0, REGION_DESCRIPTOR(0x00f9, 0x80080013), {0}},
    {0, 0, 0, REGION_DESCRIPTOR(0x0102, 0x80010009), {0}},
    {UINTPTR_MAX, 0, SIZE_MAX, REGION_DESCRIPTOR(0x0103, 0x80010009), {0}},
    {UINTPTR_MAX, 0, SIZE_MAX, REGION_DESCRIPTOR(0x010a, 0x80010009), {0}},
    {UINTPTR_MAX, 0, 0xa0000, REGION_DESCRIPTOR(0x0108, 0x80090013), {0}},
    {UINTPTR_MAX, 0, SIZE_MAX, REGION_DESCRIPTOR(0x00bf, 0x80010009), {0}},
    {0x90074000, 0x90074000, 0x1e000000,
     REGION_DESCRIPTOR(0x002e, 0x80100009), {0}},
    {0x70074000, 0x70074000, 0x20000000,
     REGION_DESCRIPTOR(0x002d, 0x80100009), {0}},
};

static RuntimePhysicalMemoryRegistry current_registry;

static RuntimePhysicalMemoryRegion *region_from_span(
    RuntimeFirebloomSpan span
) {
    if (span.current == 0) {
        return 0;
    }
    runtime_firebloom_validate_span(&span, 0x20);
    if (span.type != &region_record_type) {
        runtime_firebloom_panic();
    }
    return span.current;
}

static uintptr_t checked_upper_bound(uintptr_t base, size_t size) {
    if (UINTPTR_MAX - base < size) {
        runtime_firebloom_panic();
    }
    return base + size;
}

void runtime_physical_memory_registry_initialize(
    RuntimePhysicalMemoryRegistry registry
) {
    if (registry.count != 0 && registry.records == 0) {
        runtime_firebloom_panic();
    }
    current_registry = registry;
}

RUNTIME_PHYSICAL_MEMORY_RECOVERED
void runtime_physical_memory_registry_initialize_matching(void) {
    for (size_t index = 0;
         index < RUNTIME_PHYSICAL_REGION_COUNT_MATCHING;
         ++index) {
        matching_records[index].metadata = (RuntimeFirebloomSpan){
            .current = &matching_metadata[index],
            .lower_bound = &matching_metadata[index],
            .upper_bound = &matching_metadata[index] + 1,
            .type = &region_metadata_type,
        };
    }
    runtime_physical_memory_registry_initialize(
        (RuntimePhysicalMemoryRegistry){
            .records = matching_records,
            .count = RUNTIME_PHYSICAL_REGION_COUNT_MATCHING,
        }
    );
}

RUNTIME_PHYSICAL_MEMORY_RECOVERED
RuntimeFirebloomSpan runtime_physical_memory_region_lookup(
    uint16_t region_id
) {
    if (region_id == 0) {
        runtime_firebloom_panic();
    }
    if (region_id > RUNTIME_PHYSICAL_REGION_ID_MAX) {
        return (RuntimeFirebloomSpan){0};
    }
    for (size_t index = 0; index < current_registry.count; ++index) {
        RuntimePhysicalMemoryRegion *record = &current_registry.records[index];
        if (runtime_physical_memory_region_id(record) == region_id) {
            return (RuntimeFirebloomSpan){
                .current = record,
                .lower_bound = record,
                .upper_bound = (uint8_t *)record + 0x20,
                .type = &region_record_type,
            };
        }
    }
    return (RuntimeFirebloomSpan){0};
}

uint16_t runtime_physical_memory_region_id(
    const RuntimePhysicalMemoryRegion *region
) {
    if (region == 0) {
        runtime_firebloom_panic();
    }
    return (uint16_t)region->descriptor;
}

uint64_t runtime_physical_memory_region_attributes(
    const RuntimePhysicalMemoryRegion *region
) {
    if (region == 0) {
        runtime_firebloom_panic();
    }
    return region->descriptor >> 16;
}

RUNTIME_PHYSICAL_MEMORY_RECOVERED
bool runtime_physical_memory_region_is_defined(RuntimeFirebloomSpan span) {
    RuntimePhysicalMemoryRegion *region = region_from_span(span);
    return region != 0 && region->physical_base != UINTPTR_MAX &&
        region->size != SIZE_MAX;
}

RUNTIME_PHYSICAL_MEMORY_RECOVERED
bool runtime_physical_memory_region_is_live(RuntimeFirebloomSpan span) {
    RuntimePhysicalMemoryRegion *region = region_from_span(span);
    return region != 0 && runtime_physical_memory_region_is_defined(span) &&
        region->physical_base != 0 && region->size != 0;
}

RUNTIME_PHYSICAL_MEMORY_RECOVERED
uintptr_t runtime_physical_memory_region_active_base(
    RuntimeFirebloomSpan span
) {
    RuntimePhysicalMemoryRegion *region = region_from_span(span);
    return region->active_alias == 0 ?
        region->physical_base : region->active_alias;
}

RUNTIME_PHYSICAL_MEMORY_RECOVERED
uintptr_t runtime_physical_memory_region_physical_upper_bound(
    RuntimeFirebloomSpan span
) {
    RuntimePhysicalMemoryRegion *region = region_from_span(span);
    if (!runtime_physical_memory_region_is_live(span)) {
        runtime_firebloom_panic();
    }
    return checked_upper_bound(region->physical_base, region->size);
}

RUNTIME_PHYSICAL_MEMORY_RECOVERED
uintptr_t runtime_physical_memory_region_active_upper_bound(
    RuntimeFirebloomSpan span
) {
    RuntimePhysicalMemoryRegion *region = region_from_span(span);
    if (!runtime_physical_memory_region_is_live(span)) {
        return 0;
    }
    return checked_upper_bound(
        runtime_physical_memory_region_active_base(span),
        region->size
    );
}

RUNTIME_PHYSICAL_MEMORY_RECOVERED
void runtime_physical_memory_region_zero(RuntimeFirebloomSpan span) {
    RuntimePhysicalMemoryRegion *region = region_from_span(span);
    if (!runtime_physical_memory_region_is_live(span)) {
        runtime_firebloom_panic();
    }
    const uintptr_t active = runtime_physical_memory_region_active_base(span);
    runtime_firebloom_fill_span(
        (RuntimeFirebloomSpan){
            .current = (void *)active,
            .lower_bound = (void *)active,
            .upper_bound = (void *)checked_upper_bound(active, region->size),
            .type = &region_metadata_type,
        },
        0,
        region->size
    );
}

const RuntimePhysicalMemoryRegistry *runtime_physical_memory_registry(void) {
    return &current_registry;
}
