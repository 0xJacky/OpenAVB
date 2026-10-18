#include "runtime_firebloom_ranges_recovered_private.h"

static RuntimeFirebloomRetypeSpan
runtime_firebloom_validate_retype_span(
    RuntimeFirebloomRetypeSpan span,
    size_t size
) {
  if (span.type == NULL) {
    runtime_firebloom_ranges_trap();
  }
  const uintptr_t current = runtime_firebloom_range_address(span.current);
  const uintptr_t lower = runtime_firebloom_range_address(span.lower_bound);
  const uintptr_t upper = runtime_firebloom_range_address(span.upper_bound);
  if (current < lower || current > upper || size > upper - current) {
    runtime_firebloom_ranges_trap();
  }

  const size_t element_size = runtime_firebloom_type_element_size(span.type);
  if (!runtime_firebloom_type_is_bytewise_clone(span.type)) {
    if (element_size == 0 || (current - lower) % element_size != 0 ||
        size % element_size != 0) {
      runtime_firebloom_ranges_trap();
    }
  }
  return span;
}

RuntimeFirebloomRetypeSpan RUNTIME_FIREBLOOM_RANGES_PUBLIC
runtime_firebloom_outlined_validate_saved_span_x22_x19(
    RuntimeFirebloomRetypeSpan span,
    size_t size
) {
  return runtime_firebloom_validate_retype_span(span, size);
}

RuntimeFirebloomRetypeSpan RUNTIME_FIREBLOOM_RANGES_PUBLIC
runtime_firebloom_outlined_restore_span_bounds_x21_x19(
    RuntimeFirebloomRetypeSpan saved) {
  return saved;
}

RuntimeFirebloomRetypeSpan RUNTIME_FIREBLOOM_RANGES_PUBLIC
runtime_firebloom_outlined_save_span_bounds_x21_x19(
    RuntimeFirebloomRetypeSpan span) {
  return span;
}

RuntimeFirebloomRetypeSpan RUNTIME_FIREBLOOM_RANGES_PUBLIC
runtime_firebloom_outlined_validate_source_span_x23_x25(
    RuntimeFirebloomRetypeSpan span,
    size_t size
) {
  return runtime_firebloom_validate_retype_span(span, size);
}

RuntimeFirebloomRetypeSpan RUNTIME_FIREBLOOM_RANGES_PUBLIC
runtime_firebloom_tuple_validate(RuntimeFirebloomRetypeSpan span) {
  return runtime_firebloom_validate_retype_span(span, 0);
}

_Noreturn void RUNTIME_FIREBLOOM_RANGES_PUBLIC
runtime_firebloom_required_retype_failure(void) {
  runtime_firebloom_ranges_trap();
}

RuntimeFirebloomRetypeCandidate RUNTIME_FIREBLOOM_RANGES_PUBLIC
runtime_firebloom_prepare_invalid_span_candidate(void) {
  return (RuntimeFirebloomRetypeCandidate){0};
}

RuntimeFirebloomRetypeCandidate RUNTIME_FIREBLOOM_RANGES_PUBLIC
runtime_firebloom_outlined_prepare_structural_retype_attempt(
    RuntimeFirebloomRetypeSpan source,
    RuntimeFirebloomRangeType *target
) {
  source.type = runtime_firebloom_type_canonicalize(target);
  return (RuntimeFirebloomRetypeCandidate){
      .span = source,
      .kind = RUNTIME_FIREBLOOM_RETYPE_STRUCTURAL,
  };
}

static RuntimeFirebloomRetypeCandidate
runtime_firebloom_retype_failure(bool required) {
  if (required) {
    runtime_firebloom_required_retype_failure();
  }
  return runtime_firebloom_prepare_invalid_span_candidate();
}

RuntimeFirebloomRetypeCandidate RUNTIME_FIREBLOOM_RANGES_PUBLIC
runtime_firebloom_try_retype_span(
    RuntimeFirebloomRetypeSpan source,
    size_t offset,
    size_t size,
    RuntimeFirebloomRangeType *target,
    RuntimeFirebloomTypeRelationSet *relations,
    bool required
) {
  source = runtime_firebloom_tuple_validate(source);
  const uintptr_t current = runtime_firebloom_range_address(source.current);
  const uintptr_t upper = runtime_firebloom_range_address(source.upper_bound);
  if (offset > upper - current || size > upper - current - offset) {
    return runtime_firebloom_retype_failure(required);
  }

  RuntimeFirebloomRangeType *source_type =
      runtime_firebloom_type_canonicalize(source.type);
  target = runtime_firebloom_type_canonicalize(target);
  const size_t target_size = runtime_firebloom_type_element_size(target);
  if ((target_size == 0 && size != 0) ||
      (target_size != 0 && size % target_size != 0)) {
    return runtime_firebloom_retype_failure(required);
  }

  RuntimeFirebloomRetypeKind kind = RUNTIME_FIREBLOOM_RETYPE_INVALID;
  if (source_type == target ||
      (source_type->relation.identity != 0 &&
       source_type->relation.identity == target->relation.identity)) {
    kind = RUNTIME_FIREBLOOM_RETYPE_EXACT;
  } else if (relations != NULL && runtime_firebloom_type_relation_resolve(
                                      relations, source_type, target, true) ==
                                      RUNTIME_FIREBLOOM_RELATION_EQUIVALENT) {
    kind = RUNTIME_FIREBLOOM_RETYPE_STRUCTURAL;
  } else if (runtime_firebloom_type_is_bytewise_clone(source_type) &&
             runtime_firebloom_type_is_bytewise_clone(target)) {
    kind = RUNTIME_FIREBLOOM_RETYPE_STRUCTURAL;
  } else {
    RuntimeFirebloomRangeType *member = NULL;
    for (uint32_t index = 0; index < source_type->member_count; ++index) {
      if (source_type->members[index].offset == offset) {
        member = source_type->members[index].type;
        break;
      }
    }
    if (member != NULL &&
        runtime_firebloom_type_canonicalize(member) == target) {
      kind = RUNTIME_FIREBLOOM_RETYPE_STRUCTURAL;
    }
  }
  if (kind == RUNTIME_FIREBLOOM_RETYPE_INVALID) {
    return runtime_firebloom_retype_failure(required);
  }

  const uintptr_t result_address = current + offset;
  RuntimeFirebloomRetypeSpan result = {
      .current = (void *)result_address,
      .lower_bound = (void *)result_address,
      .upper_bound = (void *)(result_address + size),
      .type = target,
  };
  return (RuntimeFirebloomRetypeCandidate){
      .span = result,
      .kind = kind,
  };
}

RuntimeFirebloomRetypeCandidate RUNTIME_FIREBLOOM_RANGES_PUBLIC
runtime_firebloom_tuple_retype_slow(
    RuntimeFirebloomRetypeSpan source,
    RuntimeFirebloomRangeType *target,
    bool required
) {
  source = runtime_firebloom_tuple_validate(source);
  const size_t remaining = runtime_firebloom_range_address(source.upper_bound) -
                           runtime_firebloom_range_address(source.current);
  return runtime_firebloom_try_retype_span(
      source,
      0,
      remaining,
      target,
      NULL,
      required
  );
}
