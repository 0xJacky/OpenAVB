#include "runtime_page_table_recovered_private.h"

static RuntimePageTableRecoveredContext *recovered_context;

void runtime_page_table_recovered_bind_context(
    RuntimePageTableRecoveredContext *context) {
  recovered_context = context;
}

RuntimePageTableRecoveredContext *runtime_page_table_recovered_context(void) {
  return recovered_context;
}

RuntimePageTableStatus
runtime_page_table_recovered_record_status(RuntimePageTableStatus status) {
  if (recovered_context != 0) {
    recovered_context->last_status = status;
  }
  return status;
}

_Noreturn void runtime_page_table_recovered_fatal(uint32_t line) {
  if (recovered_context != 0) {
    recovered_context->fatal_line = line;
    recovered_context->last_status = RUNTIME_PAGE_TABLE_STATUS_INVALID_ARGUMENT;
  }
  __builtin_trap();
}

uint8_t runtime_page_size_shift(void) { return RUNTIME_PAGE_TABLE_PAGE_SHIFT; }

uint64_t runtime_default_mair_el1_value(void) {
  return UINT64_C(0x000000044400ff04);
}

uint8_t runtime_virtual_address_bits(void) {
  return RUNTIME_PAGE_TABLE_RECOVERED_VIRTUAL_ADDRESS_BITS;
}

uint8_t runtime_virtual_address_bits_clone(void) {
  return RUNTIME_PAGE_TABLE_RECOVERED_VIRTUAL_ADDRESS_BITS;
}

uint64_t runtime_tcr_el1_bootstrap_value(void) {
  const uint64_t physical_range =
      runtime_page_table_arch_read_id_aa64mmfr0_el1() & UINT64_C(0xf);
  uint64_t value = (physical_range & UINT64_C(0xe)) == 0
                       ? UINT64_C(0x0000000165d6a516)
                       : UINT64_C(0x0000000365d6a516);
  if (recovered_context != 0 && recovered_context->active_profile != 0) {
    value &= ~(UINT64_C(1) << 22);
  }
  return value;
}

void runtime_page_table_recovered_publish_mmu_active(void) {
  if (recovered_context != 0) {
    recovered_context->mmu_active = 1;
    recovered_context->optimized_memory_active = 1;
  }
}

void runtime_page_table_update_policy_gate_enable(void) {
  if (recovered_context != 0) {
    recovered_context->update_policy_gate = 1;
  }
}

RuntimePageTableRecoveredByteView
runtime_page_table_update_active_flag_bounded_view(void) {
  if (recovered_context == 0) {
    return (RuntimePageTableRecoveredByteView){0};
  }
  return (RuntimePageTableRecoveredByteView){
      .lower = &recovered_context->update_active,
      .current = &recovered_context->update_active,
      .upper = &recovered_context->update_active + 1,
  };
}

bool runtime_page_table_mapping_flags_valid(uint32_t flags) {
  const uint32_t common_forbidden = UINT32_C(0xfffefc60);
  if ((flags & RUNTIME_PAGE_TABLE_RECOVERED_FLAG_POLICY) != 0) {
    return (flags & (common_forbidden | UINT32_C(0x39b))) == 0;
  }
  if ((flags & RUNTIME_PAGE_TABLE_RECOVERED_FLAG_MAP) == 0 ||
      (flags & common_forbidden) != 0) {
    return false;
  }
  const uint32_t mode = flags & UINT32_C(0xe);
  return (mode & (mode - 1)) == 0;
}

void runtime_page_table_require_level_1_to_3(uint8_t level) {
  if (level < 1 || level > 3) {
    runtime_page_table_invalid_level_fatal();
  }
}

bool runtime_page_table_level_uses_child_table(uint8_t level) {
  runtime_page_table_require_level_1_to_3(level);
  return level > 1;
}

_Noreturn void runtime_page_table_invalid_level_fatal(void) {
  runtime_page_table_recovered_fatal(UINT32_C(0x209));
}

uintptr_t
runtime_page_table_validate_profile_virtual_address(uintptr_t address) {
  if (!runtime_page_table_address_is_canonical(
          address, RUNTIME_PAGE_TABLE_RECOVERED_VIRTUAL_ADDRESS_BITS)) {
    runtime_page_table_noncanonical_virtual_address_fatal();
  }
  return address;
}

_Noreturn void runtime_page_table_noncanonical_virtual_address_fatal(void) {
  runtime_page_table_recovered_fatal(UINT32_C(0x15e));
}

RuntimePageTableAttributes runtime_page_table_recovered_attributes(
    uint8_t regime,
    uint8_t memory_attribute,
    uint32_t flags
) {
  RuntimePageTableAccess access =
      RUNTIME_PAGE_TABLE_ACCESS_PRIVILEGED_READ_WRITE;
  if ((flags & RUNTIME_PAGE_TABLE_RECOVERED_FLAG_USER) != 0) {
    access = RUNTIME_PAGE_TABLE_ACCESS_USER_READ_WRITE;
  } else if ((flags & RUNTIME_PAGE_TABLE_RECOVERED_FLAG_READ_ONLY) != 0) {
    access = RUNTIME_PAGE_TABLE_ACCESS_PRIVILEGED_READ_ONLY;
  }
  if ((flags & RUNTIME_PAGE_TABLE_RECOVERED_FLAG_EXECUTE_NEVER) != 0) {
    access = RUNTIME_PAGE_TABLE_ACCESS_USER_READ_ONLY;
  }
  return (RuntimePageTableAttributes){
      .memory_attribute_index = memory_attribute,
      .non_secure = regime == 2,
      .access = access,
      .shareability = RUNTIME_PAGE_TABLE_SHAREABILITY_INNER,
      .access_flag = true,
      .not_global = regime == 3,
      .privileged_execute_never =
          (flags & RUNTIME_PAGE_TABLE_RECOVERED_FLAG_EXECUTE_NEVER) != 0,
      .unprivileged_execute_never =
          (flags & RUNTIME_PAGE_TABLE_RECOVERED_FLAG_EXECUTE_NEVER) != 0,
  };
}

RuntimePageTableStatus runtime_page_table_validate_descriptor_transition(
    uint8_t level,
    RuntimePageTableEntry old_descriptor,
    RuntimePageTableEntry new_descriptor
) {
  if (level < 1 || level > 3) {
    return runtime_page_table_recovered_record_status(
        RUNTIME_PAGE_TABLE_STATUS_INVALID_ARGUMENT);
  }
  RuntimePageTableDescriptor old_value;
  RuntimePageTableDescriptor new_value;
  RuntimePageTableStatus status =
      runtime_page_table_descriptor_decode(old_descriptor, level, &old_value);
  if (status == RUNTIME_PAGE_TABLE_STATUS_OK) {
    status =
        runtime_page_table_descriptor_decode(new_descriptor, level, &new_value);
  }
  if (status != RUNTIME_PAGE_TABLE_STATUS_OK) {
    return runtime_page_table_recovered_record_status(status);
  }
  if (old_value.kind == RUNTIME_PAGE_TABLE_DESCRIPTOR_KIND_TABLE &&
      new_value.kind != RUNTIME_PAGE_TABLE_DESCRIPTOR_KIND_INVALID &&
      (new_value.kind != RUNTIME_PAGE_TABLE_DESCRIPTOR_KIND_TABLE ||
       new_value.output_address != old_value.output_address)) {
    return runtime_page_table_recovered_record_status(
        RUNTIME_PAGE_TABLE_STATUS_CONFLICT);
  }
  if (old_value.kind != RUNTIME_PAGE_TABLE_DESCRIPTOR_KIND_INVALID &&
      new_value.kind != RUNTIME_PAGE_TABLE_DESCRIPTOR_KIND_INVALID &&
      old_value.kind != new_value.kind) {
    return runtime_page_table_recovered_record_status(
        RUNTIME_PAGE_TABLE_STATUS_CONFLICT);
  }
  return runtime_page_table_recovered_record_status(
      RUNTIME_PAGE_TABLE_STATUS_OK);
}

RuntimePageTableRecoveredOwner
runtime_page_table_zero_owner_register_arguments(void) {
  return (RuntimePageTableRecoveredOwner){0};
}

RuntimePageTableRecoveredOwner
runtime_page_table_outlined_prepare_attribute_update_arguments(
    uintptr_t lower,
    uintptr_t upper,
    uint64_t type
) {
  return (RuntimePageTableRecoveredOwner){lower, upper, type};
}

RuntimePageTableRecoveredOwner
runtime_page_table_outlined_prepare_release_or_unmap_arguments(
    uintptr_t lower,
    uintptr_t upper,
    uint64_t type
) {
  return (RuntimePageTableRecoveredOwner){lower, upper, type};
}

void runtime_page_table_restore_descriptor_access_arguments(
    RuntimePageTablePage *page,
    size_t index,
    RuntimePageTablePage **restored_page,
    size_t *restored_index
) {
  *restored_page = page;
  *restored_index = index;
}

uint32_t runtime_page_table_zero_descriptor_policy_flags_w9_w11(void) {
  return 0;
}
