#include "runtime_page_table_recovered_private.h"

static RuntimePageTableRecoveredContext *
runtime_page_table_require_context(void) {
  RuntimePageTableRecoveredContext *context =
      runtime_page_table_recovered_context();
  if (context == 0 || context->table == 0) {
    runtime_page_table_recovered_record_status(
        RUNTIME_PAGE_TABLE_STATUS_INVALID_ARGUMENT);
    return 0;
  }
  return context;
}

RuntimePageTableStatus runtime_page_table_update_range_internal(
    uintptr_t virtual_address,
    uintptr_t physical_address,
    size_t size,
    uint8_t regime,
    uint8_t memory_attribute,
    uint32_t flags,
    uintptr_t owner_lower,
    uintptr_t owner_upper,
    uint64_t owner_type
) {
  RuntimePageTableRecoveredContext *context =
      runtime_page_table_require_context();
  if (context == 0 || !runtime_page_table_mapping_flags_valid(flags)) {
    return runtime_page_table_recovered_record_status(
        RUNTIME_PAGE_TABLE_STATUS_INVALID_ARGUMENT);
  }
  context->last_owner = (RuntimePageTableRecoveredOwner){
      owner_lower,
      owner_upper,
      owner_type,
  };
  context->update_active = 1;

  RuntimePageTableStatus status;
  if (memory_attribute == RUNTIME_PAGE_TABLE_RECOVERED_ATTRIBUTE_RELEASE ||
      memory_attribute == RUNTIME_PAGE_TABLE_RECOVERED_ATTRIBUTE_UNMAP) {
    status = runtime_page_table_unmap(context->table, virtual_address, size);
  } else if (memory_attribute > 7) {
    status = RUNTIME_PAGE_TABLE_STATUS_INVALID_ARGUMENT;
  } else {
    const RuntimePageTableAttributes attributes =
        runtime_page_table_recovered_attributes(
            regime,
            memory_attribute,
            flags
        );
    RuntimePageTableTranslation translation;
    status = runtime_page_table_lookup(
        context->table,
        virtual_address,
        &translation
    );
    if (status == RUNTIME_PAGE_TABLE_STATUS_NOT_MAPPED) {
      status = runtime_page_table_map(
          context->table,
          virtual_address,
          physical_address,
          size,
          attributes
      );
    } else if (status == RUNTIME_PAGE_TABLE_STATUS_OK) {
      const uintptr_t translated_base =
          translation.physical_address -
          (virtual_address - translation.virtual_address);
      if (translated_base != physical_address) {
        status = RUNTIME_PAGE_TABLE_STATUS_CONFLICT;
      } else {
        status = runtime_page_table_reprotect(
            context->table,
            virtual_address,
            size,
            attributes
        );
      }
    }
  }
  context->update_active = 0;
  return runtime_page_table_recovered_record_status(status);
}

RuntimePageTableStatus runtime_page_table_update_range(
    uintptr_t virtual_address,
    uintptr_t physical_address,
    size_t size,
    uint8_t regime,
    uint8_t memory_attribute,
    uint32_t flags,
    uintptr_t owner_lower,
    uintptr_t owner_upper,
    uint64_t owner_type
) {
  return runtime_page_table_update_range_internal(
      virtual_address,
      physical_address,
      size,
      regime,
      memory_attribute,
      flags,
      owner_lower,
      owner_upper,
      owner_type
  );
}

RuntimePageTableStatus runtime_page_table_update_range_unowned(
    uintptr_t virtual_address,
    uintptr_t physical_address,
    size_t size,
    uint8_t regime,
    uint8_t memory_attribute,
    uint32_t flags
) {
  return runtime_page_table_update_range_internal(
      virtual_address,
      physical_address,
      size,
      regime,
      memory_attribute,
      flags,
      0,
      0,
      0
  );
}

RuntimePageTableStatus runtime_page_table_update_range_unowned_flags_1(
    uintptr_t virtual_address,
    uintptr_t physical_address,
    size_t size,
    uint8_t regime,
    uint8_t memory_attribute
) {
  return runtime_page_table_update_range_unowned(
      virtual_address,
      physical_address,
      size,
      regime,
      memory_attribute,
      RUNTIME_PAGE_TABLE_RECOVERED_FLAG_MAP
  );
}

RuntimePageTableStatus runtime_page_table_update_range_unowned_flags_10000(
    uintptr_t virtual_address,
    uintptr_t physical_address,
    size_t size,
    uint8_t regime,
    uint8_t memory_attribute
) {
  return runtime_page_table_update_range_unowned(
      virtual_address,
      physical_address,
      size,
      regime,
      memory_attribute,
      RUNTIME_PAGE_TABLE_RECOVERED_FLAG_POLICY
  );
}

RuntimePageTableStatus
runtime_page_table_unmap_range_unowned(uintptr_t virtual_address, size_t size) {
  return runtime_page_table_update_range_unowned(
      virtual_address,
      0,
      size,
      3,
      RUNTIME_PAGE_TABLE_RECOVERED_ATTRIBUTE_UNMAP,
      RUNTIME_PAGE_TABLE_RECOVERED_FLAG_MAP
  );
}

RuntimePageTableStatus
runtime_page_table_release_range(
    uintptr_t virtual_address,
    size_t size,
    uintptr_t owner_lower,
    uintptr_t owner_upper,
    uint64_t owner_type
) {
  return runtime_page_table_update_range_internal(
      virtual_address,
      0,
      size,
      3,
      RUNTIME_PAGE_TABLE_RECOVERED_ATTRIBUTE_RELEASE,
      RUNTIME_PAGE_TABLE_RECOVERED_FLAG_MAP,
      owner_lower,
      owner_upper,
      owner_type
  );
}

RuntimePageTableStatus runtime_page_table_unmap_range(
    uintptr_t virtual_address,
    size_t size,
    uintptr_t owner_lower,
    uintptr_t owner_upper,
    uint64_t owner_type
) {
  return runtime_page_table_update_range_internal(
      virtual_address,
      0,
      size,
      3,
      RUNTIME_PAGE_TABLE_RECOVERED_ATTRIBUTE_UNMAP,
      RUNTIME_PAGE_TABLE_RECOVERED_FLAG_MAP,
      owner_lower,
      owner_upper,
      owner_type
  );
}

RuntimePageTableStatus
runtime_page_table_reprotect_range(
    uintptr_t virtual_address,
    size_t size,
    uint8_t regime,
    uint8_t memory_attribute,
    uint32_t flags
) {
  RuntimePageTableRecoveredContext *context =
      runtime_page_table_require_context();
  if (context == 0) {
    return RUNTIME_PAGE_TABLE_STATUS_INVALID_ARGUMENT;
  }
  RuntimePageTableTranslation translation;
  RuntimePageTableStatus status =
      runtime_page_table_lookup(context->table, virtual_address, &translation);
  if (status != RUNTIME_PAGE_TABLE_STATUS_OK) {
    return runtime_page_table_recovered_record_status(status);
  }
  const uintptr_t physical_address =
      translation.physical_address -
      (virtual_address - translation.virtual_address);
  return runtime_page_table_update_range_unowned(
      virtual_address,
      physical_address,
      size,
      regime,
      memory_attribute,
      flags
  );
}

RuntimePageTableStatus
runtime_page_table_map_identity_regime_0_flags_1(
    uintptr_t address,
    size_t size
) {
  return runtime_page_table_update_range_unowned_flags_1(
      address,
      address,
      size,
      0,
      0
  );
}

RuntimePageTableStatus
runtime_page_table_map_identity_regime_2_flags_1(
    uintptr_t address,
    size_t size
) {
  return runtime_page_table_update_range_unowned_flags_1(
      address,
      address,
      size,
      2,
      0
  );
}

RuntimePageTableStatus
runtime_page_table_map_identity_unowned_flags_10000(
    uintptr_t address,
    size_t size
) {
  return runtime_page_table_update_range_unowned_flags_10000(
      address,
      address,
      size,
      0,
      0
  );
}

RuntimePageTableStatus
runtime_page_table_map_identity_regime0_attribute0_flags10000(
    uintptr_t address,
    size_t size
) {
  return runtime_page_table_map_identity_unowned_flags_10000(address, size);
}

RuntimePageTableStatus runtime_page_table_map_unowned_regime0_attribute0_flags1(
    uintptr_t virtual_address,
    uintptr_t physical_address,
    size_t size
) {
  return runtime_page_table_update_range_unowned_flags_1(
      virtual_address,
      physical_address,
      size,
      0,
      0
  );
}

RuntimePageTablePage *
runtime_page_table_walk_to_leaf(
    RuntimePageTable *table,
    uintptr_t virtual_address
) {
  RuntimePageTableWalk walk;
  RuntimePageTableStatus status =
      runtime_page_table_walk(table, virtual_address, 3, false, &walk);
  runtime_page_table_recovered_record_status(status);
  if (status != RUNTIME_PAGE_TABLE_STATUS_OK) {
    return 0;
  }
  return (
      RuntimePageTablePage *)((uintptr_t)walk.entry &
                              ~(uintptr_t)(RUNTIME_PAGE_TABLE_PAGE_SIZE - 1));
}

size_t runtime_page_table_level_entry_count(uint8_t level) {
  if (level < 1 || level > 3) {
    return 0;
  }
  const uint8_t shift = runtime_page_table_level_shift(level);
  if (shift >= RUNTIME_PAGE_TABLE_RECOVERED_VIRTUAL_ADDRESS_BITS) {
    return 1;
  }
  const uint8_t bits =
      (uint8_t)(RUNTIME_PAGE_TABLE_RECOVERED_VIRTUAL_ADDRESS_BITS - shift);
  return bits >= 11 ? RUNTIME_PAGE_TABLE_ENTRY_COUNT : (size_t)1 << bits;
}

RuntimePageTableEntry
runtime_page_table_descriptor_read(
    const RuntimePageTablePage *page,
    size_t index
) {
  if (page == 0 || index >= RUNTIME_PAGE_TABLE_ENTRY_COUNT) {
    runtime_page_table_recovered_record_status(
        RUNTIME_PAGE_TABLE_STATUS_INVALID_ARGUMENT);
    return 0;
  }
  runtime_page_table_recovered_record_status(RUNTIME_PAGE_TABLE_STATUS_OK);
  return page->entries[index];
}

RuntimePageTableStatus
runtime_page_table_descriptor_write(
    RuntimePageTablePage *page,
    size_t index,
    RuntimePageTableEntry descriptor
) {
  if (page == 0 || index >= RUNTIME_PAGE_TABLE_ENTRY_COUNT) {
    return runtime_page_table_recovered_record_status(
        RUNTIME_PAGE_TABLE_STATUS_INVALID_ARGUMENT);
  }
  page->entries[index] = descriptor;
  runtime_page_table_arch_publish();
  return runtime_page_table_recovered_record_status(
      RUNTIME_PAGE_TABLE_STATUS_OK);
}

RuntimePageTablePage *
runtime_page_table_active_leaf_table_for_address(uintptr_t virtual_address) {
  RuntimePageTableRecoveredContext *context =
      runtime_page_table_require_context();
  return context == 0
             ? 0
             : runtime_page_table_walk_to_leaf(context->table, virtual_address);
}

RuntimePageTableRecoveredStorageSpan
runtime_page_table_make_storage_span(uintptr_t address, size_t size) {
  RuntimePageTableRecoveredContext *context =
      runtime_page_table_recovered_context();
  uintptr_t lower = address;
  if (context != 0 && context->active_profile != 0) {
    lower += context->alternate_profile_slide;
  }
  if (size > UINTPTR_MAX - lower) {
    runtime_page_table_recovered_record_status(
        RUNTIME_PAGE_TABLE_STATUS_INVALID_ARGUMENT);
    return (RuntimePageTableRecoveredStorageSpan){0};
  }
  return (RuntimePageTableRecoveredStorageSpan){
      .lower = lower,
      .current = lower,
      .upper = lower + size,
      .type = UINT64_C(0x144f30),
  };
}

bool runtime_page_table_compare_published_root(uint64_t root) {
  RuntimePageTableRecoveredContext *context =
      runtime_page_table_recovered_context();
  return context != 0 &&
         (root & UINT64_C(0x0000ffffffffffff)) == context->published_root;
}
