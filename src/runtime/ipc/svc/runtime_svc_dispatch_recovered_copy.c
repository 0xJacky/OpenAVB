#include "runtime_svc_dispatch_recovered_private.h"

#include "avp/runtime/memory/runtime_memory.h"
#include "avp/runtime/support/runtime_diagnostics.h"
#include "avp/runtime/support/runtime_unprivileged_memory_recovered.h"


void RUNTIME_SVC_DISPATCH_PUBLIC runtime_copy_unprivileged_c_string_bounded(
    uint8_t *destination,
    uintptr_t destination_lower_bound,
    uintptr_t destination_upper_bound,
    const uint8_t *source,
    uint64_t capacity
) {
  if (capacity == 0 || !runtime_svc_dispatch_recovered_contains(
                           destination, (const void *)destination_lower_bound,
                           (const void *)destination_upper_bound,
                           (size_t)capacity)) {
    runtime_svc_dispatch_recovered_trap();
  }
  for (uint64_t index = 0; index + 1 < capacity; ++index) {
    uint8_t value = 0;
    runtime_copy_from_unprivileged(
        (RuntimeFirebloomSpan){
            .current = &value,
            .lower_bound = &value,
            .upper_bound = &value + 1,
            .type = &runtime_unprivileged_byte_type,
        },
        source + index,
        1
    );
    destination[index] = value;
    if (value == 0) {
      return;
    }
  }
  destination[capacity - 1] = 0;
}

_Noreturn void RUNTIME_SVC_DISPATCH_PUBLIC
runtime_svc_copy_unprivileged_diagnostic_and_fatal(
    const uint8_t *short_diagnostic,
    const uint8_t *long_diagnostic
) {
  char short_copy[0x40] = {0};
  char long_copy[0x800] = {0};
  if (short_diagnostic != 0) {
    runtime_copy_unprivileged_c_string_bounded(
        (uint8_t *)short_copy,
        (uintptr_t)short_copy,
        (uintptr_t)(short_copy + sizeof(short_copy)),
        short_diagnostic,
        sizeof(short_copy)
    );
  }
  if (long_diagnostic != 0) {
    runtime_copy_unprivileged_c_string_bounded(
        (uint8_t *)long_copy,
        (uintptr_t)long_copy,
        (uintptr_t)(long_copy + sizeof(long_copy)),
        long_diagnostic,
        sizeof(long_copy)
    );
  }
  RuntimeSVCDispatchRecoveredState *state =
      runtime_svc_dispatch_recovered_state();
  if (state->fatal != 0) {
    state->fatal(
        state->context,
        runtime_address_space_source_token(),
        0x2762,
        short_copy,
        long_copy
    );
  }
  runtime_svc_dispatch_recovered_trap();
}

_Noreturn void RUNTIME_SVC_DISPATCH_PUBLIC
runtime_svc_bootstrap_memory_command_fatal(void) {
  RuntimeSVCDispatchRecoveredState *state =
      runtime_svc_dispatch_recovered_state();
  if (state->fatal != 0) {
    state->fatal(
        state->context,
        runtime_address_space_source_token(),
        10018,
        0,
        0
    );
  }
  runtime_svc_dispatch_recovered_trap();
}

void RUNTIME_SVC_DISPATCH_PUBLIC
runtime_svc_outlined_zero_16k_page(void *page) {
  if (page == 0 || ((uintptr_t)page & (RUNTIME_PAGE_TABLE_PAGE_SIZE - 1)) != 0) {
    runtime_svc_dispatch_recovered_trap();
  }
  runtime_bzero(page, RUNTIME_PAGE_TABLE_PAGE_SIZE);
}

uint64_t RUNTIME_SVC_DISPATCH_PUBLIC
runtime_svc_outlined_map_identity_attribute0_flags11(
    uintptr_t virtual_base,
    uintptr_t physical_base,
    size_t byte_size,
    uint32_t translation_regime,
    void *owner,
    void *owner_lower_bound
) {
  if (owner == 0 || owner_lower_bound == 0 ||
      (uintptr_t)owner < (uintptr_t)owner_lower_bound) {
    runtime_svc_dispatch_recovered_trap();
  }
  return runtime_page_table_update_range(
      virtual_base,
      physical_base,
      byte_size,
      (uint8_t)translation_regime,
      0,
      0x11,
      (uintptr_t)owner_lower_bound,
      (uintptr_t)owner_lower_bound + sizeof(uintptr_t),
      (uintptr_t)owner
  );
}

RuntimeFirebloomSpan RUNTIME_SVC_DISPATCH_PUBLIC
runtime_svc_outlined_copy_span_with_saved_source(
    RuntimeFirebloomSpan destination,
    size_t byte_count,
    const void *source_current,
    const RuntimeFirebloomType *source_type
) {
  if (source_current == 0 || source_type == 0) {
    runtime_svc_dispatch_recovered_trap();
  }
  RuntimeFirebloomSpan source = {
      .current = (void *)(uintptr_t)source_current,
      .lower_bound = (void *)(uintptr_t)source_current,
      .upper_bound = (uint8_t *)(uintptr_t)source_current + byte_count,
      .type = source_type,
  };
  (void)runtime_firebloom_copy_span(destination, source, byte_count);
  return destination;
}
