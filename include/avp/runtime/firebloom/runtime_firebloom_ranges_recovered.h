#pragma once

#include "avp/runtime/firebloom/runtime_firebloom_relations_recovered.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if __has_attribute(retain)
#define RUNTIME_FIREBLOOM_RANGES_PUBLIC __attribute__((used, retain))
#else
#define RUNTIME_FIREBLOOM_RANGES_PUBLIC __attribute__((used))
#endif

typedef enum {
  RUNTIME_FIREBLOOM_RANGE_KIND_BYTEWISE = 1,
  RUNTIME_FIREBLOOM_RANGE_KIND_SEQUENCE = 2,
  RUNTIME_FIREBLOOM_RANGE_KIND_AGGREGATE = 3,
  RUNTIME_FIREBLOOM_RANGE_KIND_POINTER = 4,
  RUNTIME_FIREBLOOM_RANGE_KIND_OPAQUE = 5,
  RUNTIME_FIREBLOOM_RANGE_KIND_INDIRECT = 6,
} RuntimeFirebloomRangeKind;

typedef struct RuntimeFirebloomRangeType RuntimeFirebloomRangeType;

typedef struct {
  size_t offset;
  size_t size;
  RuntimeFirebloomRangeType *type;
} RuntimeFirebloomRangeMember;

struct RuntimeFirebloomRangeType {
  RuntimeFirebloomRelationType relation;
  uint32_t element_size;
  uint32_t pointer_count;
  uint32_t member_count;
  uint32_t alignment;
  const RuntimeFirebloomRangeMember *members;
  RuntimeFirebloomRangeType *indirect_header;
};

typedef struct {
  void *current;
  void *lower_bound;
  void *upper_bound;
  RuntimeFirebloomRangeType *type;
} RuntimeFirebloomRetypeSpan;

_Static_assert(sizeof(RuntimeFirebloomRetypeSpan) == 0x20,
               "Firebloom retype spans use the recovered four-register ABI");

typedef enum {
  RUNTIME_FIREBLOOM_RETYPE_INVALID = 0,
  RUNTIME_FIREBLOOM_RETYPE_EXACT = 1,
  RUNTIME_FIREBLOOM_RETYPE_STRUCTURAL = 2,
} RuntimeFirebloomRetypeKind;

typedef struct {
  RuntimeFirebloomRetypeSpan span;
  RuntimeFirebloomRetypeKind kind;
} RuntimeFirebloomRetypeCandidate;

RUNTIME_FIREBLOOM_RANGES_PUBLIC size_t
runtime_firebloom_type_element_size(const RuntimeFirebloomRangeType *type);
RUNTIME_FIREBLOOM_RANGES_PUBLIC RuntimeFirebloomRetypeSpan
runtime_firebloom_outlined_validate_saved_span_x22_x19(
  RuntimeFirebloomRetypeSpan span,
  size_t size
);
RUNTIME_FIREBLOOM_RANGES_PUBLIC RuntimeFirebloomRetypeSpan
runtime_firebloom_outlined_restore_span_bounds_x21_x19(
    RuntimeFirebloomRetypeSpan saved);
RUNTIME_FIREBLOOM_RANGES_PUBLIC RuntimeFirebloomRetypeSpan
runtime_firebloom_outlined_save_span_bounds_x21_x19(
    RuntimeFirebloomRetypeSpan span);
RUNTIME_FIREBLOOM_RANGES_PUBLIC RuntimeFirebloomRetypeSpan
runtime_firebloom_outlined_validate_source_span_x23_x25(
  RuntimeFirebloomRetypeSpan span,
  size_t size
);
RUNTIME_FIREBLOOM_RANGES_PUBLIC RuntimeFirebloomRetypeCandidate
runtime_firebloom_tuple_retype_slow(
  RuntimeFirebloomRetypeSpan source,
  RuntimeFirebloomRangeType *target,
  bool required
);
RUNTIME_FIREBLOOM_RANGES_PUBLIC bool
runtime_firebloom_type_is_zero_sized(RuntimeFirebloomRangeType *type);
RUNTIME_FIREBLOOM_RANGES_PUBLIC _Noreturn void
runtime_firebloom_required_retype_failure(void);
RUNTIME_FIREBLOOM_RANGES_PUBLIC RuntimeFirebloomRetypeSpan
runtime_firebloom_tuple_validate(RuntimeFirebloomRetypeSpan span);
RUNTIME_FIREBLOOM_RANGES_PUBLIC size_t
runtime_firebloom_type_element_size_clone(
    const RuntimeFirebloomRangeType *type);
RUNTIME_FIREBLOOM_RANGES_PUBLIC RuntimeFirebloomRangeType *
runtime_firebloom_type_canonicalize(RuntimeFirebloomRangeType *type);
RUNTIME_FIREBLOOM_RANGES_PUBLIC RuntimeFirebloomRetypeCandidate
runtime_firebloom_try_retype_span(
  RuntimeFirebloomRetypeSpan source,
  size_t offset,
  size_t size,
  RuntimeFirebloomRangeType *target,
  RuntimeFirebloomTypeRelationSet *relations,
  bool required
);
RUNTIME_FIREBLOOM_RANGES_PUBLIC RuntimeFirebloomRetypeCandidate
runtime_firebloom_prepare_invalid_span_candidate(void);
RUNTIME_FIREBLOOM_RANGES_PUBLIC RuntimeFirebloomRangeType *
runtime_firebloom_aggregate_type_resolve_byte_offset(
  RuntimeFirebloomRangeType *type,
  size_t offset,
  bool required
);
RUNTIME_FIREBLOOM_RANGES_PUBLIC bool
runtime_firebloom_type_is_bytewise_clone(RuntimeFirebloomRangeType *type);
RUNTIME_FIREBLOOM_RANGES_PUBLIC RuntimeFirebloomRelationResult
runtime_firebloom_type_relation_resolve(
  RuntimeFirebloomTypeRelationSet *set,
  RuntimeFirebloomRangeType *left,
  RuntimeFirebloomRangeType *right,
  bool structural
);
RUNTIME_FIREBLOOM_RANGES_PUBLIC RuntimeFirebloomRetypeCandidate
runtime_firebloom_outlined_prepare_structural_retype_attempt(
  RuntimeFirebloomRetypeSpan source,
  RuntimeFirebloomRangeType *target
);
RUNTIME_FIREBLOOM_RANGES_PUBLIC void *
runtime_firebloom_outlined_return_null_pointer(void);
RUNTIME_FIREBLOOM_RANGES_PUBLIC RuntimeFirebloomRangeType *
runtime_firebloom_require_indirect_type_header(RuntimeFirebloomRangeType *type);
RUNTIME_FIREBLOOM_RANGES_PUBLIC RuntimeFirebloomRangeType *
runtime_firebloom_indirect_type_resolve_member(
  RuntimeFirebloomRangeType *type,
  size_t offset,
  bool required
);
RUNTIME_FIREBLOOM_RANGES_PUBLIC RuntimeFirebloomRelationResult
runtime_firebloom_type_relation_set_insert(
  RuntimeFirebloomTypeRelationSet *set,
  RuntimeFirebloomRangeType *left,
  RuntimeFirebloomRangeType *right,
  RuntimeFirebloomRelationResult result,
  bool directed
);
RUNTIME_FIREBLOOM_RANGES_PUBLIC bool
runtime_firebloom_type_relation_reduce(RuntimeFirebloomTypeRelationSet *set);
