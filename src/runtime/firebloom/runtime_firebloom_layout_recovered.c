#include "runtime_firebloom_layout_recovered_private.h"

#include <limits.h>

#if defined(__ELF__)
#define RUNTIME_FIREBLOOM_LAYOUT_STATE                                      \
  __attribute__((section(".runtime_auxiliary.state"), used))
#else
#define RUNTIME_FIREBLOOM_LAYOUT_STATE
#endif

RuntimeFirebloomLayoutRuntime runtime_firebloom_layout_runtime
    RUNTIME_FIREBLOOM_LAYOUT_STATE;
bool runtime_firebloom_layout_mutation_active
    RUNTIME_FIREBLOOM_LAYOUT_STATE;

_Noreturn void runtime_firebloom_layout_contract_fail(void) {
  __builtin_trap();
}

void runtime_firebloom_layout_bind_runtime(
    RuntimeFirebloomLayoutRuntime runtime) {
  if (runtime.allocate == NULL) {
    runtime_firebloom_layout_contract_fail();
  }
  runtime_firebloom_layout_runtime = runtime;
}

void runtime_firebloom_type_cache_mutation_enter(void) {
  if (runtime_firebloom_layout_mutation_active) {
    runtime_firebloom_layout_contract_fail();
  }
  runtime_firebloom_layout_mutation_active = true;
}

bool runtime_firebloom_type_cache_mutation_is_active(void) {
  return runtime_firebloom_layout_mutation_active;
}

size_t runtime_firebloom_layout_kind_stride(uint8_t kind) {
  static const uint8_t strides[] = {0x20U, 0x10U, 0x18U, 0x08U, 0x08U};
  kind &= 7U;
  if (kind >= sizeof(strides)) {
    runtime_firebloom_layout_kind_stride_invalid_kind();
  }
  return strides[kind];
}

_Noreturn void runtime_firebloom_layout_kind_stride_invalid_kind(void) {
  runtime_firebloom_layout_contract_fail();
}

size_t runtime_firebloom_layout_entry_end_offset(
    const RuntimeFirebloomLayoutEntry *entry) {
  const size_t offset = runtime_firebloom_layout_entry_offset(entry);
  const size_t count = runtime_firebloom_layout_entry_count(entry);
  const size_t stride = runtime_firebloom_layout_kind_stride(
      runtime_firebloom_layout_entry_kind(entry));
  if (count > (SIZE_MAX - offset) / stride) {
    runtime_firebloom_layout_entry_end_offset_invalid_kind();
  }
  return offset + count * stride;
}

_Noreturn void runtime_firebloom_layout_entry_end_offset_invalid_kind(void) {
  runtime_firebloom_layout_contract_fail();
}

void runtime_firebloom_type_layout_validate_ranges(
    const RuntimeFirebloomLayoutType *type) {
  if (type == NULL || (type->entry_count != 0U && type->entries == NULL)) {
    runtime_firebloom_layout_contract_fail();
  }
  size_t previous_end = 0U;
  for (size_t index = 0U; index < type->entry_count; ++index) {
    const RuntimeFirebloomLayoutEntry *entry = &type->entries[index];
    const size_t offset = runtime_firebloom_layout_entry_offset(entry);
    const size_t end = runtime_firebloom_layout_entry_end_offset(entry);
    if (offset < previous_end || offset >= type->byte_size ||
        end > type->byte_size) {
      runtime_firebloom_layout_contract_fail();
    }
    previous_end = end;
  }
}

void runtime_firebloom_type_relation_mark_failure_from_saved_state(
    RuntimeFirebloomLayoutReduction *reduction) {
  if (reduction == NULL) {
    runtime_firebloom_layout_contract_fail();
  }
  reduction->succeeded = false;
  reduction->failure_source = reduction->saved_state;
}

bool runtime_firebloom_type_relation_validate_layout_entry_pair(
    RuntimeFirebloomLayoutReduction *reduction,
    const RuntimeFirebloomLayoutEntry *left,
    const RuntimeFirebloomLayoutEntry *right
) {
  const uint64_t geometry_mask = UINT64_C(0xffffffff00000000);
  if (runtime_firebloom_layout_entry_offset(left) !=
          runtime_firebloom_layout_entry_offset(right) ||
      ((left->encoded ^ right->encoded) & geometry_mask) != 0U) {
    runtime_firebloom_type_relation_mark_failure_from_saved_state(reduction);
    return false;
  }
  if (runtime_firebloom_layout_entry_end_offset(left) !=
      runtime_firebloom_layout_entry_end_offset(right)) {
    runtime_firebloom_type_relation_mark_failure_from_saved_state(reduction);
    return false;
  }
  return true;
}

bool runtime_firebloom_type_relation_validate_saved_layout_entry_pair(
    RuntimeFirebloomLayoutReduction *reduction,
    const RuntimeFirebloomLayoutEntry *left,
    const RuntimeFirebloomLayoutEntry *right
) {
  return runtime_firebloom_type_relation_validate_layout_entry_pair(
      reduction,
      left,
      right
  );
}

bool runtime_firebloom_type_relation_reduce_layout_pair(
    RuntimeFirebloomTypeRelationSet *relations,
    RuntimeFirebloomLayoutReduction *reduction,
    RuntimeFirebloomLayoutType *left,
    RuntimeFirebloomLayoutType *right
) {
  if (relations == NULL || reduction == NULL || left == NULL || right == NULL) {
    runtime_firebloom_layout_contract_fail();
  }
  reduction->relations = relations;
  reduction->succeeded = true;
  reduction->failure_source = NULL;
  if (left->byte_size != right->byte_size ||
      left->entry_count != right->entry_count) {
    runtime_firebloom_type_relation_mark_failure_from_saved_state(reduction);
    return false;
  }
  runtime_firebloom_type_layout_validate_ranges(left);
  runtime_firebloom_type_layout_validate_ranges(right);
  for (size_t index = left->entry_count; index != 0U; --index) {
    RuntimeFirebloomLayoutEntry *left_entry = &left->entries[index - 1U];
    RuntimeFirebloomLayoutEntry *right_entry = &right->entries[index - 1U];
    if (!runtime_firebloom_type_relation_validate_saved_layout_entry_pair(
            reduction, left_entry, right_entry)) {
      return false;
    }
    const bool directed = reduction->structural &&
                          runtime_firebloom_layout_entry_is_structural(
                              left_entry);
    const RuntimeFirebloomRelationResult result = directed
        ? RUNTIME_FIREBLOOM_RELATION_DIRECTED
        : RUNTIME_FIREBLOOM_RELATION_EQUIVALENT;
    runtime_firebloom_type_relation_set_insert(
        relations,
        left_entry->type,
        right_entry->type,
        result,
        directed
    );
  }
  return true;
}

void runtime_firebloom_type_cache_mutation_leave(void) {
  if (!runtime_firebloom_layout_mutation_active) {
    runtime_firebloom_layout_contract_fail();
  }
  runtime_firebloom_layout_mutation_active = false;
  if (runtime_firebloom_layout_runtime.yield != NULL) {
    runtime_firebloom_layout_runtime.yield(
        runtime_firebloom_layout_runtime.context);
  }
}
