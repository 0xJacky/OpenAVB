#include "runtime_address_space_recovered_private.h"

#if defined(__ELF__)
#define RUNTIME_ADDRESS_SPACE_RECOVERED_STATE                                  \
  __attribute__((section(".runtime_task.state")))
#else
#define RUNTIME_ADDRESS_SPACE_RECOVERED_STATE
#endif

static RuntimeAddressSpaceRecoveredContext *recovered_context
    RUNTIME_ADDRESS_SPACE_RECOVERED_STATE;

void runtime_address_space_recovered_bind_context(
    RuntimeAddressSpaceRecoveredContext *context) {
  recovered_context = context;
}

RuntimeAddressSpaceRecoveredContext *
runtime_address_space_recovered_context(void) {
  return recovered_context;
}

RuntimeAddressSpaceRecoveredStatus runtime_address_space_recovered_status(
    RuntimeAddressSpaceRecoveredStatus status) {
  if (recovered_context != 0) {
    recovered_context->last_status = status;
  }
  return status;
}

_Noreturn void runtime_address_space_recovered_fatal(uint32_t line) {
  if (recovered_context != 0) {
    recovered_context->fatal_line = line;
    recovered_context->last_status =
        RUNTIME_ADDRESS_SPACE_RECOVERED_INVALID_ARGUMENT;
  }
  __builtin_trap();
}

bool runtime_address_space_recovered_range_valid(
    uintptr_t address,
    size_t size
) {
  return size >= RUNTIME_ADDRESS_SPACE_PAGE_SIZE &&
         (address & (RUNTIME_ADDRESS_SPACE_PAGE_SIZE - 1)) == 0 &&
         (size & (RUNTIME_ADDRESS_SPACE_PAGE_SIZE - 1)) == 0 &&
         address <= UINTPTR_MAX - size;
}

RuntimeAddressSpaceRecoveredMappingSlot *
runtime_address_space_recovered_slot(uint16_t mapping_handle) {
  if (recovered_context == 0 || mapping_handle == 0) {
    return 0;
  }
  for (size_t index = 0;
       index < RUNTIME_ADDRESS_SPACE_RECOVERED_MAPPING_CAPACITY; ++index) {
    RuntimeAddressSpaceRecoveredMappingSlot *slot =
        &recovered_context->mappings[index];
    if (slot->active && slot->handle == mapping_handle) {
      return slot;
    }
  }
  return 0;
}

RuntimeAddressSpaceRecoveredMappingSlot *
runtime_address_space_recovered_allocate_slot(void) {
  if (recovered_context == 0) {
    return 0;
  }
  for (size_t index = 0;
       index < RUNTIME_ADDRESS_SPACE_RECOVERED_MAPPING_CAPACITY; ++index) {
    if (!recovered_context->mappings[index].active) {
      return &recovered_context->mappings[index];
    }
  }
  return 0;
}

RuntimeAddressSpaceRecoveredBitmapSpan
runtime_address_space_attached_region_bitmap_span(void) {
  if (recovered_context == 0) {
    return (RuntimeAddressSpaceRecoveredBitmapSpan){0};
  }
  return (RuntimeAddressSpaceRecoveredBitmapSpan){
      .lower = recovered_context->attached_region_bitmap,
      .current = recovered_context->attached_region_bitmap,
      .upper = recovered_context->attached_region_bitmap +
               RUNTIME_ADDRESS_SPACE_RECOVERED_BITMAP_BYTES,
      .type = UINT64_C(0x14a860),
  };
}

bool runtime_address_space_uses_profile_direct_map(void) {
  return recovered_context != 0 && recovered_context->profile_direct_map;
}

_Noreturn void runtime_address_space_mapping_audience_bounds_panic(void) {
  runtime_address_space_recovered_fatal(UINT32_C(0x20eec));
}

_Noreturn void runtime_address_space_line_10018_fatal(void) {
  runtime_address_space_recovered_fatal(UINT32_C(10018));
}

uintptr_t runtime_address_profile_load_slide_to_x8(void) {
  return recovered_context == 0 ? 0 : recovered_context->profile_slide;
}

RuntimeAddressSpaceRecoveredProfileDelta
runtime_address_profile_outlined_load_slide_and_domain_local_delta(void) {
  return (RuntimeAddressSpaceRecoveredProfileDelta){
      .slide = runtime_address_profile_load_slide_to_x8(),
      .domain_local_delta = UINT64_C(0xfffffcf000000000),
  };
}

uintptr_t runtime_address_space_outlined_compute_domain_local_window_base(
    uint8_t domain_id) {
  if (recovered_context == 0 || domain_id == 0 ||
      domain_id > RUNTIME_DOMAIN_ID_MAX) {
    runtime_address_space_recovered_status(
        RUNTIME_ADDRESS_SPACE_RECOVERED_INVALID_ARGUMENT);
    return 0;
  }
  return recovered_context->domain_local_base | ((uintptr_t)domain_id << 32);
}

bool runtime_address_profile_outlined_finish_domain_local_window_test(
    uintptr_t address,
    uintptr_t window_base
) {
  return address >= window_base &&
         address - window_base <
             RUNTIME_ADDRESS_SPACE_RECOVERED_DOMAIN_WINDOW_SIZE;
}

bool runtime_address_space_address_in_mapped_allocation_window(
    uintptr_t address) {
  if (recovered_context == 0) {
    return false;
  }
  /* Alternate profiles slide both bounds together, preserving half-open size.
   */
  const uintptr_t lower = recovered_context->mapped_allocation_lower +
                          recovered_context->profile_slide;
  const uintptr_t upper = recovered_context->mapped_allocation_upper +
                          recovered_context->profile_slide;
  return address >= lower && address < upper;
}

bool runtime_address_space_address_is_domain_local(uintptr_t address) {
  if (runtime_address_space_address_in_mapped_allocation_window(address)) {
    return true;
  }
  if (recovered_context == 0 || recovered_context->current_domain == 0) {
    return false;
  }
  const uintptr_t base =
      runtime_address_space_outlined_compute_domain_local_window_base(
          recovered_context->current_domain);
  return runtime_address_profile_outlined_finish_domain_local_window_test(
      address,
      base
  );
}

RuntimeAddressSpaceRecoveredMappingArguments
runtime_address_space_outlined_prepare_mapping_arguments(
    uintptr_t virtual_address,
    uintptr_t physical_address,
    size_t size,
    uint8_t regime,
    uint8_t memory_attribute,
    uint32_t flags
) {
  return (RuntimeAddressSpaceRecoveredMappingArguments){
      virtual_address, physical_address, size, regime, memory_attribute, flags,
  };
}

RuntimeAddressSpaceRecoveredDomainLinkSlots
runtime_address_space_outlined_load_current_domain_link_slots(void) {
  if (recovered_context == 0) {
    return (RuntimeAddressSpaceRecoveredDomainLinkSlots){0};
  }
  RuntimeAddressSpaceRecoveredDomainLink *current = 0;
  for (size_t index = 0; index < recovered_context->domain_count; ++index) {
    if (recovered_context->domains[index].domain_id ==
        recovered_context->current_domain) {
      current = &recovered_context->domains[index];
      break;
    }
  }
  return (RuntimeAddressSpaceRecoveredDomainLinkSlots){
      .current = current,
      .lower = recovered_context->domains,
      .upper = recovered_context->domains + recovered_context->domain_count,
      .type = recovered_context->domain_link_type,
  };
}

RuntimeDomainRegistryLink *
runtime_address_space_outlined_resolve_domain_link_x26(
    const RuntimeAddressSpaceRecoveredDomainLink *link) {
  if (recovered_context == 0 || link == 0 ||
      link < recovered_context->domains ||
      link >= recovered_context->domains + recovered_context->domain_count ||
      link->link == 0) {
    runtime_address_space_recovered_status(
        RUNTIME_ADDRESS_SPACE_RECOVERED_NOT_FOUND);
    return 0;
  }
  return link->link;
}

RuntimeAddressSpaceRecoveredBitmapSpan
runtime_address_space_outlined_prepare_attached_region_bitmap_arguments(
    uint16_t region_id) {
  if (region_id == 0 || region_id > RUNTIME_PHYSICAL_REGION_ID_MAX) {
    return (RuntimeAddressSpaceRecoveredBitmapSpan){0};
  }
  return runtime_address_space_attached_region_bitmap_span();
}

RuntimeAddressSpaceRecoveredOptionalMappingState
runtime_address_space_outlined_clear_optional_mapping_state(void) {
  return (RuntimeAddressSpaceRecoveredOptionalMappingState){0};
}
