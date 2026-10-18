#include "avp/runtime/firebloom/runtime_firebloom_tuples_recovered.h"

#include <stdbool.h>
#include <stdint.h>


static _Noreturn void runtime_firebloom_tuple_trap(void) {
  __builtin_trap();
}

static RuntimeFirebloomTuple runtime_firebloom_validate_tuple_as(
    RuntimeFirebloomTuple tuple,
    RuntimeFirebloomRangeType *expected
) {
  tuple = runtime_firebloom_tuple_validate(tuple);
  RuntimeFirebloomRangeType *actual =
      runtime_firebloom_type_canonicalize(tuple.type);
  expected = runtime_firebloom_type_canonicalize(expected);
  if (actual != expected &&
      (actual->relation.identity == 0 ||
       actual->relation.identity != expected->relation.identity)) {
    runtime_firebloom_tuple_trap();
  }
  tuple.type = expected;
  return tuple;
}

RuntimeFirebloomTuple RUNTIME_FIREBLOOM_TUPLES_PUBLIC
runtime_firebloom_validate_qword_tuple(
    void *current,
    const void *lower_bound,
    const void *upper_bound,
    RuntimeFirebloomRangeType *type_context
) {
  return runtime_firebloom_validate_tuple_as(
      (RuntimeFirebloomTuple){current, (void *)(uintptr_t)lower_bound,
                             (void *)(uintptr_t)upper_bound, type_context},
      &runtime_firebloom_tuple_qword_validation_type
  );
}

RuntimeFirebloomTuple RUNTIME_FIREBLOOM_TUPLES_PUBLIC
runtime_firebloom_validate_byte_tuple(
    void *current,
    const void *lower_bound,
    const void *upper_bound,
    RuntimeFirebloomRangeType *type_context
) {
  return runtime_firebloom_validate_tuple_as(
      (RuntimeFirebloomTuple){current, (void *)(uintptr_t)lower_bound,
                             (void *)(uintptr_t)upper_bound, type_context},
      &runtime_firebloom_tuple_byte_type
  );
}

bool RUNTIME_FIREBLOOM_TUPLES_PUBLIC
runtime_firebloom_validate_dispatch_tuple_compare_current_to_lower(
    void *current,
    const void *lower_bound,
    const void *upper_bound,
    RuntimeFirebloomRangeType *type_context
) {
  const RuntimeFirebloomTuple tuple = runtime_firebloom_validate_tuple_as(
      (RuntimeFirebloomTuple){current, (void *)(uintptr_t)lower_bound,
                             (void *)(uintptr_t)upper_bound, type_context},
      &runtime_firebloom_tuple_dispatch_type
  );
  return (uintptr_t)tuple.current >= (uintptr_t)tuple.lower_bound;
}
