#pragma once

#include "avp/runtime/firebloom/runtime_firebloom.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


#define RUNTIME_PHYSICAL_REGION_ID_MAX UINT16_C(0x027d)
#define RUNTIME_PHYSICAL_REGION_COUNT_MATCHING ((size_t)11)

typedef struct {
    uintptr_t physical_base;
    uintptr_t active_alias;
    size_t size;
    uint64_t descriptor;
    RuntimeFirebloomSpan metadata;
} RuntimePhysicalMemoryRegion;

_Static_assert(
    offsetof(RuntimePhysicalMemoryRegion, descriptor) == 0x18,
    "RuntimePhysicalMemoryRegion descriptor offset"
);
_Static_assert(
    offsetof(RuntimePhysicalMemoryRegion, metadata) == 0x20,
    "RuntimePhysicalMemoryRegion metadata offset"
);
_Static_assert(
    sizeof(RuntimePhysicalMemoryRegion) == 0x40,
    "RuntimePhysicalMemoryRegion size"
);

typedef struct {
    RuntimePhysicalMemoryRegion *records;
    size_t count;
} RuntimePhysicalMemoryRegistry;

void runtime_physical_memory_registry_initialize(
    RuntimePhysicalMemoryRegistry registry
);
void runtime_physical_memory_registry_initialize_matching(void);
RuntimeFirebloomSpan runtime_physical_memory_region_lookup(
    uint16_t region_id
);
uint16_t runtime_physical_memory_region_id(
    const RuntimePhysicalMemoryRegion *region
);
uint64_t runtime_physical_memory_region_attributes(
    const RuntimePhysicalMemoryRegion *region
);
bool runtime_physical_memory_region_is_defined(RuntimeFirebloomSpan region);
bool runtime_physical_memory_region_is_live(RuntimeFirebloomSpan region);
uintptr_t runtime_physical_memory_region_active_base(
    RuntimeFirebloomSpan region
);
uintptr_t runtime_physical_memory_region_physical_upper_bound(
    RuntimeFirebloomSpan region
);
uintptr_t runtime_physical_memory_region_active_upper_bound(
    RuntimeFirebloomSpan region
);
void runtime_physical_memory_region_zero(RuntimeFirebloomSpan region);
const RuntimePhysicalMemoryRegistry *runtime_physical_memory_registry(void);
