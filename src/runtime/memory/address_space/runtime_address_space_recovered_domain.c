#include "runtime_address_space_recovered_private.h"

#include "avp/runtime/ipc/runtime_capability.h"

static bool runtime_address_space_recovered_mode_valid(uint8_t mode) {
  return mode == 0 || mode == 1 || mode == 2 || mode == 4 || mode == 8;
}

static void runtime_address_space_recovered_bitmap_update(
    uint16_t region_id,
    bool attached
) {
  RuntimeAddressSpaceRecoveredContext *context =
      runtime_address_space_recovered_context();
  if (context == 0 || region_id == 0 ||
      region_id > RUNTIME_PHYSICAL_REGION_ID_MAX) {
    return;
  }
  const uint8_t mask = (uint8_t)(1u << (region_id & 7));
  uint8_t *byte = &context->attached_region_bitmap[region_id >> 3];
  if (attached) {
    *byte |= mask;
  } else {
    *byte &= (uint8_t)~mask;
  }
}

static RuntimeAddressSpaceRecoveredStatus
runtime_address_space_recovered_unmap_page_range(
    uintptr_t virtual_address,
    size_t size
) {
  RuntimeAddressSpaceRecoveredContext *context =
      runtime_address_space_recovered_context();
  if (context == 0 || context->page_table == 0) {
    return RUNTIME_ADDRESS_SPACE_RECOVERED_OK;
  }
  return runtime_page_table_unmap(context->page_table, virtual_address, size) ==
                 RUNTIME_PAGE_TABLE_STATUS_OK
             ? RUNTIME_ADDRESS_SPACE_RECOVERED_OK
             : RUNTIME_ADDRESS_SPACE_RECOVERED_PAGE_TABLE;
}

RuntimeAddressSpaceRecoveredStatus
runtime_address_space_unmap(
    uint16_t mapping_handle,
    uint8_t domain_id,
    uint8_t mode
) {
  if (!runtime_address_space_recovered_mode_valid(mode) || domain_id == 0 ||
      domain_id > RUNTIME_DOMAIN_ID_MAX) {
    return runtime_address_space_recovered_status(
        RUNTIME_ADDRESS_SPACE_RECOVERED_INVALID_ARGUMENT);
  }
  RuntimeAddressSpaceMapping *mapping =
      runtime_address_space_mapping_resolve(mapping_handle, 3);
  const uint16_t audience = runtime_capability_audience_for_domain(domain_id);
  if ((mapping->audience & audience) == 0) {
    return runtime_address_space_recovered_status(
        RUNTIME_ADDRESS_SPACE_RECOVERED_PERMISSION);
  }
  mapping->audience &= (uint16_t)~audience;
  if (mapping->audience != 0 || mode == 0) {
    return runtime_address_space_recovered_status(
        RUNTIME_ADDRESS_SPACE_RECOVERED_OK);
  }
  RuntimeAddressSpaceRecoveredStatus status =
      runtime_address_space_recovered_unmap_page_range(
          mapping->virtual_lower_bound,
          mapping->virtual_upper_bound - mapping->virtual_lower_bound
      );
  if (status != RUNTIME_ADDRESS_SPACE_RECOVERED_OK) {
    return runtime_address_space_recovered_status(status);
  }
  RuntimeAddressSpaceRecoveredMappingSlot *slot =
      runtime_address_space_recovered_slot(mapping_handle);
  runtime_capability_revoke_rights(
      mapping_handle,
      slot == 0 ? UINT8_C(3) : slot->rights
  );
  if (slot != 0) {
    slot->active = false;
  }
  runtime_address_space_recovered_bitmap_update(
      mapping->physical_region_id,
      false
  );
  return runtime_address_space_recovered_status(
      RUNTIME_ADDRESS_SPACE_RECOVERED_OK);
}

RuntimeAddressSpaceRecoveredStatus
runtime_address_space_unmap_current_range(
    uintptr_t virtual_address,
    size_t size
) {
  RuntimeAddressSpaceRecoveredContext *context =
      runtime_address_space_recovered_context();
  if (context == 0 || context->current_domain == 0 ||
      !runtime_address_space_recovered_range_valid(virtual_address, size)) {
    return runtime_address_space_recovered_status(
        RUNTIME_ADDRESS_SPACE_RECOVERED_INVALID_ARGUMENT);
  }
  const uintptr_t upper = virtual_address + size;
  const uint16_t audience =
      runtime_capability_audience_for_domain(context->current_domain);
  for (size_t index = 0;
       index < RUNTIME_ADDRESS_SPACE_RECOVERED_MAPPING_CAPACITY; ++index) {
    RuntimeAddressSpaceRecoveredMappingSlot *slot = &context->mappings[index];
    RuntimeAddressSpaceMapping *mapping = &slot->mapping;
    if (!slot->active || (mapping->audience & audience) == 0 ||
        virtual_address < mapping->virtual_lower_bound ||
        upper > mapping->virtual_upper_bound) {
      continue;
    }
    RuntimeAddressSpaceRecoveredStatus status =
        runtime_address_space_recovered_unmap_page_range(virtual_address, size);
    if (status != RUNTIME_ADDRESS_SPACE_RECOVERED_OK) {
      return runtime_address_space_recovered_status(status);
    }
    if (virtual_address == mapping->virtual_lower_bound &&
        upper == mapping->virtual_upper_bound) {
      runtime_capability_revoke_rights(slot->handle, slot->rights);
      slot->active = false;
      runtime_address_space_recovered_bitmap_update(
          mapping->physical_region_id,
          false
      );
    } else if (virtual_address == mapping->virtual_lower_bound) {
      mapping->virtual_lower_bound = upper;
      mapping->physical_base += size;
    } else if (upper == mapping->virtual_upper_bound) {
      mapping->virtual_upper_bound = virtual_address;
    } else {
      RuntimeAddressSpaceRecoveredMappingSlot *right =
          runtime_address_space_recovered_allocate_slot();
      if (right == 0) {
        return runtime_address_space_recovered_status(
            RUNTIME_ADDRESS_SPACE_RECOVERED_CAPACITY);
      }
      /* A middle hole needs two stable capability objects around the gap. */
      right->mapping = *mapping;
      right->mapping.virtual_lower_bound = upper;
      right->mapping.physical_base += upper - mapping->virtual_lower_bound;
      mapping->virtual_upper_bound = virtual_address;
      (void)runtime_address_space_recovered_publish_slot(right, 7);
    }
    return runtime_address_space_recovered_status(
        RUNTIME_ADDRESS_SPACE_RECOVERED_OK);
  }
  return runtime_address_space_recovered_status(
      RUNTIME_ADDRESS_SPACE_RECOVERED_NOT_FOUND);
}

RuntimeAddressSpaceMapResult
runtime_address_space_attach_mapping_to_domain(
    uint16_t mapping_handle,
    uint8_t domain_id,
    size_t offset,
    size_t size,
    uint8_t rights
) {
  RuntimeAddressSpaceMapping *source =
      runtime_address_space_mapping_resolve(mapping_handle, 3);
  if (source->audience == 0 ||
      (source->audience & ~RUNTIME_CAPABILITY_AUDIENCE_ALL) != 0) {
    runtime_address_space_mapping_audience_bounds_panic();
  }
  if (size == 0) {
    size = source->virtual_upper_bound - source->virtual_lower_bound - offset;
  }
  RuntimeAddressSpaceMapResult result =
      runtime_address_space_map_and_publish_subrange(
          mapping_handle,
          offset,
          size,
          domain_id,
          rights
      );
  if (result.mapping_handle != 0) {
    runtime_address_space_recovered_bitmap_update(
        source->physical_region_id,
        true
    );
  }
  return result;
}

RuntimeAddressSpaceRecoveredStatus
runtime_address_space_detach_mapping_from_domain(
    uint16_t mapping_handle,
    uint8_t domain_id
) {
  RuntimeAddressSpaceMapping *mapping =
      runtime_address_space_mapping_resolve(mapping_handle, 3);
  const uint16_t audience = runtime_capability_audience_for_domain(domain_id);
  if ((mapping->audience & audience) == 0) {
    return runtime_address_space_recovered_status(
        RUNTIME_ADDRESS_SPACE_RECOVERED_PERMISSION);
  }
  mapping->audience &= (uint16_t)~audience;
  if (mapping->audience == 0) {
    RuntimeAddressSpaceRecoveredMappingSlot *slot =
        runtime_address_space_recovered_slot(mapping_handle);
    runtime_capability_revoke_rights(
        mapping_handle,
        slot == 0 ? UINT8_C(3) : slot->rights
    );
    if (slot != 0) {
      slot->active = false;
    }
    runtime_address_space_recovered_bitmap_update(
        mapping->physical_region_id,
        false
    );
  }
  return runtime_address_space_recovered_status(
      RUNTIME_ADDRESS_SPACE_RECOVERED_OK);
}

uintptr_t
runtime_task_ensure_shared_page_mapping(
    RuntimeAddressSpaceRecoveredTask *task,
    uint8_t memory_attribute,
    uint8_t mapping_attribute
) {
  if (task == 0 || task->domain_id == 0 ||
      task->domain_id > RUNTIME_DOMAIN_ID_MAX || memory_attribute > 6 ||
      mapping_attribute > 1 ||
      (task->shared_virtual_base & (RUNTIME_ADDRESS_SPACE_PAGE_SIZE - 1)) !=
          0 ||
      (task->shared_physical_base & (RUNTIME_ADDRESS_SPACE_PAGE_SIZE - 1)) !=
          0) {
    runtime_address_space_recovered_status(
        RUNTIME_ADDRESS_SPACE_RECOVERED_INVALID_ARGUMENT);
    return 0;
  }
  if (task->mapping_handle != 0) {
    return task->shared_virtual_base;
  }
  RuntimeAddressSpaceRecoveredMappingSlot *slot =
      runtime_address_space_recovered_allocate_slot();
  if (slot == 0) {
    runtime_address_space_recovered_status(
        RUNTIME_ADDRESS_SPACE_RECOVERED_CAPACITY);
    return 0;
  }
  slot->mapping = (RuntimeAddressSpaceMapping){
      .virtual_lower_bound = task->shared_virtual_base,
      .virtual_upper_bound =
          task->shared_virtual_base + RUNTIME_ADDRESS_SPACE_PAGE_SIZE,
      .physical_base = task->shared_physical_base,
      .audience = runtime_capability_audience_for_domain(task->domain_id),
      .owner_domain = task->domain_id,
      .memory_attributes = memory_attribute,
      .mapping_attributes = mapping_attribute,
      .flags = RUNTIME_ADDRESS_SPACE_MAPPING_FLAG_SHARED,
  };
  RuntimeAddressSpaceMapResult result =
      runtime_address_space_recovered_publish_slot(slot, 7);
  task->mapping_handle = result.mapping_handle;
  return result.mapping_handle == 0 ? 0 : task->shared_virtual_base;
}

bool runtime_address_space_outlined_validate_saved_region_mapping(
    RuntimeFirebloomSpan region,
    const RuntimeAddressSpaceMapping *mapping,
    uint8_t domain_id
) {
  runtime_address_space_validate_region_mapping(region, mapping, domain_id);
  return true;
}
