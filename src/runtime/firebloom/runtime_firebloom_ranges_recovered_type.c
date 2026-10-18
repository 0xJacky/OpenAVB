#include "runtime_firebloom_ranges_recovered_private.h"

RuntimeFirebloomRangeType *RUNTIME_FIREBLOOM_RANGES_PUBLIC
runtime_firebloom_type_canonicalize(RuntimeFirebloomRangeType *type) {
  if (type == NULL) {
    runtime_firebloom_ranges_trap();
  }
  return runtime_firebloom_range_from_relation(
      runtime_firebloom_type_follow_identity_chain(&type->relation));
}

RuntimeFirebloomRangeType *RUNTIME_FIREBLOOM_RANGES_PUBLIC
runtime_firebloom_require_indirect_type_header(
    RuntimeFirebloomRangeType *type) {
  type = runtime_firebloom_type_canonicalize(type);
  if (runtime_firebloom_range_kind(type) !=
          RUNTIME_FIREBLOOM_RANGE_KIND_INDIRECT ||
      type->indirect_header == NULL) {
    runtime_firebloom_ranges_trap();
  }
  return runtime_firebloom_type_canonicalize(type->indirect_header);
}

size_t RUNTIME_FIREBLOOM_RANGES_PUBLIC
runtime_firebloom_type_element_size(const RuntimeFirebloomRangeType *type) {
  RuntimeFirebloomRangeType *canonical = runtime_firebloom_type_canonicalize(
      (RuntimeFirebloomRangeType *)(uintptr_t)type);
  const RuntimeFirebloomRangeKind kind =
      runtime_firebloom_range_kind(canonical);
  if (kind < RUNTIME_FIREBLOOM_RANGE_KIND_BYTEWISE ||
      kind > RUNTIME_FIREBLOOM_RANGE_KIND_INDIRECT) {
    return 0;
  }
  if (kind == RUNTIME_FIREBLOOM_RANGE_KIND_INDIRECT) {
    canonical = runtime_firebloom_require_indirect_type_header(canonical);
  }
  return canonical->element_size;
}

size_t RUNTIME_FIREBLOOM_RANGES_PUBLIC
runtime_firebloom_type_element_size_clone(
    const RuntimeFirebloomRangeType *type) {
  return runtime_firebloom_type_element_size(type);
}

bool RUNTIME_FIREBLOOM_RANGES_PUBLIC
runtime_firebloom_type_is_zero_sized(RuntimeFirebloomRangeType *type) {
  return runtime_firebloom_type_element_size(type) == 0;
}

bool RUNTIME_FIREBLOOM_RANGES_PUBLIC
runtime_firebloom_type_is_bytewise_clone(RuntimeFirebloomRangeType *type) {
  type = runtime_firebloom_type_canonicalize(type);
  RuntimeFirebloomRangeKind kind = runtime_firebloom_range_kind(type);
  if (kind == RUNTIME_FIREBLOOM_RANGE_KIND_INDIRECT) {
    type = runtime_firebloom_require_indirect_type_header(type);
    kind = runtime_firebloom_range_kind(type);
  }
  return kind == RUNTIME_FIREBLOOM_RANGE_KIND_BYTEWISE &&
         type->pointer_count == 0;
}

RuntimeFirebloomRangeType *RUNTIME_FIREBLOOM_RANGES_PUBLIC
runtime_firebloom_aggregate_type_resolve_byte_offset(
    RuntimeFirebloomRangeType *type,
    size_t offset,
    bool required
) {
  type = runtime_firebloom_type_canonicalize(type);
  if (offset == 0) {
    return type;
  }
  if (runtime_firebloom_range_kind(type) ==
      RUNTIME_FIREBLOOM_RANGE_KIND_AGGREGATE) {
    for (uint32_t index = 0; index < type->member_count; ++index) {
      if (type->members[index].offset == offset &&
          type->members[index].type != NULL) {
        return runtime_firebloom_type_canonicalize(type->members[index].type);
      }
    }
  }
  if (required) {
    runtime_firebloom_required_retype_failure();
  }
  return NULL;
}

RuntimeFirebloomRangeType *RUNTIME_FIREBLOOM_RANGES_PUBLIC
runtime_firebloom_indirect_type_resolve_member(
    RuntimeFirebloomRangeType *type,
    size_t offset,
    bool required
) {
  RuntimeFirebloomRangeType *header =
      runtime_firebloom_require_indirect_type_header(type);
  if (offset == 0) {
    return header;
  }
  RuntimeFirebloomRangeType *member =
      runtime_firebloom_aggregate_type_resolve_byte_offset(
          header,
          offset,
          false
      );
  if (member == NULL && required) {
    runtime_firebloom_required_retype_failure();
  }
  return member;
}

void *RUNTIME_FIREBLOOM_RANGES_PUBLIC
runtime_firebloom_outlined_return_null_pointer(void) {
  return NULL;
}
