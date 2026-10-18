#include "runtime_address_space_recovered_private.h"

#include "avp/runtime/ipc/runtime_capability.h"

static RuntimePageTableAttributes runtime_address_space_recovered_attributes(
    uint8_t memory_attribute,
    uint8_t mapping_attribute,
    uint8_t flags
) {
  const bool execute_never =
      mapping_attribute != 0 ||
      (flags & RUNTIME_ADDRESS_SPACE_MAPPING_FLAG_DEVICE) != 0;
  return (RuntimePageTableAttributes){
      .memory_attribute_index = memory_attribute,
      .access = RUNTIME_PAGE_TABLE_ACCESS_USER_READ_WRITE,
      .shareability = RUNTIME_PAGE_TABLE_SHAREABILITY_INNER,
      .access_flag = true,
      .not_global = (flags & RUNTIME_ADDRESS_SPACE_MAPPING_FLAG_GLOBAL) == 0,
      .privileged_execute_never = execute_never,
      .unprivileged_execute_never = execute_never,
  };
}

static RuntimeAddressSpaceRecoveredStatus
runtime_address_space_recovered_apply_mapping(
    const RuntimeAddressSpaceMapping *mapping) {
  RuntimeAddressSpaceRecoveredContext *context =
      runtime_address_space_recovered_context();
  if (context == 0 || context->page_table == 0) {
    return RUNTIME_ADDRESS_SPACE_RECOVERED_OK;
  }
  const size_t size =
      mapping->virtual_upper_bound - mapping->virtual_lower_bound;
  const RuntimePageTableAttributes attributes =
      runtime_address_space_recovered_attributes(
          mapping->memory_attributes,
          mapping->mapping_attributes,
          mapping->flags
      );
  RuntimePageTableTranslation translation;
  RuntimePageTableStatus page_status = runtime_page_table_lookup(
      context->page_table,
      mapping->virtual_lower_bound,
      &translation
  );
  if (page_status == RUNTIME_PAGE_TABLE_STATUS_NOT_MAPPED) {
    page_status = runtime_page_table_map(
        context->page_table,
        mapping->virtual_lower_bound,
        mapping->physical_base,
        size,
        attributes
    );
  } else if (page_status == RUNTIME_PAGE_TABLE_STATUS_OK) {
    const uintptr_t translated_base =
        translation.physical_address -
        (mapping->virtual_lower_bound - translation.virtual_address);
    if (translated_base != mapping->physical_base) {
      return RUNTIME_ADDRESS_SPACE_RECOVERED_CONFLICT;
    }
    page_status = runtime_page_table_reprotect(
        context->page_table,
        mapping->virtual_lower_bound,
        size,
        attributes
    );
  }
  return page_status == RUNTIME_PAGE_TABLE_STATUS_OK
             ? RUNTIME_ADDRESS_SPACE_RECOVERED_OK
             : RUNTIME_ADDRESS_SPACE_RECOVERED_PAGE_TABLE;
}

RuntimeAddressSpaceMapResult runtime_address_space_recovered_publish_slot(
    RuntimeAddressSpaceRecoveredMappingSlot *slot,
    uint8_t rights
) {
  /* Capability objects retain this in-context address for their lifetime. */
  RuntimeAddressSpaceRecoveredStatus status =
      runtime_address_space_recovered_apply_mapping(&slot->mapping);
  if (status != RUNTIME_ADDRESS_SPACE_RECOVERED_OK) {
    runtime_address_space_recovered_status(status);
    return (RuntimeAddressSpaceMapResult){0};
  }
  const uint16_t handle = runtime_capability_publish(
      &slot->mapping,
      &runtime_memory_mapping_type,
      RUNTIME_CAPABILITY_TYPE_MEMORY_MAPPING,
      rights,
      slot->mapping.audience
  );
  slot->handle = handle;
  slot->rights = rights;
  slot->active = true;
  runtime_address_space_recovered_status(RUNTIME_ADDRESS_SPACE_RECOVERED_OK);
  return (RuntimeAddressSpaceMapResult){
      .mapping_handle = handle,
      .virtual_lower_bound = slot->mapping.virtual_lower_bound,
  };
}

RuntimeAddressSpaceMapping *
runtime_address_space_mapping_resolve_object_id(uint16_t object_id) {
  RuntimeFirebloomSpan span = runtime_object_resolve(object_id, 0x20);
  if (span.type != &runtime_memory_mapping_type || span.current == 0) {
    runtime_address_space_recovered_status(
        RUNTIME_ADDRESS_SPACE_RECOVERED_NOT_FOUND);
    return 0;
  }
  runtime_address_space_recovered_status(RUNTIME_ADDRESS_SPACE_RECOVERED_OK);
  return span.current;
}

bool runtime_address_space_physical_range_is_global_direct(
    uintptr_t physical_address,
    size_t size,
    uint16_t region_id
) {
  RuntimeAddressSpaceRecoveredContext *context =
      runtime_address_space_recovered_context();
  if (context == 0 ||
      !runtime_address_space_recovered_range_valid(physical_address, size)) {
    return false;
  }
  const uintptr_t upper = physical_address + size;
  for (size_t index = 0; index < context->physical_policy_count; ++index) {
    const RuntimeAddressSpaceRecoveredPhysicalPolicy *policy =
        &context->physical_policies[index];
    if (policy->region_id == region_id && physical_address >= policy->lower &&
        upper <= policy->upper) {
      return policy->policy_class == 0 && !policy->reserved &&
             policy->excluded_set_mask == 0;
    }
  }
  return false;
}

RuntimeAddressSpaceMapResult runtime_address_space_map_global_direct_range(
    uintptr_t physical_address,
    size_t size,
    uint16_t region_id,
    uint8_t memory_attribute,
    uint8_t mapping_attribute,
    uint8_t rights
) {
  RuntimeAddressSpaceRecoveredContext *context =
      runtime_address_space_recovered_context();
  if (context == 0 || memory_attribute > 6 || mapping_attribute > 1 ||
      rights == 0 || rights > 7 ||
      !runtime_address_space_physical_range_is_global_direct(physical_address,
                                                             size, region_id)) {
    runtime_address_space_recovered_status(
        RUNTIME_ADDRESS_SPACE_RECOVERED_INVALID_ARGUMENT);
    return (RuntimeAddressSpaceMapResult){0};
  }
  uint16_t audience = 0;
  for (size_t index = 0; index < context->domain_count; ++index) {
    audience |= runtime_capability_audience_for_domain(
        context->domains[index].domain_id);
  }
  if (audience == 0 || context->current_domain == 0) {
    runtime_address_space_recovered_status(
        RUNTIME_ADDRESS_SPACE_RECOVERED_PERMISSION);
    return (RuntimeAddressSpaceMapResult){0};
  }
  RuntimeAddressSpaceRecoveredMappingSlot *slot =
      runtime_address_space_recovered_allocate_slot();
  if (slot == 0) {
    runtime_address_space_recovered_status(
        RUNTIME_ADDRESS_SPACE_RECOVERED_CAPACITY);
    return (RuntimeAddressSpaceMapResult){0};
  }
  slot->mapping = (RuntimeAddressSpaceMapping){
      .virtual_lower_bound = physical_address + context->profile_slide,
      .virtual_upper_bound = physical_address + context->profile_slide + size,
      .physical_base = physical_address,
      .audience = audience,
      .physical_region_id = region_id,
      .owner_domain = context->current_domain,
      .memory_attributes = memory_attribute,
      .mapping_attributes = mapping_attribute,
      .flags = RUNTIME_ADDRESS_SPACE_MAPPING_FLAG_GLOBAL |
               RUNTIME_ADDRESS_SPACE_MAPPING_FLAG_SHARED,
  };
  return runtime_address_space_recovered_publish_slot(slot, rights);
}

RuntimeAddressSpaceMapResult runtime_address_space_map_and_publish_subrange(
    uint16_t mapping_handle,
    size_t offset,
    size_t size,
    uint8_t domain_id,
    uint8_t rights
) {
  RuntimeAddressSpaceMapping *source =
      runtime_address_space_mapping_resolve(mapping_handle, 3);
  const size_t source_size =
      source->virtual_upper_bound - source->virtual_lower_bound;
  if (domain_id == 0 || domain_id > RUNTIME_DOMAIN_ID_MAX || rights == 0 ||
      rights > 7 || (offset & (RUNTIME_ADDRESS_SPACE_PAGE_SIZE - 1)) != 0 ||
      offset > source_size || size == 0 || size > source_size - offset ||
      (size & (RUNTIME_ADDRESS_SPACE_PAGE_SIZE - 1)) != 0) {
    runtime_address_space_recovered_status(
        RUNTIME_ADDRESS_SPACE_RECOVERED_INVALID_ARGUMENT);
    return (RuntimeAddressSpaceMapResult){0};
  }
  RuntimeAddressSpaceRecoveredMappingSlot *slot =
      runtime_address_space_recovered_allocate_slot();
  if (slot == 0) {
    runtime_address_space_recovered_status(
        RUNTIME_ADDRESS_SPACE_RECOVERED_CAPACITY);
    return (RuntimeAddressSpaceMapResult){0};
  }
  slot->mapping = *source;
  slot->mapping.virtual_lower_bound += offset;
  slot->mapping.virtual_upper_bound = slot->mapping.virtual_lower_bound + size;
  slot->mapping.physical_base += offset;
  slot->mapping.audience = runtime_capability_audience_for_domain(domain_id);
  slot->mapping.owner_domain = domain_id;
  slot->mapping.flags |= RUNTIME_ADDRESS_SPACE_MAPPING_FLAG_SHARED;
  return runtime_address_space_recovered_publish_slot(slot, rights);
}
