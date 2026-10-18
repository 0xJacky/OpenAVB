#pragma once

#include "avp/runtime/memory/runtime_address_space.h"
#include "avp/runtime/ipc/runtime_domain.h"
#include "avp/runtime/memory/runtime_physical_memory.h"

#define RUNTIME_ADDRESS_SPACE_RECOVERED_ENTRY                                  \
  __attribute__((used, retain, minsize, noinline))

enum {
  RUNTIME_ADDRESS_SPACE_RECOVERED_BITMAP_BYTES = 80,
  RUNTIME_ADDRESS_SPACE_RECOVERED_MAPPING_CAPACITY = 32,
  RUNTIME_ADDRESS_SPACE_RECOVERED_DOMAIN_CAPACITY = 15,
  RUNTIME_ADDRESS_SPACE_RECOVERED_DOMAIN_WINDOW_SIZE = 0xf0000000,
};

typedef enum {
  RUNTIME_ADDRESS_SPACE_RECOVERED_OK = 0,
  RUNTIME_ADDRESS_SPACE_RECOVERED_INVALID_ARGUMENT = 1,
  RUNTIME_ADDRESS_SPACE_RECOVERED_NOT_FOUND = 2,
  RUNTIME_ADDRESS_SPACE_RECOVERED_PERMISSION = 3,
  RUNTIME_ADDRESS_SPACE_RECOVERED_CONFLICT = 4,
  RUNTIME_ADDRESS_SPACE_RECOVERED_PAGE_TABLE = 5,
  RUNTIME_ADDRESS_SPACE_RECOVERED_CAPACITY = 6,
} RuntimeAddressSpaceRecoveredStatus;

typedef struct {
  uint8_t *lower;
  uint8_t *current;
  uint8_t *upper;
  uint64_t type;
} RuntimeAddressSpaceRecoveredBitmapSpan;

typedef struct {
  uintptr_t lower;
  uintptr_t upper;
  uint16_t region_id;
  uint8_t policy_class;
  uint8_t excluded_set_mask;
  bool reserved;
} RuntimeAddressSpaceRecoveredPhysicalPolicy;

typedef struct {
  uint8_t domain_id;
  RuntimeDomainRegistryLink *link;
} RuntimeAddressSpaceRecoveredDomainLink;

typedef struct {
  RuntimeAddressSpaceRecoveredDomainLink *current;
  RuntimeAddressSpaceRecoveredDomainLink *lower;
  RuntimeAddressSpaceRecoveredDomainLink *upper;
  uint64_t type;
} RuntimeAddressSpaceRecoveredDomainLinkSlots;

typedef struct {
  uintptr_t virtual_address;
  uintptr_t physical_address;
  size_t size;
  uint8_t regime;
  uint8_t memory_attribute;
  uint32_t flags;
} RuntimeAddressSpaceRecoveredMappingArguments;

typedef struct {
  uintptr_t slide;
  uintptr_t domain_local_delta;
} RuntimeAddressSpaceRecoveredProfileDelta;

typedef struct {
  uintptr_t direct_mapping;
  uintptr_t owner_lower;
  uintptr_t owner_upper;
  uint64_t owner_type;
} RuntimeAddressSpaceRecoveredOptionalMappingState;

typedef struct {
  uint16_t mapping_handle;
  uintptr_t shared_virtual_base;
  uintptr_t shared_physical_base;
  uint8_t domain_id;
} RuntimeAddressSpaceRecoveredTask;

typedef struct {
  RuntimeAddressSpaceMapping mapping;
  uint16_t handle;
  uint8_t rights;
  bool active;
} RuntimeAddressSpaceRecoveredMappingSlot;

typedef struct {
  RuntimePageTable *page_table;
  const RuntimeAddressSpaceRecoveredPhysicalPolicy *physical_policies;
  size_t physical_policy_count;
  RuntimeAddressSpaceRecoveredDomainLink
      domains[RUNTIME_ADDRESS_SPACE_RECOVERED_DOMAIN_CAPACITY];
  size_t domain_count;
  RuntimeAddressSpaceRecoveredMappingSlot
      mappings[RUNTIME_ADDRESS_SPACE_RECOVERED_MAPPING_CAPACITY];
  uint8_t attached_region_bitmap[RUNTIME_ADDRESS_SPACE_RECOVERED_BITMAP_BYTES];
  uintptr_t profile_slide;
  uintptr_t mapped_allocation_lower;
  uintptr_t mapped_allocation_upper;
  uintptr_t domain_local_base;
  uint64_t domain_link_type;
  RuntimeAddressSpaceRecoveredStatus last_status;
  uint32_t fatal_line;
  uint8_t current_domain;
  bool profile_direct_map;
} RuntimeAddressSpaceRecoveredContext;

void runtime_address_space_recovered_bind_context(
    RuntimeAddressSpaceRecoveredContext *context);
RuntimeAddressSpaceRecoveredContext *
runtime_address_space_recovered_context(void);

RUNTIME_ADDRESS_SPACE_RECOVERED_ENTRY RuntimeAddressSpaceRecoveredBitmapSpan
runtime_address_space_attached_region_bitmap_span(void);
RUNTIME_ADDRESS_SPACE_RECOVERED_ENTRY bool
runtime_address_space_uses_profile_direct_map(void);
RUNTIME_ADDRESS_SPACE_RECOVERED_ENTRY RuntimeAddressSpaceMapping *
runtime_address_space_mapping_resolve_object_id(uint16_t object_id);
RUNTIME_ADDRESS_SPACE_RECOVERED_ENTRY _Noreturn void
runtime_address_space_mapping_audience_bounds_panic(void);
RUNTIME_ADDRESS_SPACE_RECOVERED_ENTRY RuntimeAddressSpaceRecoveredStatus
runtime_address_space_unmap(
  uint16_t mapping_handle,
  uint8_t domain_id,
  uint8_t mode
);
RUNTIME_ADDRESS_SPACE_RECOVERED_ENTRY _Noreturn void
runtime_address_space_line_10018_fatal(void);
RUNTIME_ADDRESS_SPACE_RECOVERED_ENTRY RuntimeAddressSpaceRecoveredStatus
runtime_address_space_unmap_current_range(
  uintptr_t virtual_address,
  size_t size
);
RUNTIME_ADDRESS_SPACE_RECOVERED_ENTRY RuntimeAddressSpaceMapResult
runtime_address_space_attach_mapping_to_domain(
  uint16_t mapping_handle,
  uint8_t domain_id,
  size_t offset,
  size_t size,
  uint8_t rights
);
RUNTIME_ADDRESS_SPACE_RECOVERED_ENTRY RuntimeAddressSpaceRecoveredStatus
runtime_address_space_detach_mapping_from_domain(
  uint16_t mapping_handle,
  uint8_t domain_id
);
RUNTIME_ADDRESS_SPACE_RECOVERED_ENTRY uintptr_t
runtime_task_ensure_shared_page_mapping(
  RuntimeAddressSpaceRecoveredTask *task,
  uint8_t memory_attribute,
  uint8_t mapping_attribute
);

RUNTIME_ADDRESS_SPACE_RECOVERED_ENTRY bool
runtime_address_space_physical_range_is_global_direct(
  uintptr_t physical_address,
  size_t size,
  uint16_t region_id
);
RUNTIME_ADDRESS_SPACE_RECOVERED_ENTRY RuntimeAddressSpaceMapResult
runtime_address_space_map_global_direct_range(
  uintptr_t physical_address,
  size_t size,
  uint16_t region_id,
  uint8_t memory_attribute,
  uint8_t mapping_attribute,
  uint8_t rights
);
RUNTIME_ADDRESS_SPACE_RECOVERED_ENTRY bool
runtime_address_space_address_is_domain_local(uintptr_t address);
RUNTIME_ADDRESS_SPACE_RECOVERED_ENTRY bool
runtime_address_space_address_in_mapped_allocation_window(uintptr_t address);
RUNTIME_ADDRESS_SPACE_RECOVERED_ENTRY RuntimeAddressSpaceMapResult
runtime_address_space_map_and_publish_subrange(
  uint16_t mapping_handle,
  size_t offset,
  size_t size,
  uint8_t domain_id,
  uint8_t rights
);

RUNTIME_ADDRESS_SPACE_RECOVERED_ENTRY bool
runtime_address_profile_outlined_finish_domain_local_window_test(
  uintptr_t address,
  uintptr_t window_base
);
RUNTIME_ADDRESS_SPACE_RECOVERED_ENTRY
RuntimeAddressSpaceRecoveredProfileDelta
runtime_address_profile_outlined_load_slide_and_domain_local_delta(void);
RUNTIME_ADDRESS_SPACE_RECOVERED_ENTRY uintptr_t
runtime_address_space_outlined_compute_domain_local_window_base(
    uint8_t domain_id);
RUNTIME_ADDRESS_SPACE_RECOVERED_ENTRY
RuntimeAddressSpaceRecoveredMappingArguments
runtime_address_space_outlined_prepare_mapping_arguments(
  uintptr_t virtual_address,
  uintptr_t physical_address,
  size_t size,
  uint8_t regime,
  uint8_t memory_attribute,
  uint32_t flags
);
RUNTIME_ADDRESS_SPACE_RECOVERED_ENTRY uintptr_t
runtime_address_profile_load_slide_to_x8(void);
RUNTIME_ADDRESS_SPACE_RECOVERED_ENTRY
RuntimeAddressSpaceRecoveredDomainLinkSlots
runtime_address_space_outlined_load_current_domain_link_slots(void);
RUNTIME_ADDRESS_SPACE_RECOVERED_ENTRY RuntimeDomainRegistryLink *
runtime_address_space_outlined_resolve_domain_link_x26(
    const RuntimeAddressSpaceRecoveredDomainLink *link);
RUNTIME_ADDRESS_SPACE_RECOVERED_ENTRY RuntimeAddressSpaceRecoveredBitmapSpan
runtime_address_space_outlined_prepare_attached_region_bitmap_arguments(
    uint16_t region_id);
RUNTIME_ADDRESS_SPACE_RECOVERED_ENTRY
RuntimeAddressSpaceRecoveredOptionalMappingState
runtime_address_space_outlined_clear_optional_mapping_state(void);
RUNTIME_ADDRESS_SPACE_RECOVERED_ENTRY bool
runtime_address_space_outlined_validate_saved_region_mapping(
  RuntimeFirebloomSpan region,
  const RuntimeAddressSpaceMapping *mapping,
  uint8_t domain_id
);
