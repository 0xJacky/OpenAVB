#pragma once

#include "avp/runtime/memory/runtime_page_table.h"

#define RUNTIME_PAGE_TABLE_RECOVERED_ENTRY                                     \
  __attribute__((used, retain, minsize, noinline))

enum {
  RUNTIME_PAGE_TABLE_RECOVERED_VIRTUAL_ADDRESS_BITS = 42,
  RUNTIME_PAGE_TABLE_RECOVERED_FLAG_MAP = 1,
  RUNTIME_PAGE_TABLE_RECOVERED_FLAG_USER = 2,
  RUNTIME_PAGE_TABLE_RECOVERED_FLAG_READ_ONLY = 4,
  RUNTIME_PAGE_TABLE_RECOVERED_FLAG_EXECUTE_NEVER = 8,
  RUNTIME_PAGE_TABLE_RECOVERED_FLAG_POLICY = 0x10000,
  RUNTIME_PAGE_TABLE_RECOVERED_ATTRIBUTE_RELEASE = 7,
  RUNTIME_PAGE_TABLE_RECOVERED_ATTRIBUTE_UNMAP = 8,
};

typedef struct {
  uintptr_t lower;
  uintptr_t upper;
  uint64_t type;
} RuntimePageTableRecoveredOwner;

typedef struct {
  uintptr_t lower;
  uintptr_t current;
  uintptr_t upper;
  uint64_t type;
} RuntimePageTableRecoveredStorageSpan;

typedef struct {
  uint8_t *lower;
  uint8_t *current;
  uint8_t *upper;
} RuntimePageTableRecoveredByteView;

typedef struct {
  uintptr_t root_physical_address;
  bool use_ttbr1;
} RuntimePageTableRecoveredDomainRoot;

typedef struct {
  uintptr_t virtual_address;
  size_t size;
  uint8_t regime;
  uint8_t memory_attribute;
  uint32_t flags;
  uint16_t repeat_count;
  size_t repeat_stride;
  bool enabled;
} RuntimePageTableRecoveredBootstrapRecord;

typedef struct {
  RuntimePageTable *table;
  RuntimePageTablePool *pool;
  RuntimePageTableRecoveredBootstrapRecord *bootstrap_records;
  size_t bootstrap_record_count;
  uintptr_t published_root;
  uintptr_t alternate_profile_slide;
  RuntimePageTableRecoveredOwner last_owner;
  RuntimePageTableStatus last_status;
  uint32_t fatal_line;
  uint8_t active_profile;
  uint8_t update_active;
  uint8_t update_policy_gate;
  uint8_t mmu_active;
  uint8_t optimized_memory_active;
} RuntimePageTableRecoveredContext;

void runtime_page_table_recovered_bind_context(
    RuntimePageTableRecoveredContext *context);
RuntimePageTableRecoveredContext *runtime_page_table_recovered_context(void);

RUNTIME_PAGE_TABLE_RECOVERED_ENTRY uint8_t runtime_page_size_shift(void);
RUNTIME_PAGE_TABLE_RECOVERED_ENTRY uint64_t
runtime_default_mair_el1_value(void);
RUNTIME_PAGE_TABLE_RECOVERED_ENTRY uint8_t runtime_virtual_address_bits(void);
RUNTIME_PAGE_TABLE_RECOVERED_ENTRY uint8_t
runtime_virtual_address_bits_clone(void);
RUNTIME_PAGE_TABLE_RECOVERED_ENTRY uint64_t
runtime_tcr_el1_bootstrap_value(void);

RUNTIME_PAGE_TABLE_RECOVERED_ENTRY void
runtime_instruction_cache_invalidate_all(void);
RUNTIME_PAGE_TABLE_RECOVERED_ENTRY void
runtime_branch_predictor_invalidate_all_noop(void);
RUNTIME_PAGE_TABLE_RECOVERED_ENTRY void runtime_tlb_invalidate_all_el1(void);
RUNTIME_PAGE_TABLE_RECOVERED_ENTRY void
runtime_tlb_invalidate_asid_el1(uint16_t asid);
RUNTIME_PAGE_TABLE_RECOVERED_ENTRY void
j_runtime_tlb_invalidate_asid_el1(uint16_t asid);
RUNTIME_PAGE_TABLE_RECOVERED_ENTRY void runtime_write_tcr_el1(uint64_t value);
RUNTIME_PAGE_TABLE_RECOVERED_ENTRY void runtime_write_ttbr0_el1(uint64_t value);
RUNTIME_PAGE_TABLE_RECOVERED_ENTRY void runtime_write_ttbr1_el1(uint64_t value);
RUNTIME_PAGE_TABLE_RECOVERED_ENTRY uint64_t runtime_read_ttbr0_el1(void);
RUNTIME_PAGE_TABLE_RECOVERED_ENTRY uint64_t runtime_read_ttbr1_el1(void);
RUNTIME_PAGE_TABLE_RECOVERED_ENTRY void runtime_write_mair_el1(uint64_t value);
RUNTIME_PAGE_TABLE_RECOVERED_ENTRY void
runtime_tcr_el1_enable_ttbr1_walks(void);
RUNTIME_PAGE_TABLE_RECOVERED_ENTRY void
runtime_write_sctlr_el1_require_mmu_wxn(uint64_t value);
RUNTIME_PAGE_TABLE_RECOVERED_ENTRY void
runtime_disable_el0_timer_and_maintenance_access(void);

void runtime_page_table_recovered_publish_mmu_active(void);

RUNTIME_PAGE_TABLE_RECOVERED_ENTRY RuntimePageTablePage *
runtime_page_table_allocate_zeroed_page(uint8_t level);
RUNTIME_PAGE_TABLE_RECOVERED_ENTRY RuntimePageTableRecoveredStorageSpan
runtime_page_table_make_storage_span(uintptr_t address, size_t size);
RUNTIME_PAGE_TABLE_RECOVERED_ENTRY RuntimePageTableStatus
runtime_page_table_assert_active_root(void);
RUNTIME_PAGE_TABLE_RECOVERED_ENTRY uint64_t
runtime_page_table_switch_to_domain_root(
    const RuntimePageTableRecoveredDomainRoot *domain);
RUNTIME_PAGE_TABLE_RECOVERED_ENTRY RuntimePageTableStatus
runtime_page_table_restore_active_root(uint64_t saved_root, bool use_ttbr1);

RUNTIME_PAGE_TABLE_RECOVERED_ENTRY RuntimePageTableStatus
runtime_page_table_update_range_unowned(
  uintptr_t virtual_address,
  uintptr_t physical_address,
  size_t size,
  uint8_t regime,
  uint8_t memory_attribute,
  uint32_t flags
);
RUNTIME_PAGE_TABLE_RECOVERED_ENTRY RuntimePageTableStatus
runtime_page_table_update_range_internal(
  uintptr_t virtual_address,
  uintptr_t physical_address,
  size_t size,
  uint8_t regime,
  uint8_t memory_attribute,
  uint32_t flags,
  uintptr_t owner_lower,
  uintptr_t owner_upper,
  uint64_t owner_type
);
RUNTIME_PAGE_TABLE_RECOVERED_ENTRY RuntimePageTableStatus
runtime_page_table_update_range(
  uintptr_t virtual_address,
  uintptr_t physical_address,
  size_t size,
  uint8_t regime,
  uint8_t memory_attribute,
  uint32_t flags,
  uintptr_t owner_lower,
  uintptr_t owner_upper,
  uint64_t owner_type
);
RUNTIME_PAGE_TABLE_RECOVERED_ENTRY RuntimePageTableStatus
runtime_page_table_unmap_range_unowned(uintptr_t virtual_address, size_t size);
RUNTIME_PAGE_TABLE_RECOVERED_ENTRY RuntimePageTableStatus
runtime_page_table_release_range(
  uintptr_t virtual_address,
  size_t size,
  uintptr_t owner_lower,
  uintptr_t owner_upper,
  uint64_t owner_type
);
RUNTIME_PAGE_TABLE_RECOVERED_ENTRY RuntimePageTableStatus
runtime_page_table_unmap_range(
  uintptr_t virtual_address,
  size_t size,
  uintptr_t owner_lower,
  uintptr_t owner_upper,
  uint64_t owner_type
);
RUNTIME_PAGE_TABLE_RECOVERED_ENTRY RuntimePageTableStatus
runtime_page_table_reprotect_range(
  uintptr_t virtual_address,
  size_t size,
  uint8_t regime,
  uint8_t memory_attribute,
  uint32_t flags
);

RUNTIME_PAGE_TABLE_RECOVERED_ENTRY RuntimePageTablePage *
runtime_page_table_walk_to_leaf(
  RuntimePageTable *table,
  uintptr_t virtual_address
);
RUNTIME_PAGE_TABLE_RECOVERED_ENTRY size_t
runtime_page_table_level_entry_count(uint8_t level);
RUNTIME_PAGE_TABLE_RECOVERED_ENTRY RuntimePageTableEntry
runtime_page_table_descriptor_read(
  const RuntimePageTablePage *page,
  size_t index
);
RUNTIME_PAGE_TABLE_RECOVERED_ENTRY RuntimePageTablePage *
runtime_page_table_active_leaf_table_for_address(uintptr_t virtual_address);
RUNTIME_PAGE_TABLE_RECOVERED_ENTRY RuntimePageTableStatus
runtime_page_table_descriptor_write(
  RuntimePageTablePage *page,
  size_t index,
  RuntimePageTableEntry descriptor
);

RUNTIME_PAGE_TABLE_RECOVERED_ENTRY RuntimePageTableStatus
runtime_page_table_initialize_bootstrap_roots(void);
RUNTIME_PAGE_TABLE_RECOVERED_ENTRY RuntimePageTableStatus
runtime_page_table_bootstrap_initialize(void);
RUNTIME_PAGE_TABLE_RECOVERED_ENTRY RuntimePageTableStatus
runtime_page_table_apply_bootstrap_mapping_records(void);
RUNTIME_PAGE_TABLE_RECOVERED_ENTRY RuntimePageTableStatus
runtime_page_table_map_bootstrap_identity_ranges(void);

RUNTIME_PAGE_TABLE_RECOVERED_ENTRY RuntimePageTableStatus
runtime_page_table_map_identity_regime_0_flags_1(
  uintptr_t address,
  size_t size
);
RUNTIME_PAGE_TABLE_RECOVERED_ENTRY RuntimePageTableStatus
runtime_page_table_map_identity_regime_2_flags_1(
  uintptr_t address,
  size_t size
);
RUNTIME_PAGE_TABLE_RECOVERED_ENTRY RuntimePageTableStatus
runtime_page_table_map_identity_unowned_flags_10000(
  uintptr_t address,
  size_t size
);
RUNTIME_PAGE_TABLE_RECOVERED_ENTRY RuntimePageTableStatus
runtime_page_table_map_identity_regime0_attribute0_flags10000(
  uintptr_t address,
  size_t size
);
RUNTIME_PAGE_TABLE_RECOVERED_ENTRY RuntimePageTableStatus
runtime_page_table_map_unowned_regime0_attribute0_flags1(
  uintptr_t virtual_address,
  uintptr_t physical_address,
  size_t size
);
RUNTIME_PAGE_TABLE_RECOVERED_ENTRY RuntimePageTableStatus
runtime_page_table_update_range_unowned_flags_1(
  uintptr_t virtual_address,
  uintptr_t physical_address,
  size_t size,
  uint8_t regime,
  uint8_t memory_attribute
);
RUNTIME_PAGE_TABLE_RECOVERED_ENTRY RuntimePageTableStatus
runtime_page_table_update_range_unowned_flags_10000(
  uintptr_t virtual_address,
  uintptr_t physical_address,
  size_t size,
  uint8_t regime,
  uint8_t memory_attribute
);

RUNTIME_PAGE_TABLE_RECOVERED_ENTRY void
runtime_page_table_update_policy_gate_enable(void);
RUNTIME_PAGE_TABLE_RECOVERED_ENTRY RuntimePageTableRecoveredByteView
runtime_page_table_update_active_flag_bounded_view(void);
RUNTIME_PAGE_TABLE_RECOVERED_ENTRY bool
runtime_page_table_mapping_flags_valid(uint32_t flags);
RUNTIME_PAGE_TABLE_RECOVERED_ENTRY bool
runtime_page_table_level_uses_child_table(uint8_t level);
RUNTIME_PAGE_TABLE_RECOVERED_ENTRY RuntimePageTableStatus
runtime_page_table_validate_descriptor_transition(
  uint8_t level,
  RuntimePageTableEntry old_descriptor,
  RuntimePageTableEntry new_descriptor
);
RUNTIME_PAGE_TABLE_RECOVERED_ENTRY uintptr_t
runtime_page_table_validate_profile_virtual_address(uintptr_t address);
RUNTIME_PAGE_TABLE_RECOVERED_ENTRY void
runtime_page_table_require_level_1_to_3(uint8_t level);
RUNTIME_PAGE_TABLE_RECOVERED_ENTRY _Noreturn void
runtime_page_table_invalid_level_fatal(void);
RUNTIME_PAGE_TABLE_RECOVERED_ENTRY _Noreturn void
runtime_page_table_noncanonical_virtual_address_fatal(void);

RUNTIME_PAGE_TABLE_RECOVERED_ENTRY RuntimePageTableRecoveredOwner
runtime_page_table_zero_owner_register_arguments(void);
RUNTIME_PAGE_TABLE_RECOVERED_ENTRY RuntimePageTableRecoveredOwner
runtime_page_table_outlined_prepare_attribute_update_arguments(
  uintptr_t lower,
  uintptr_t upper,
  uint64_t type
);
RUNTIME_PAGE_TABLE_RECOVERED_ENTRY RuntimePageTableRecoveredOwner
runtime_page_table_outlined_prepare_release_or_unmap_arguments(
  uintptr_t lower,
  uintptr_t upper,
  uint64_t type
);
RUNTIME_PAGE_TABLE_RECOVERED_ENTRY void
runtime_page_table_restore_descriptor_access_arguments(
  RuntimePageTablePage *page,
  size_t index,
  RuntimePageTablePage **restored_page,
  size_t *restored_index
);
RUNTIME_PAGE_TABLE_RECOVERED_ENTRY bool
runtime_page_table_compare_published_root(uint64_t root);
RUNTIME_PAGE_TABLE_RECOVERED_ENTRY uint32_t
runtime_page_table_zero_descriptor_policy_flags_w9_w11(void);
