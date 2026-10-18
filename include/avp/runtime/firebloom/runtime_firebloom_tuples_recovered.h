#pragma once

#include "avp/runtime/firebloom/runtime_firebloom_ranges_recovered.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if __has_attribute(retain)
#define RUNTIME_FIREBLOOM_TUPLES_PUBLIC __attribute__((used, retain))
#else
#define RUNTIME_FIREBLOOM_TUPLES_PUBLIC __attribute__((used))
#endif

typedef RuntimeFirebloomRetypeSpan RuntimeFirebloomTuple;

typedef struct {
  void *current;
  void *upper_bound;
} RuntimeFirebloomTupleCurrentUpper;

typedef struct {
  void *current;
  void *lower_bound;
  void *upper_bound;
} RuntimeFirebloomTupleBounds;

typedef struct {
  RuntimeFirebloomTuple span;
  bool flag;
} RuntimeFirebloomByteSpanArguments;

typedef struct {
  uintptr_t first;
  uintptr_t second;
  uintptr_t third;
} RuntimeFirebloomRegisterTriplet;

extern RuntimeFirebloomRangeType runtime_firebloom_tuple_character_type;
extern RuntimeFirebloomRangeType runtime_firebloom_tuple_byte_type;
extern RuntimeFirebloomRangeType runtime_firebloom_tuple_qword_validation_type;
extern RuntimeFirebloomRangeType runtime_firebloom_tuple_qword_type;
extern RuntimeFirebloomRangeType runtime_firebloom_tuple_dispatch_type;

RUNTIME_FIREBLOOM_TUPLES_PUBLIC RuntimeFirebloomTuple
runtime_firebloom_empty_span(void);
RUNTIME_FIREBLOOM_TUPLES_PUBLIC RuntimeFirebloomTuple
runtime_firebloom_make_cstring_span_bounded(
    const char *string,
    size_t maximum_length
);
RUNTIME_FIREBLOOM_TUPLES_PUBLIC RuntimeFirebloomTuple
runtime_firebloom_make_cstring_span(const char *string);
RUNTIME_FIREBLOOM_TUPLES_PUBLIC RuntimeFirebloomTuple
runtime_firebloom_make_five_byte_cstring_span(const char *current);
RUNTIME_FIREBLOOM_TUPLES_PUBLIC RuntimeFirebloomTuple
runtime_firebloom_return_empty_tuple(void);
RUNTIME_FIREBLOOM_TUPLES_PUBLIC RuntimeFirebloomTuple
runtime_firebloom_make_qword_slot_span(uint64_t *slot);
RUNTIME_FIREBLOOM_TUPLES_PUBLIC RuntimeFirebloomTuple
runtime_firebloom_make_byte_slot_span(uint8_t *slot);
RUNTIME_FIREBLOOM_TUPLES_PUBLIC RuntimeFirebloomTuple
runtime_firebloom_make_qword_slot_span_clone(uint64_t *slot);

RUNTIME_FIREBLOOM_TUPLES_PUBLIC RuntimeFirebloomTuple
runtime_firebloom_prepare_sp08_result_tuple(void);
RUNTIME_FIREBLOOM_TUPLES_PUBLIC RuntimeFirebloomRangeType *
runtime_firebloom_load_byte_type_to_x4(void);
RUNTIME_FIREBLOOM_TUPLES_PUBLIC RuntimeFirebloomTuple
runtime_firebloom_prepare_primary_result_tuple(void);
RUNTIME_FIREBLOOM_TUPLES_PUBLIC RuntimeFirebloomTuple
runtime_firebloom_prepare_secondary_result_tuple(void);
RUNTIME_FIREBLOOM_TUPLES_PUBLIC bool
runtime_firebloom_compare_primary_current_to_upper(
    const RuntimeFirebloomTuple *primary);
RUNTIME_FIREBLOOM_TUPLES_PUBLIC RuntimeFirebloomTuple
runtime_firebloom_zero_primary_tuple_arguments(void);
RUNTIME_FIREBLOOM_TUPLES_PUBLIC bool
runtime_firebloom_compare_secondary_current_to_upper(
    const RuntimeFirebloomTuple *secondary);
RUNTIME_FIREBLOOM_TUPLES_PUBLIC RuntimeFirebloomTupleBounds
runtime_firebloom_restore_saved_primary_bounds(
    RuntimeFirebloomTupleBounds saved);
RUNTIME_FIREBLOOM_TUPLES_PUBLIC RuntimeFirebloomTuple
runtime_firebloom_prepare_frame_tuple_80(void);
RUNTIME_FIREBLOOM_TUPLES_PUBLIC RuntimeFirebloomTuple
runtime_firebloom_prepare_frame_tuple_88(void);
RUNTIME_FIREBLOOM_TUPLES_PUBLIC bool
runtime_firebloom_compare_frame_tuple_80_current_to_upper(
    const RuntimeFirebloomTuple *tuple);
RUNTIME_FIREBLOOM_TUPLES_PUBLIC RuntimeFirebloomTupleCurrentUpper
runtime_firebloom_load_primary_current_and_upper(
    const RuntimeFirebloomTuple *primary);
RUNTIME_FIREBLOOM_TUPLES_PUBLIC RuntimeFirebloomByteSpanArguments
runtime_firebloom_outlined_prepare_byte_span_and_clear_flag(
    void *current,
    void *upper_bound
);
RUNTIME_FIREBLOOM_TUPLES_PUBLIC RuntimeFirebloomTuple
runtime_firebloom_prepare_sp50_result_tuple(void);
RUNTIME_FIREBLOOM_TUPLES_PUBLIC bool
runtime_firebloom_compare_sp50_current_to_upper(
    const RuntimeFirebloomTuple *tuple);
RUNTIME_FIREBLOOM_TUPLES_PUBLIC RuntimeFirebloomTupleBounds
runtime_firebloom_save_primary_bounds_x22_x24(RuntimeFirebloomTuple primary);
RUNTIME_FIREBLOOM_TUPLES_PUBLIC RuntimeFirebloomTuple
runtime_firebloom_duplicate_x19_current_lower(
    void *current,
    void *upper_bound,
    RuntimeFirebloomRangeType *type
);
RUNTIME_FIREBLOOM_TUPLES_PUBLIC RuntimeFirebloomTuple
runtime_firebloom_restore_x19_x21_x20_bounds(
    RuntimeFirebloomTupleBounds saved,
    RuntimeFirebloomRangeType *type
);
RUNTIME_FIREBLOOM_TUPLES_PUBLIC RuntimeFirebloomTuple
runtime_firebloom_restore_x19_x23_x21_bounds(
    RuntimeFirebloomTupleBounds saved,
    RuntimeFirebloomRangeType *type
);
RUNTIME_FIREBLOOM_TUPLES_PUBLIC RuntimeFirebloomTuple
runtime_firebloom_prepare_sp40_result_tuple(void);
RUNTIME_FIREBLOOM_TUPLES_PUBLIC RuntimeFirebloomTuple
runtime_firebloom_prepare_sp78_result_tuple(void);
RUNTIME_FIREBLOOM_TUPLES_PUBLIC RuntimeFirebloomRangeType *
runtime_firebloom_outlined_load_qword_type_to_x3(void);
RUNTIME_FIREBLOOM_TUPLES_PUBLIC RuntimeFirebloomTuple
runtime_firebloom_save_secondary_tuple(RuntimeFirebloomTuple secondary);
RUNTIME_FIREBLOOM_TUPLES_PUBLIC RuntimeFirebloomTuple
runtime_firebloom_copy_primary_bounds_to_secondary(
    void *secondary_current,
    RuntimeFirebloomTuple primary
);
RUNTIME_FIREBLOOM_TUPLES_PUBLIC RuntimeFirebloomTuple
runtime_firebloom_prepare_primary_result_tuple_clone(void);
RUNTIME_FIREBLOOM_TUPLES_PUBLIC bool
runtime_firebloom_compare_primary_current_to_upper_clone(
    const RuntimeFirebloomTuple *primary);
RUNTIME_FIREBLOOM_TUPLES_PUBLIC RuntimeFirebloomRangeType *
runtime_firebloom_load_byte_type_to_x7(void);
RUNTIME_FIREBLOOM_TUPLES_PUBLIC size_t
runtime_firebloom_prepare_16_byte_count(void);
RUNTIME_FIREBLOOM_TUPLES_PUBLIC bool
runtime_firebloom_compare_frame_tuple_88_current_to_upper(
    const RuntimeFirebloomTuple *tuple);
RUNTIME_FIREBLOOM_TUPLES_PUBLIC RuntimeFirebloomTuple
runtime_firebloom_restore_sp00_current_upper(
    void *current,
    void *upper_bound,
    RuntimeFirebloomRangeType *type
);
RUNTIME_FIREBLOOM_TUPLES_PUBLIC RuntimeFirebloomTuple
runtime_firebloom_restore_frame_tuple_80_current_upper(
    void *current,
    void *upper_bound,
    RuntimeFirebloomRangeType *type
);
RUNTIME_FIREBLOOM_TUPLES_PUBLIC RuntimeFirebloomRegisterTriplet
runtime_restore_saved_x19_x21_arguments(
    uintptr_t x19,
    uintptr_t x20,
    uintptr_t x21
);

RUNTIME_FIREBLOOM_TUPLES_PUBLIC RuntimeFirebloomTuple
runtime_firebloom_validate_qword_tuple(
    void *current,
    const void *lower_bound,
    const void *upper_bound,
    RuntimeFirebloomRangeType *type_context
);
RUNTIME_FIREBLOOM_TUPLES_PUBLIC RuntimeFirebloomTuple
runtime_firebloom_validate_byte_tuple(
    void *current,
    const void *lower_bound,
    const void *upper_bound,
    RuntimeFirebloomRangeType *type_context
);
RUNTIME_FIREBLOOM_TUPLES_PUBLIC bool
runtime_firebloom_validate_dispatch_tuple_compare_current_to_lower(
    void *current,
    const void *lower_bound,
    const void *upper_bound,
    RuntimeFirebloomRangeType *type_context
);
