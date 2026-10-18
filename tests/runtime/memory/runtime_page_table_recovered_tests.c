#include "avp/runtime/memory/runtime_page_table_recovered.h"

#include <assert.h>
#include <stdio.h>

static uint64_t mock_ttbr0;
static uint64_t mock_ttbr1;
static uint64_t mock_tcr;
static uint64_t mock_mair;
static uint64_t mock_mmfr0;
static unsigned mock_publish_count;
static unsigned mock_invalidate_count;
static unsigned mock_icache_count;
static unsigned mock_disable_count;

void runtime_page_table_arch_publish(void) { ++mock_publish_count; }

void runtime_page_table_arch_invalidate_all(void) { ++mock_invalidate_count; }

uint64_t runtime_page_table_arch_read_id_aa64mmfr0_el1(void) {
  return mock_mmfr0;
}

void runtime_instruction_cache_invalidate_all(void) { ++mock_icache_count; }

void runtime_branch_predictor_invalidate_all_noop(void) {}

void runtime_tlb_invalidate_all_el1(void) { ++mock_invalidate_count; }

void runtime_tlb_invalidate_asid_el1(uint16_t asid) {
  mock_invalidate_count += asid != 0;
}

void j_runtime_tlb_invalidate_asid_el1(uint16_t asid) {
  runtime_tlb_invalidate_asid_el1(asid);
}

void runtime_write_tcr_el1(uint64_t value) { mock_tcr = value; }

void runtime_write_ttbr0_el1(uint64_t value) { mock_ttbr0 = value; }

void runtime_write_ttbr1_el1(uint64_t value) { mock_ttbr1 = value; }

uint64_t runtime_read_ttbr0_el1(void) { return mock_ttbr0; }

uint64_t runtime_read_ttbr1_el1(void) { return mock_ttbr1; }

void runtime_write_mair_el1(uint64_t value) { mock_mair = value; }

void runtime_tcr_el1_enable_ttbr1_walks(void) {
  mock_tcr &= ~(UINT64_C(1) << 23);
}

void runtime_write_sctlr_el1_require_mmu_wxn(uint64_t value) {
  (void)value;
  runtime_page_table_recovered_publish_mmu_active();
}

void runtime_disable_el0_timer_and_maintenance_access(void) {
  ++mock_disable_count;
}

static void initialize_pool(RuntimePageTablePage *pages, uint8_t count,
                            RuntimePageTablePool *pool) {
  assert(runtime_page_table_pool_initialize(pool, pages, UINT64_C(0x10000000),
                                            count) ==
         RUNTIME_PAGE_TABLE_STATUS_OK);
}

static void test_geometry_and_policy(void) {
  RuntimePageTableRecoveredContext context = {0};
  runtime_page_table_recovered_bind_context(&context);
  assert(runtime_page_size_shift() == 14);
  assert(runtime_virtual_address_bits() == 42);
  assert(runtime_virtual_address_bits_clone() == 42);
  assert(runtime_default_mair_el1_value() == UINT64_C(0x000000044400ff04));

  mock_mmfr0 = 0;
  assert(runtime_tcr_el1_bootstrap_value() == UINT64_C(0x0000000165d6a516));
  mock_mmfr0 = 5;
  assert(runtime_tcr_el1_bootstrap_value() == UINT64_C(0x0000000365d6a516));
  context.active_profile = 1;
  assert(runtime_tcr_el1_bootstrap_value() ==
         (UINT64_C(0x0000000365d6a516) & ~(UINT64_C(1) << 22)));

  assert(runtime_page_table_mapping_flags_valid(1));
  assert(runtime_page_table_mapping_flags_valid(3));
  assert(runtime_page_table_mapping_flags_valid(5));
  assert(runtime_page_table_mapping_flags_valid(9));
  assert(runtime_page_table_mapping_flags_valid(0x10000));
  assert(!runtime_page_table_mapping_flags_valid(0));
  assert(!runtime_page_table_mapping_flags_valid(7));
  assert(!runtime_page_table_mapping_flags_valid(0x20));

  assert(!runtime_page_table_level_uses_child_table(1));
  assert(runtime_page_table_level_uses_child_table(2));
  assert(runtime_page_table_level_entry_count(1) == 64);
  assert(runtime_page_table_level_entry_count(2) == 2048);
  assert(runtime_page_table_level_entry_count(3) == 2048);
  assert(runtime_page_table_level_entry_count(0) == 0);
  assert(runtime_page_table_validate_profile_virtual_address(
             UINT64_C(0x12344000)) == UINT64_C(0x12344000));

  runtime_page_table_update_policy_gate_enable();
  assert(context.update_policy_gate == 1);
  RuntimePageTableRecoveredByteView view =
      runtime_page_table_update_active_flag_bounded_view();
  assert(view.lower == &context.update_active);
  assert(view.current == view.lower);
  assert(view.upper == view.lower + 1);

  RuntimePageTableRecoveredOwner owner =
      runtime_page_table_zero_owner_register_arguments();
  assert(owner.lower == 0 && owner.upper == 0 && owner.type == 0);
  owner =
      runtime_page_table_outlined_prepare_attribute_update_arguments(1, 2, 3);
  assert(owner.lower == 1 && owner.upper == 2 && owner.type == 3);
  owner =
      runtime_page_table_outlined_prepare_release_or_unmap_arguments(4, 5, 6);
  assert(owner.lower == 4 && owner.upper == 5 && owner.type == 6);
  assert(runtime_page_table_zero_descriptor_policy_flags_w9_w11() == 0);
}

static void test_descriptor_contracts(void) {
  static RuntimePageTablePage page;
  RuntimePageTableRecoveredContext context = {0};
  runtime_page_table_recovered_bind_context(&context);
  mock_publish_count = 0;

  RuntimePageTableEntry leaf = 0;
  RuntimePageTableAttributes attributes = {
      .memory_attribute_index = 0,
      .access = RUNTIME_PAGE_TABLE_ACCESS_PRIVILEGED_READ_WRITE,
      .shareability = RUNTIME_PAGE_TABLE_SHAREABILITY_INNER,
      .access_flag = true,
  };
  assert(runtime_page_table_descriptor_encode_leaf(3, UINT64_C(0x800000),
                                                   attributes, &leaf) ==
         RUNTIME_PAGE_TABLE_STATUS_OK);
  assert(runtime_page_table_validate_descriptor_transition(3, 0, leaf) ==
         RUNTIME_PAGE_TABLE_STATUS_OK);
  assert(runtime_page_table_validate_descriptor_transition(0, 0, leaf) ==
         RUNTIME_PAGE_TABLE_STATUS_INVALID_ARGUMENT);

  assert(runtime_page_table_descriptor_write(&page, 7, leaf) ==
         RUNTIME_PAGE_TABLE_STATUS_OK);
  assert(mock_publish_count == 1);
  assert(runtime_page_table_descriptor_read(&page, 7) == leaf);
  assert(runtime_page_table_descriptor_read(
             &page, RUNTIME_PAGE_TABLE_ENTRY_COUNT) == 0);
  assert(context.last_status == RUNTIME_PAGE_TABLE_STATUS_INVALID_ARGUMENT);

  RuntimePageTablePage *restored_page = 0;
  size_t restored_index = 0;
  runtime_page_table_restore_descriptor_access_arguments(
      &page, 7, &restored_page, &restored_index);
  assert(restored_page == &page && restored_index == 7);
}

static void test_mapping_and_routes(void) {
  static RuntimePageTablePage pages[12];
  RuntimePageTablePool pool;
  RuntimePageTable table = {0};
  initialize_pool(pages, 12, &pool);
  assert(runtime_page_table_initialize(&table, &pool, 42, 3) ==
         RUNTIME_PAGE_TABLE_STATUS_OK);
  RuntimePageTableRecoveredContext context = {
      .table = &table,
      .pool = &pool,
      .published_root = table.root_physical_address,
  };
  runtime_page_table_recovered_bind_context(&context);

  assert(runtime_page_table_map_unowned_regime0_attribute0_flags1(
             UINT64_C(0x400000), UINT64_C(0x800000),
             RUNTIME_PAGE_TABLE_PAGE_SIZE) == RUNTIME_PAGE_TABLE_STATUS_OK);
  RuntimePageTableTranslation translation;
  assert(runtime_page_table_lookup(&table, UINT64_C(0x400123), &translation) ==
         RUNTIME_PAGE_TABLE_STATUS_OK);
  assert(translation.physical_address == UINT64_C(0x800123));
  assert(runtime_page_table_active_leaf_table_for_address(UINT64_C(0x400000)) !=
         0);
  assert(runtime_page_table_walk_to_leaf(&table, UINT64_C(0x400000)) != 0);

  assert(runtime_page_table_reprotect_range(UINT64_C(0x400000),
                                            RUNTIME_PAGE_TABLE_PAGE_SIZE, 0, 0,
                                            5) == RUNTIME_PAGE_TABLE_STATUS_OK);
  assert(runtime_page_table_lookup(&table, UINT64_C(0x400000), &translation) ==
         RUNTIME_PAGE_TABLE_STATUS_OK);
  assert(translation.attributes.access ==
         RUNTIME_PAGE_TABLE_ACCESS_PRIVILEGED_READ_ONLY);

  assert(runtime_page_table_release_range(UINT64_C(0x400000),
                                          RUNTIME_PAGE_TABLE_PAGE_SIZE, 11, 22,
                                          33) == RUNTIME_PAGE_TABLE_STATUS_OK);
  assert(context.last_owner.lower == 11);
  assert(context.last_owner.upper == 22);
  assert(context.last_owner.type == 33);
  assert(runtime_page_table_lookup(&table, UINT64_C(0x400000), &translation) ==
         RUNTIME_PAGE_TABLE_STATUS_NOT_MAPPED);

  assert(runtime_page_table_map_identity_regime_0_flags_1(
             UINT64_C(0x1000000), RUNTIME_PAGE_TABLE_PAGE_SIZE) ==
         RUNTIME_PAGE_TABLE_STATUS_OK);
  assert(runtime_page_table_unmap_range(UINT64_C(0x1000000),
                                        RUNTIME_PAGE_TABLE_PAGE_SIZE, 1, 2,
                                        3) == RUNTIME_PAGE_TABLE_STATUS_OK);

  assert(runtime_page_table_map_identity_regime_2_flags_1(
             UINT64_C(0x1040000), RUNTIME_PAGE_TABLE_PAGE_SIZE) ==
         RUNTIME_PAGE_TABLE_STATUS_OK);
  assert(runtime_page_table_unmap_range_unowned(UINT64_C(0x1040000),
                                                RUNTIME_PAGE_TABLE_PAGE_SIZE) ==
         RUNTIME_PAGE_TABLE_STATUS_OK);

  assert(runtime_page_table_map_identity_unowned_flags_10000(
             UINT64_C(0x1080000), RUNTIME_PAGE_TABLE_PAGE_SIZE) ==
         RUNTIME_PAGE_TABLE_STATUS_OK);
  assert(runtime_page_table_unmap_range_unowned(UINT64_C(0x1080000),
                                                RUNTIME_PAGE_TABLE_PAGE_SIZE) ==
         RUNTIME_PAGE_TABLE_STATUS_OK);

  assert(runtime_page_table_map_identity_regime0_attribute0_flags10000(
             UINT64_C(0x10c0000), RUNTIME_PAGE_TABLE_PAGE_SIZE) ==
         RUNTIME_PAGE_TABLE_STATUS_OK);
  assert(runtime_page_table_update_range_unowned_flags_10000(
             UINT64_C(0x10c0000), UINT64_C(0x10c0000),
             RUNTIME_PAGE_TABLE_PAGE_SIZE, 0,
             0) == RUNTIME_PAGE_TABLE_STATUS_OK);
  assert(runtime_page_table_update_range_unowned_flags_1(
             UINT64_C(0x1100000), UINT64_C(0x1100000),
             RUNTIME_PAGE_TABLE_PAGE_SIZE, 0,
             0) == RUNTIME_PAGE_TABLE_STATUS_OK);
  assert(
      runtime_page_table_update_range(UINT64_C(0x1140000), UINT64_C(0x1140000),
                                      RUNTIME_PAGE_TABLE_PAGE_SIZE, 0, 0, 1, 0,
                                      0, 0) == RUNTIME_PAGE_TABLE_STATUS_OK);
}

static void test_bootstrap_and_domain_roots(void) {
  static RuntimePageTablePage pages[12];
  RuntimePageTablePool pool;
  RuntimePageTable table = {0};
  initialize_pool(pages, 12, &pool);
  RuntimePageTableRecoveredBootstrapRecord records[] = {
      {
          .virtual_address = UINT64_C(0x2000000),
          .size = RUNTIME_PAGE_TABLE_PAGE_SIZE,
          .flags = RUNTIME_PAGE_TABLE_RECOVERED_FLAG_MAP,
          .repeat_count = 1,
          .repeat_stride = RUNTIME_PAGE_TABLE_PAGE_SIZE,
          .enabled = true,
      },
      {
          .virtual_address = UINT64_C(0x3000000),
          .size = RUNTIME_PAGE_TABLE_PAGE_SIZE,
      },
  };
  RuntimePageTableRecoveredContext context = {
      .table = &table,
      .pool = &pool,
      .bootstrap_records = records,
      .bootstrap_record_count = 2,
  };
  runtime_page_table_recovered_bind_context(&context);
  mock_mmfr0 = 0;
  mock_ttbr0 = 0;
  mock_ttbr1 = 0;
  mock_tcr = 0;
  mock_mair = 0;
  assert(runtime_page_table_bootstrap_initialize() ==
         RUNTIME_PAGE_TABLE_STATUS_OK);
  assert(context.published_root == table.root_physical_address);
  assert(mock_ttbr0 == table.root_physical_address);
  assert(mock_ttbr1 == 0);
  assert(mock_mair == runtime_default_mair_el1_value());
  assert(mock_tcr == runtime_tcr_el1_bootstrap_value());
  assert(runtime_page_table_assert_active_root() ==
         RUNTIME_PAGE_TABLE_STATUS_OK);
  assert(runtime_page_table_compare_published_root(mock_ttbr0 |
                                                   (UINT64_C(7) << 48)));

  RuntimePageTableTranslation translation;
  assert(runtime_page_table_lookup(&table, UINT64_C(0x2000000), &translation) ==
         RUNTIME_PAGE_TABLE_STATUS_OK);
  assert(runtime_page_table_lookup(&table, UINT64_C(0x2004000), &translation) ==
         RUNTIME_PAGE_TABLE_STATUS_OK);
    assert(runtime_page_table_apply_bootstrap_mapping_records() ==
        RUNTIME_PAGE_TABLE_STATUS_OK);

  RuntimePageTableRecoveredDomainRoot domain = {
      .root_physical_address = UINT64_C(0xabc000),
      .use_ttbr1 = true,
  };
  mock_ttbr1 = UINT64_C(0x123000);
  assert(runtime_page_table_switch_to_domain_root(&domain) ==
         UINT64_C(0x123000));
  assert(mock_ttbr1 == UINT64_C(0xabc000));
  assert(runtime_page_table_restore_active_root(UINT64_C(0x123000), true) ==
         RUNTIME_PAGE_TABLE_STATUS_OK);
  assert(mock_ttbr1 == UINT64_C(0x123000));

  RuntimePageTablePage *allocated = runtime_page_table_allocate_zeroed_page(3);
  assert(allocated != 0);
  for (size_t index = 0; index < RUNTIME_PAGE_TABLE_ENTRY_COUNT; ++index) {
    assert(allocated->entries[index] == 0);
  }
}

static void test_spans_and_arch_contracts(void) {
  RuntimePageTableRecoveredContext context = {
      .active_profile = 1,
      .alternate_profile_slide = UINT64_C(0x100000),
  };
  runtime_page_table_recovered_bind_context(&context);
  RuntimePageTableRecoveredStorageSpan span =
      runtime_page_table_make_storage_span(UINT64_C(0x4000), 0x80);
  assert(span.lower == UINT64_C(0x104000));
  assert(span.current == span.lower);
  assert(span.upper == UINT64_C(0x104080));
  assert(span.type == UINT64_C(0x144f30));

  mock_icache_count = 0;
  mock_disable_count = 0;
  mock_invalidate_count = 0;
  runtime_instruction_cache_invalidate_all();
  runtime_branch_predictor_invalidate_all_noop();
  runtime_tlb_invalidate_all_el1();
  runtime_tlb_invalidate_asid_el1(4);
  j_runtime_tlb_invalidate_asid_el1(5);
  runtime_disable_el0_timer_and_maintenance_access();
  assert(mock_icache_count == 1);
  assert(mock_disable_count == 1);
  assert(mock_invalidate_count == 3);

  mock_tcr = UINT64_C(1) << 23;
  runtime_tcr_el1_enable_ttbr1_walks();
  assert((mock_tcr & (UINT64_C(1) << 23)) == 0);
  runtime_write_sctlr_el1_require_mmu_wxn(0);
  assert(context.mmu_active == 1);
  assert(context.optimized_memory_active == 1);
}

int main(void) {
  test_geometry_and_policy();
  test_descriptor_contracts();
  test_mapping_and_routes();
  test_bootstrap_and_domain_roots();
  test_spans_and_arch_contracts();
  puts("runtime_page_table_recovered_tests: PASS");
  return 0;
}
