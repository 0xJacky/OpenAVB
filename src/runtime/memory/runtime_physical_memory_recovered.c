#include "avp/runtime/memory/runtime_physical_memory_recovered.h"

#include <limits.h>

#define RUNTIME_PHYSICAL_MEMORY_ALIAS_DESCRIPTOR_SHIFT UINT32_C(16)
#define RUNTIME_PHYSICAL_MEMORY_ALIAS_DESCRIPTOR_MASK \
  (UINT64_C(0xffff) << RUNTIME_PHYSICAL_MEMORY_ALIAS_DESCRIPTOR_SHIFT)
#define RUNTIME_PHYSICAL_MEMORY_RECORD_SKIP_BIT \
  (UINT64_C(1) << UINT32_C(32))
#define RUNTIME_PHYSICAL_MEMORY_RECORD_VALID_BIT \
  (UINT64_C(1) << UINT32_C(47))

static uint16_t recovered_policy_class_count;
static uint8_t recovered_policy_class_limit;
static uint8_t recovered_regions_initialized;
static RuntimePhysicalMemoryRecoveredConfiguration recovered_configuration;

static _Noreturn void runtime_physical_memory_recovered_trap(void) {
  __builtin_trap();
  __builtin_unreachable();
}

static bool runtime_physical_memory_address_in_bounds(
    uintptr_t current,
    uintptr_t lower_bound,
    uintptr_t upper_bound,
    size_t size
) {
  return current != 0 && lower_bound != 0 && upper_bound != 0 &&
         current >= lower_bound && current < upper_bound &&
         size <= upper_bound - current;
}

static RuntimePhysicalMemoryRegion *runtime_physical_memory_region_from_span(
    RuntimeFirebloomSpan span) {
  if (!runtime_physical_memory_address_in_bounds(
          (uintptr_t)span.current, (uintptr_t)span.lower_bound,
          (uintptr_t)span.upper_bound, 0x20)) {
    runtime_physical_memory_recovered_trap();
  }
  return span.current;
}

static uintptr_t runtime_physical_memory_checked_upper_bound(
    uintptr_t base,
    size_t size
) {
  if (size > UINTPTR_MAX - base) {
    runtime_physical_memory_recovered_trap();
  }
  return base + size;
}

static void runtime_physical_memory_set_mapping_attribute(
    RuntimePhysicalMemoryRegion *region,
    uint32_t mapping_attribute
) {
  region->descriptor =
      (region->descriptor & ~RUNTIME_PHYSICAL_MEMORY_ALIAS_DESCRIPTOR_MASK) |
      ((uint64_t)(uint16_t)mapping_attribute
       << RUNTIME_PHYSICAL_MEMORY_ALIAS_DESCRIPTOR_SHIFT);
}

void runtime_physical_memory_recovered_configure(
    const RuntimePhysicalMemoryRecoveredConfiguration *configuration) {
  if (configuration == NULL || configuration->policy_class_count == 0 ||
      configuration->policy_class_count >
          RUNTIME_PHYSICAL_MEMORY_POLICY_CLASS_COUNT ||
      configuration->policy_classes == NULL) {
    runtime_physical_memory_recovered_trap();
  }
  for (size_t index = 0; index < configuration->policy_class_count; ++index) {
    if (configuration->policy_classes[index] >
        configuration->policy_class_limit) {
      runtime_physical_memory_recovered_trap();
    }
  }
  recovered_policy_class_count = configuration->policy_class_count;
  recovered_policy_class_limit = configuration->policy_class_limit;
  recovered_regions_initialized = 0;
  recovered_configuration = *configuration;
}

bool RUNTIME_PHYSICAL_MEMORY_RECOVERED_PUBLIC
runtime_physical_memory_region_is_16k_aligned(RuntimeFirebloomSpan span) {
  RuntimePhysicalMemoryRegion *region =
      runtime_physical_memory_region_from_span(span);
  return (region->physical_base &
          (RUNTIME_PHYSICAL_MEMORY_PAGE_SIZE - 1)) == 0 &&
         (region->size & (RUNTIME_PHYSICAL_MEMORY_PAGE_SIZE - 1)) == 0;
}

_Noreturn void RUNTIME_PHYSICAL_MEMORY_RECOVERED_PUBLIC
runtime_physical_memory_region_alignment_bounds_panic(void) {
  runtime_physical_memory_recovered_trap();
}

bool RUNTIME_PHYSICAL_MEMORY_RECOVERED_PUBLIC
j_runtime_physical_memory_region_is_live(RuntimeFirebloomSpan region) {
  return runtime_physical_memory_region_is_live(region);
}

bool RUNTIME_PHYSICAL_MEMORY_RECOVERED_PUBLIC
j_runtime_physical_memory_region_is_live_clone(RuntimeFirebloomSpan region) {
  return runtime_physical_memory_region_is_live(region);
}

uint8_t RUNTIME_PHYSICAL_MEMORY_RECOVERED_PUBLIC
runtime_physical_memory_region_policy_class(uint16_t region_id) {
  if (region_id >= recovered_policy_class_count) {
    runtime_physical_memory_recovered_trap();
  }
  const uint8_t policy_class = recovered_configuration.policy_classes[region_id];
  if (policy_class > recovered_policy_class_limit) {
    runtime_physical_memory_recovered_trap();
  }
  return policy_class;
}

_Noreturn void RUNTIME_PHYSICAL_MEMORY_RECOVERED_PUBLIC
runtime_physical_memory_active_base_bounds_panic(void) {
  runtime_physical_memory_recovered_trap();
}

void RUNTIME_PHYSICAL_MEMORY_RECOVERED_PUBLIC
runtime_physical_memory_region_deactivate_alias(
    RuntimeFirebloomSpan span,
    uint32_t flags
) {
  RuntimePhysicalMemoryRegion *region =
      runtime_physical_memory_region_from_span(span);
  if (region->active_alias == 0) {
    return;
  }
  if (recovered_configuration.unmap_alias != NULL) {
    recovered_configuration.unmap_alias(
        runtime_physical_memory_region_id(region),
        region->active_alias,
        region->size,
        flags,
        recovered_configuration.uses_supervisor_services,
        recovered_configuration.context
    );
  }
  region->active_alias = 0;
  runtime_physical_memory_set_mapping_attribute(region, 0);
}

RuntimePhysicalMemoryAliasView RUNTIME_PHYSICAL_MEMORY_RECOVERED_PUBLIC
runtime_physical_memory_region_activate_alias(
    RuntimeFirebloomSpan region,
    uint32_t mapping_attribute
) {
  return runtime_physical_memory_region_activate_alias_with_flags(
      region,
      mapping_attribute,
      0
  );
}

RuntimePhysicalMemoryAliasView RUNTIME_PHYSICAL_MEMORY_RECOVERED_PUBLIC
runtime_physical_memory_region_activate_alias_with_flags(
    RuntimeFirebloomSpan span,
    uint32_t mapping_attribute,
    uint32_t flags
) {
  RuntimePhysicalMemoryRegion *region =
      runtime_physical_memory_region_from_span(span);
  if (!runtime_physical_memory_region_is_live(span) ||
      mapping_attribute < RUNTIME_PHYSICAL_MEMORY_MAPPING_ATTRIBUTE_MIN ||
      mapping_attribute > UINT16_MAX) {
    runtime_physical_memory_recovered_trap();
  }
  if (region->active_alias != 0) {
    runtime_physical_memory_region_deactivate_alias(span, flags);
  }

  uintptr_t active_alias = region->physical_base;
  if (recovered_configuration.map_alias != NULL) {
    active_alias = recovered_configuration.map_alias(
        runtime_physical_memory_region_id(region),
        region->physical_base,
        region->size,
        mapping_attribute,
        flags,
        recovered_configuration.uses_supervisor_services,
        recovered_configuration.context
    );
  }
  if (active_alias == 0) {
    runtime_physical_memory_recovered_trap();
  }
  const uintptr_t upper_bound = runtime_physical_memory_checked_upper_bound(
      active_alias,
      region->size
  );
  region->active_alias = active_alias;
  runtime_physical_memory_set_mapping_attribute(region, mapping_attribute);
  return (RuntimePhysicalMemoryAliasView){
      .current = active_alias,
      .lower_bound = active_alias,
      .upper_bound = upper_bound,
  };
}

void RUNTIME_PHYSICAL_MEMORY_RECOVERED_PUBLIC
runtime_physical_memory_initialize_fixed_registry_view(
    RuntimePhysicalMemoryRegionView *view,
    uint32_t *count
) {
  if (view == NULL || count == NULL) {
    runtime_physical_memory_recovered_trap();
  }
  const RuntimePhysicalMemoryRegistry *registry =
      runtime_physical_memory_registry();
  if (registry == NULL || registry->count > UINT32_MAX ||
      (registry->count != 0 && registry->records == NULL)) {
    runtime_physical_memory_recovered_trap();
  }
  *view = (RuntimePhysicalMemoryRegionView){
      .current = registry->records,
      .lower_bound = registry->records,
      .upper_bound = registry->records + registry->count,
  };
  *count = (uint32_t)registry->count;
}

RuntimePhysicalMemoryCountView RUNTIME_PHYSICAL_MEMORY_RECOVERED_PUBLIC
runtime_physical_memory_region_count_span(void) {
  return (RuntimePhysicalMemoryCountView){
      .current = &recovered_policy_class_count,
      .lower_bound = &recovered_policy_class_count,
      .upper_bound = &recovered_policy_class_count + 1,
  };
}

RuntimePhysicalMemoryConstByteView RUNTIME_PHYSICAL_MEMORY_RECOVERED_PUBLIC
runtime_physical_memory_region_policy_class_limit_span(void) {
  return (RuntimePhysicalMemoryConstByteView){
      .current = &recovered_policy_class_limit,
      .lower_bound = &recovered_policy_class_limit,
      .upper_bound = &recovered_policy_class_limit + 1,
  };
}

RuntimePhysicalMemoryConstByteView RUNTIME_PHYSICAL_MEMORY_RECOVERED_PUBLIC
runtime_physical_memory_region_policy_class_table_span(void) {
  return (RuntimePhysicalMemoryConstByteView){
      .current = recovered_configuration.policy_classes,
      .lower_bound = recovered_configuration.policy_classes,
      .upper_bound = recovered_configuration.policy_classes +
                     recovered_policy_class_count,
  };
}

uintptr_t RUNTIME_PHYSICAL_MEMORY_RECOVERED_PUBLIC
runtime_physical_memory_apply_region_records(
    const RuntimePhysicalMemoryRegion *records,
    size_t byte_size
) {
  if (records == NULL || byte_size == 0 ||
      byte_size % sizeof(*records) != 0 ||
      byte_size / sizeof(*records) > RUNTIME_PHYSICAL_MEMORY_RECORD_LIMIT ||
      recovered_regions_initialized != 0) {
    runtime_physical_memory_recovered_trap();
  }

  const size_t count = byte_size / sizeof(*records);
  uintptr_t last_physical_base = UINTPTR_MAX;
  for (size_t index = 0; index < count; ++index) {
    const RuntimePhysicalMemoryRegion *source = &records[index];
    if ((source->descriptor & RUNTIME_PHYSICAL_MEMORY_RECORD_VALID_BIT) == 0) {
      runtime_physical_memory_recovered_trap();
    }
    if ((source->descriptor & RUNTIME_PHYSICAL_MEMORY_RECORD_SKIP_BIT) != 0) {
      continue;
    }
    const uint16_t region_id = runtime_physical_memory_region_id(source);
    (void)runtime_physical_memory_region_policy_class(region_id);
    RuntimeFirebloomSpan target_span =
        runtime_physical_memory_region_lookup(region_id);
    if (target_span.current == NULL || source->physical_base == UINTPTR_MAX ||
        source->size == SIZE_MAX ||
        source->size > UINTPTR_MAX - source->physical_base) {
      runtime_physical_memory_recovered_trap();
    }

    RuntimePhysicalMemoryRegion *target =
        runtime_physical_memory_region_from_span(target_span);
    const RuntimeFirebloomSpan metadata = target->metadata;
    target->physical_base = source->physical_base;
    target->active_alias = 0;
    target->size = source->size;
    target->descriptor = source->descriptor;
    target->metadata = metadata;

    const uint32_t mapping_attribute =
        (uint32_t)((source->descriptor >>
                    RUNTIME_PHYSICAL_MEMORY_ALIAS_DESCRIPTOR_SHIFT) &
                   UINT16_MAX);
    if (source->physical_base != 0 && source->size != 0 &&
        mapping_attribute >= RUNTIME_PHYSICAL_MEMORY_MAPPING_ATTRIBUTE_MIN) {
      (void)runtime_physical_memory_region_activate_alias(
          target_span,
          mapping_attribute
      );
    }
    last_physical_base = source->physical_base;
  }
  recovered_regions_initialized = 1;
  return last_physical_base;
}

RuntimePhysicalMemoryByteView RUNTIME_PHYSICAL_MEMORY_RECOVERED_PUBLIC
runtime_physical_memory_regions_initialized_flag_bounded_view(void) {
  return (RuntimePhysicalMemoryByteView){
      .current = &recovered_regions_initialized,
      .lower_bound = &recovered_regions_initialized,
      .upper_bound = &recovered_regions_initialized + 1,
  };
}

bool RUNTIME_PHYSICAL_MEMORY_RECOVERED_PUBLIC
runtime_physical_memory_uses_supervisor_services(void) {
  return recovered_configuration.uses_supervisor_services;
}

RuntimePhysicalMemoryRegionView RUNTIME_PHYSICAL_MEMORY_RECOVERED_PUBLIC
runtime_physical_memory_region_bounds_from_x23(
    RuntimePhysicalMemoryRegion *region) {
  if (region == NULL) {
    runtime_physical_memory_recovered_trap();
  }
  return (RuntimePhysicalMemoryRegionView){
      .current = region,
      .lower_bound = region,
      .upper_bound = (RuntimePhysicalMemoryRegion *)((uint8_t *)region + 0x20),
  };
}

_Noreturn void RUNTIME_PHYSICAL_MEMORY_RECOVERED_PUBLIC
runtime_physical_memory_region_is_defined_bounds_panic(void) {
  runtime_physical_memory_recovered_trap();
}

RuntimeFirebloomSpan RUNTIME_PHYSICAL_MEMORY_RECOVERED_PUBLIC
runtime_physical_memory_outlined_restore_region_tuple_x22_x19(
    RuntimeFirebloomSpan region) {
  return region;
}

RuntimeFirebloomSpan RUNTIME_PHYSICAL_MEMORY_RECOVERED_PUBLIC
runtime_physical_memory_outlined_save_region_tuple_x19_x21_x20(
    RuntimeFirebloomSpan region) {
  return region;
}

bool RUNTIME_PHYSICAL_MEMORY_RECOVERED_PUBLIC
runtime_physical_memory_outlined_save_region_tuple_and_test_live(
    RuntimeFirebloomSpan region) {
  return runtime_physical_memory_region_is_live(region);
}
