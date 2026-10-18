#include "avp/runtime/firebloom/runtime_firebloom_tuples_recovered.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


static _Noreturn void runtime_firebloom_tuple_trap(void) {
  __builtin_trap();
}

static RuntimeFirebloomTuple runtime_firebloom_clear_tuple(void) {
  return (RuntimeFirebloomTuple){0};
}

static bool runtime_firebloom_current_reaches_upper(
    const RuntimeFirebloomTuple *tuple) {
  if (tuple == NULL) {
    runtime_firebloom_tuple_trap();
  }
  return (uintptr_t)tuple->current >= (uintptr_t)tuple->upper_bound;
}

static RuntimeFirebloomTuple runtime_firebloom_restore_tuple(
    RuntimeFirebloomTupleBounds bounds,
    RuntimeFirebloomRangeType *type
) {
  if (type == NULL || (uintptr_t)bounds.lower_bound >
                          (uintptr_t)bounds.current ||
      (uintptr_t)bounds.current > (uintptr_t)bounds.upper_bound) {
    runtime_firebloom_tuple_trap();
  }
  return (RuntimeFirebloomTuple){bounds.current, bounds.lower_bound,
                                bounds.upper_bound, type};
}

RuntimeFirebloomTuple RUNTIME_FIREBLOOM_TUPLES_PUBLIC
runtime_firebloom_prepare_sp08_result_tuple(void) {
  return runtime_firebloom_clear_tuple();
}

RuntimeFirebloomRangeType *RUNTIME_FIREBLOOM_TUPLES_PUBLIC
runtime_firebloom_load_byte_type_to_x4(void) {
  return &runtime_firebloom_tuple_byte_type;
}

RuntimeFirebloomTuple RUNTIME_FIREBLOOM_TUPLES_PUBLIC
runtime_firebloom_prepare_primary_result_tuple(void) {
  return runtime_firebloom_clear_tuple();
}

RuntimeFirebloomTuple RUNTIME_FIREBLOOM_TUPLES_PUBLIC
runtime_firebloom_prepare_secondary_result_tuple(void) {
  return runtime_firebloom_clear_tuple();
}

bool RUNTIME_FIREBLOOM_TUPLES_PUBLIC
runtime_firebloom_compare_primary_current_to_upper(
    const RuntimeFirebloomTuple *primary) {
  return runtime_firebloom_current_reaches_upper(primary);
}

RuntimeFirebloomTuple RUNTIME_FIREBLOOM_TUPLES_PUBLIC
runtime_firebloom_zero_primary_tuple_arguments(void) {
  return runtime_firebloom_clear_tuple();
}

bool RUNTIME_FIREBLOOM_TUPLES_PUBLIC
runtime_firebloom_compare_secondary_current_to_upper(
    const RuntimeFirebloomTuple *secondary) {
  return runtime_firebloom_current_reaches_upper(secondary);
}

RuntimeFirebloomTupleBounds RUNTIME_FIREBLOOM_TUPLES_PUBLIC
runtime_firebloom_restore_saved_primary_bounds(
    RuntimeFirebloomTupleBounds saved) {
  return saved;
}

RuntimeFirebloomTuple RUNTIME_FIREBLOOM_TUPLES_PUBLIC
runtime_firebloom_prepare_frame_tuple_80(void) {
  return runtime_firebloom_clear_tuple();
}

RuntimeFirebloomTuple RUNTIME_FIREBLOOM_TUPLES_PUBLIC
runtime_firebloom_prepare_frame_tuple_88(void) {
  return runtime_firebloom_clear_tuple();
}

bool RUNTIME_FIREBLOOM_TUPLES_PUBLIC
runtime_firebloom_compare_frame_tuple_80_current_to_upper(
    const RuntimeFirebloomTuple *tuple) {
  return runtime_firebloom_current_reaches_upper(tuple);
}

RuntimeFirebloomTupleCurrentUpper RUNTIME_FIREBLOOM_TUPLES_PUBLIC
runtime_firebloom_load_primary_current_and_upper(
    const RuntimeFirebloomTuple *primary) {
  if (primary == NULL) {
    runtime_firebloom_tuple_trap();
  }
  return (RuntimeFirebloomTupleCurrentUpper){primary->current,
                                            primary->upper_bound};
}

RuntimeFirebloomByteSpanArguments RUNTIME_FIREBLOOM_TUPLES_PUBLIC
runtime_firebloom_outlined_prepare_byte_span_and_clear_flag(
    void *current,
    void *upper_bound
) {
  if (current == NULL || (uintptr_t)current > (uintptr_t)upper_bound) {
    runtime_firebloom_tuple_trap();
  }
  return (RuntimeFirebloomByteSpanArguments){
      .span = {current, current, upper_bound,
               &runtime_firebloom_tuple_byte_type},
      .flag = false,
  };
}

RuntimeFirebloomTuple RUNTIME_FIREBLOOM_TUPLES_PUBLIC
runtime_firebloom_prepare_sp50_result_tuple(void) {
  return runtime_firebloom_clear_tuple();
}

bool RUNTIME_FIREBLOOM_TUPLES_PUBLIC
runtime_firebloom_compare_sp50_current_to_upper(
    const RuntimeFirebloomTuple *tuple) {
  return runtime_firebloom_current_reaches_upper(tuple);
}

RuntimeFirebloomTupleBounds RUNTIME_FIREBLOOM_TUPLES_PUBLIC
runtime_firebloom_save_primary_bounds_x22_x24(RuntimeFirebloomTuple primary) {
  return (RuntimeFirebloomTupleBounds){primary.current, primary.lower_bound,
                                      primary.upper_bound};
}

RuntimeFirebloomTuple RUNTIME_FIREBLOOM_TUPLES_PUBLIC
runtime_firebloom_duplicate_x19_current_lower(
    void *current,
    void *upper_bound,
    RuntimeFirebloomRangeType *type
) {
  return runtime_firebloom_restore_tuple(
      (RuntimeFirebloomTupleBounds){current, current, upper_bound},
      type
  );
}

RuntimeFirebloomTuple RUNTIME_FIREBLOOM_TUPLES_PUBLIC
runtime_firebloom_restore_x19_x21_x20_bounds(
    RuntimeFirebloomTupleBounds saved,
    RuntimeFirebloomRangeType *type
) {
  return runtime_firebloom_restore_tuple(saved, type);
}

RuntimeFirebloomTuple RUNTIME_FIREBLOOM_TUPLES_PUBLIC
runtime_firebloom_restore_x19_x23_x21_bounds(
    RuntimeFirebloomTupleBounds saved,
    RuntimeFirebloomRangeType *type
) {
  return runtime_firebloom_restore_tuple(saved, type);
}

RuntimeFirebloomTuple RUNTIME_FIREBLOOM_TUPLES_PUBLIC
runtime_firebloom_prepare_sp40_result_tuple(void) {
  return runtime_firebloom_clear_tuple();
}

RuntimeFirebloomTuple RUNTIME_FIREBLOOM_TUPLES_PUBLIC
runtime_firebloom_prepare_sp78_result_tuple(void) {
  return runtime_firebloom_clear_tuple();
}

RuntimeFirebloomRangeType *RUNTIME_FIREBLOOM_TUPLES_PUBLIC
runtime_firebloom_outlined_load_qword_type_to_x3(void) {
  return &runtime_firebloom_tuple_qword_type;
}

RuntimeFirebloomTuple RUNTIME_FIREBLOOM_TUPLES_PUBLIC
runtime_firebloom_save_secondary_tuple(RuntimeFirebloomTuple secondary) {
  return secondary;
}

RuntimeFirebloomTuple RUNTIME_FIREBLOOM_TUPLES_PUBLIC
runtime_firebloom_copy_primary_bounds_to_secondary(
    void *secondary_current,
    RuntimeFirebloomTuple primary
) {
  if (secondary_current == NULL ||
      (uintptr_t)secondary_current < (uintptr_t)primary.lower_bound ||
      (uintptr_t)secondary_current > (uintptr_t)primary.upper_bound) {
    runtime_firebloom_tuple_trap();
  }
  primary.current = secondary_current;
  return primary;
}

RuntimeFirebloomTuple RUNTIME_FIREBLOOM_TUPLES_PUBLIC
runtime_firebloom_prepare_primary_result_tuple_clone(void) {
  return runtime_firebloom_clear_tuple();
}

bool RUNTIME_FIREBLOOM_TUPLES_PUBLIC
runtime_firebloom_compare_primary_current_to_upper_clone(
    const RuntimeFirebloomTuple *primary) {
  return runtime_firebloom_current_reaches_upper(primary);
}

RuntimeFirebloomRangeType *RUNTIME_FIREBLOOM_TUPLES_PUBLIC
runtime_firebloom_load_byte_type_to_x7(void) {
  return &runtime_firebloom_tuple_byte_type;
}

size_t RUNTIME_FIREBLOOM_TUPLES_PUBLIC
runtime_firebloom_prepare_16_byte_count(void) {
  return 16;
}

bool RUNTIME_FIREBLOOM_TUPLES_PUBLIC
runtime_firebloom_compare_frame_tuple_88_current_to_upper(
    const RuntimeFirebloomTuple *tuple) {
  return runtime_firebloom_current_reaches_upper(tuple);
}

RuntimeFirebloomTuple RUNTIME_FIREBLOOM_TUPLES_PUBLIC
runtime_firebloom_restore_sp00_current_upper(
    void *current,
    void *upper_bound,
    RuntimeFirebloomRangeType *type
) {
  return runtime_firebloom_duplicate_x19_current_lower(
      current,
      upper_bound,
      type
  );
}

RuntimeFirebloomTuple RUNTIME_FIREBLOOM_TUPLES_PUBLIC
runtime_firebloom_restore_frame_tuple_80_current_upper(
    void *current,
    void *upper_bound,
    RuntimeFirebloomRangeType *type
) {
  return runtime_firebloom_duplicate_x19_current_lower(
      current,
      upper_bound,
      type
  );
}

RuntimeFirebloomRegisterTriplet RUNTIME_FIREBLOOM_TUPLES_PUBLIC
runtime_restore_saved_x19_x21_arguments(
    uintptr_t x19,
    uintptr_t x20,
    uintptr_t x21
) {
  return (RuntimeFirebloomRegisterTriplet){x19, x20, x21};
}
