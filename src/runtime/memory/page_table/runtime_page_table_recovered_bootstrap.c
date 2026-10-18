#include "runtime_page_table_recovered_private.h"

#include "runtime_page_table_private.h"

RuntimePageTablePage *runtime_page_table_allocate_zeroed_page(uint8_t level) {
  RuntimePageTableRecoveredContext *context =
      runtime_page_table_recovered_context();
  if (level < 1 || level > 3 || context == 0 || context->pool == 0) {
    runtime_page_table_recovered_record_status(
        RUNTIME_PAGE_TABLE_STATUS_INVALID_ARGUMENT);
    return 0;
  }
  RuntimePageTablePage *page = 0;
  uintptr_t physical_address = 0;
  RuntimePageTableStatus status =
      runtime_page_table_pool_allocate(context->pool, &page, &physical_address);
  (void)physical_address;
  runtime_page_table_recovered_record_status(status);
  return status == RUNTIME_PAGE_TABLE_STATUS_OK ? page : 0;
}

RuntimePageTableStatus runtime_page_table_assert_active_root(void) {
  RuntimePageTableRecoveredContext *context =
      runtime_page_table_recovered_context();
  if (context == 0 || context->published_root == 0) {
    return runtime_page_table_recovered_record_status(
        RUNTIME_PAGE_TABLE_STATUS_INVALID_ARGUMENT);
  }
  const uint64_t root = context->active_profile == 0 ? runtime_read_ttbr0_el1()
                                                     : runtime_read_ttbr1_el1();
  return runtime_page_table_recovered_record_status(
      runtime_page_table_compare_published_root(root)
          ? RUNTIME_PAGE_TABLE_STATUS_OK
          : RUNTIME_PAGE_TABLE_STATUS_CONFLICT);
}

uint64_t runtime_page_table_switch_to_domain_root(
    const RuntimePageTableRecoveredDomainRoot *domain) {
  if (domain == 0 || domain->root_physical_address == 0) {
    runtime_page_table_recovered_record_status(
        RUNTIME_PAGE_TABLE_STATUS_INVALID_ARGUMENT);
    return 0;
  }
  uint64_t previous;
  if (domain->use_ttbr1) {
    previous = runtime_read_ttbr1_el1();
    runtime_write_ttbr1_el1(domain->root_physical_address);
  } else {
    previous = runtime_read_ttbr0_el1();
    runtime_write_ttbr0_el1(domain->root_physical_address);
  }
  runtime_tlb_invalidate_all_el1();
  runtime_page_table_recovered_record_status(RUNTIME_PAGE_TABLE_STATUS_OK);
  return previous;
}

RuntimePageTableStatus
runtime_page_table_restore_active_root(uint64_t saved_root, bool use_ttbr1) {
  if (saved_root == 0) {
    return runtime_page_table_recovered_record_status(
        RUNTIME_PAGE_TABLE_STATUS_INVALID_ARGUMENT);
  }
  if (use_ttbr1) {
    runtime_write_ttbr1_el1(saved_root);
  } else {
    runtime_write_ttbr0_el1(saved_root);
  }
  runtime_tlb_invalidate_all_el1();
  return runtime_page_table_recovered_record_status(
      RUNTIME_PAGE_TABLE_STATUS_OK);
}

RuntimePageTableStatus runtime_page_table_initialize_bootstrap_roots(void) {
  RuntimePageTableRecoveredContext *context =
      runtime_page_table_recovered_context();
  if (context == 0 || context->table == 0 || context->pool == 0) {
    return runtime_page_table_recovered_record_status(
        RUNTIME_PAGE_TABLE_STATUS_INVALID_ARGUMENT);
  }
  RuntimePageTableStatus status = RUNTIME_PAGE_TABLE_STATUS_OK;
  if (context->table->root == 0) {
    status = runtime_page_table_initialize(
        context->table,
        context->pool,
        RUNTIME_PAGE_TABLE_RECOVERED_VIRTUAL_ADDRESS_BITS,
        0
    );
  }
  if (status != RUNTIME_PAGE_TABLE_STATUS_OK) {
    return runtime_page_table_recovered_record_status(status);
  }
  context->published_root = context->table->root_physical_address;
  runtime_write_mair_el1(runtime_default_mair_el1_value());
  if (context->active_profile == 0) {
    runtime_write_ttbr0_el1(context->published_root);
    runtime_write_ttbr1_el1(0);
  } else {
    runtime_write_ttbr0_el1(0);
    runtime_write_ttbr1_el1(context->published_root);
  }
  runtime_tlb_invalidate_all_el1();
  return runtime_page_table_recovered_record_status(
      RUNTIME_PAGE_TABLE_STATUS_OK);
}

RuntimePageTableStatus runtime_page_table_bootstrap_initialize(void) {
  runtime_write_tcr_el1(runtime_tcr_el1_bootstrap_value());
  RuntimePageTableStatus status =
      runtime_page_table_initialize_bootstrap_roots();
  if (status == RUNTIME_PAGE_TABLE_STATUS_OK) {
    status = runtime_page_table_map_bootstrap_identity_ranges();
  }
  return runtime_page_table_recovered_record_status(status);
}

RuntimePageTableStatus
runtime_page_table_apply_bootstrap_mapping_records(void) {
  RuntimePageTableRecoveredContext *context =
      runtime_page_table_recovered_context();
  if (context == 0 || (context->bootstrap_record_count != 0 &&
                       context->bootstrap_records == 0)) {
    return runtime_page_table_recovered_record_status(
        RUNTIME_PAGE_TABLE_STATUS_INVALID_ARGUMENT);
  }
  for (size_t index = 0; index < context->bootstrap_record_count; ++index) {
    const RuntimePageTableRecoveredBootstrapRecord *record =
        &context->bootstrap_records[index];
    if (!record->enabled) {
      continue;
    }
    for (uint16_t repeat = 0; repeat <= record->repeat_count; ++repeat) {
      if (repeat != 0 && record->repeat_stride != 0 &&
          repeat >
              (UINTPTR_MAX - record->virtual_address) / record->repeat_stride) {
        return runtime_page_table_recovered_record_status(
            RUNTIME_PAGE_TABLE_STATUS_INVALID_ARGUMENT);
      }
      const uintptr_t address =
          record->virtual_address + (uintptr_t)repeat * record->repeat_stride;
      RuntimePageTableStatus status = runtime_page_table_update_range_unowned(
          address,
          address,
          record->size,
          record->regime,
          record->memory_attribute,
          record->flags
      );
      if (status != RUNTIME_PAGE_TABLE_STATUS_OK) {
        return status;
      }
    }
  }
  return runtime_page_table_recovered_record_status(
      RUNTIME_PAGE_TABLE_STATUS_OK);
}

RuntimePageTableStatus runtime_page_table_map_bootstrap_identity_ranges(void) {
  return runtime_page_table_apply_bootstrap_mapping_records();
}
