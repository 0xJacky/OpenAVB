#pragma once

#include "avp/runtime/memory/runtime_physical_memory.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if __has_attribute(retain)
#define RUNTIME_PHYSICAL_MEMORY_RECOVERED_PUBLIC \
  __attribute__((used, retain))
#else
#define RUNTIME_PHYSICAL_MEMORY_RECOVERED_PUBLIC __attribute__((used))
#endif

#define RUNTIME_PHYSICAL_MEMORY_PAGE_SIZE ((size_t)0x4000)
#define RUNTIME_PHYSICAL_MEMORY_POLICY_CLASS_COUNT \
  ((size_t)RUNTIME_PHYSICAL_REGION_ID_MAX + 1)
#define RUNTIME_PHYSICAL_MEMORY_RECORD_LIMIT ((size_t)128)
#define RUNTIME_PHYSICAL_MEMORY_MAPPING_ATTRIBUTE_MIN UINT32_C(24)

typedef struct {
  uint8_t *current;
  uint8_t *lower_bound;
  uint8_t *upper_bound;
} RuntimePhysicalMemoryByteView;

typedef struct {
  const uint8_t *current;
  const uint8_t *lower_bound;
  const uint8_t *upper_bound;
} RuntimePhysicalMemoryConstByteView;

typedef struct {
  uint16_t *current;
  uint16_t *lower_bound;
  uint16_t *upper_bound;
} RuntimePhysicalMemoryCountView;

typedef struct {
  RuntimePhysicalMemoryRegion *current;
  RuntimePhysicalMemoryRegion *lower_bound;
  RuntimePhysicalMemoryRegion *upper_bound;
} RuntimePhysicalMemoryRegionView;

typedef struct {
  uintptr_t current;
  uintptr_t lower_bound;
  uintptr_t upper_bound;
} RuntimePhysicalMemoryAliasView;

typedef uintptr_t (*RuntimePhysicalMemoryRecoveredMapAlias)(
  uint16_t region_id,
  uintptr_t physical_base,
  size_t size,
  uint32_t mapping_attribute,
  uint32_t flags,
  bool supervisor_services,
  void *context
);
typedef void (*RuntimePhysicalMemoryRecoveredUnmapAlias)(
  uint16_t region_id,
  uintptr_t active_alias,
  size_t size,
  uint32_t flags,
  bool supervisor_services,
  void *context
);

typedef struct {
  const uint8_t *policy_classes;
  uint16_t policy_class_count;
  uint8_t policy_class_limit;
  bool uses_supervisor_services;
  RuntimePhysicalMemoryRecoveredMapAlias map_alias;
  RuntimePhysicalMemoryRecoveredUnmapAlias unmap_alias;
  void *context;
} RuntimePhysicalMemoryRecoveredConfiguration;

void runtime_physical_memory_recovered_configure(
    const RuntimePhysicalMemoryRecoveredConfiguration *configuration);

RUNTIME_PHYSICAL_MEMORY_RECOVERED_PUBLIC bool
runtime_physical_memory_region_is_16k_aligned(RuntimeFirebloomSpan region);
RUNTIME_PHYSICAL_MEMORY_RECOVERED_PUBLIC _Noreturn void
runtime_physical_memory_region_alignment_bounds_panic(void);
RUNTIME_PHYSICAL_MEMORY_RECOVERED_PUBLIC bool
j_runtime_physical_memory_region_is_live(RuntimeFirebloomSpan region);
RUNTIME_PHYSICAL_MEMORY_RECOVERED_PUBLIC bool
j_runtime_physical_memory_region_is_live_clone(RuntimeFirebloomSpan region);
RUNTIME_PHYSICAL_MEMORY_RECOVERED_PUBLIC uint8_t
runtime_physical_memory_region_policy_class(uint16_t region_id);
RUNTIME_PHYSICAL_MEMORY_RECOVERED_PUBLIC _Noreturn void
runtime_physical_memory_active_base_bounds_panic(void);
RUNTIME_PHYSICAL_MEMORY_RECOVERED_PUBLIC void
runtime_physical_memory_region_deactivate_alias(
  RuntimeFirebloomSpan region,
  uint32_t flags
);
RUNTIME_PHYSICAL_MEMORY_RECOVERED_PUBLIC RuntimePhysicalMemoryAliasView
runtime_physical_memory_region_activate_alias(
  RuntimeFirebloomSpan region,
  uint32_t mapping_attribute
);
RUNTIME_PHYSICAL_MEMORY_RECOVERED_PUBLIC RuntimePhysicalMemoryAliasView
runtime_physical_memory_region_activate_alias_with_flags(
  RuntimeFirebloomSpan region,
  uint32_t mapping_attribute,
  uint32_t flags
);
RUNTIME_PHYSICAL_MEMORY_RECOVERED_PUBLIC void
runtime_physical_memory_initialize_fixed_registry_view(
  RuntimePhysicalMemoryRegionView *view,
  uint32_t *count
);
RUNTIME_PHYSICAL_MEMORY_RECOVERED_PUBLIC RuntimePhysicalMemoryCountView
runtime_physical_memory_region_count_span(void);
RUNTIME_PHYSICAL_MEMORY_RECOVERED_PUBLIC RuntimePhysicalMemoryConstByteView
runtime_physical_memory_region_policy_class_limit_span(void);
RUNTIME_PHYSICAL_MEMORY_RECOVERED_PUBLIC RuntimePhysicalMemoryConstByteView
runtime_physical_memory_region_policy_class_table_span(void);
RUNTIME_PHYSICAL_MEMORY_RECOVERED_PUBLIC uintptr_t
runtime_physical_memory_apply_region_records(
  const RuntimePhysicalMemoryRegion *records,
  size_t byte_size
);
RUNTIME_PHYSICAL_MEMORY_RECOVERED_PUBLIC RuntimePhysicalMemoryByteView
runtime_physical_memory_regions_initialized_flag_bounded_view(void);
RUNTIME_PHYSICAL_MEMORY_RECOVERED_PUBLIC bool
runtime_physical_memory_uses_supervisor_services(void);
RUNTIME_PHYSICAL_MEMORY_RECOVERED_PUBLIC RuntimePhysicalMemoryRegionView
runtime_physical_memory_region_bounds_from_x23(
    RuntimePhysicalMemoryRegion *region);
RUNTIME_PHYSICAL_MEMORY_RECOVERED_PUBLIC _Noreturn void
runtime_physical_memory_region_is_defined_bounds_panic(void);
RUNTIME_PHYSICAL_MEMORY_RECOVERED_PUBLIC RuntimeFirebloomSpan
runtime_physical_memory_outlined_restore_region_tuple_x22_x19(
    RuntimeFirebloomSpan region);
RUNTIME_PHYSICAL_MEMORY_RECOVERED_PUBLIC RuntimeFirebloomSpan
runtime_physical_memory_outlined_save_region_tuple_x19_x21_x20(
    RuntimeFirebloomSpan region);
RUNTIME_PHYSICAL_MEMORY_RECOVERED_PUBLIC bool
runtime_physical_memory_outlined_save_region_tuple_and_test_live(
    RuntimeFirebloomSpan region);
