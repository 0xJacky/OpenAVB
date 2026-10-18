#include "avp/runtime/firebloom/runtime_firebloom_tuples_recovered.h"

#include <assert.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>


static void expect_trap(void (*operation)(void)) {
  const pid_t child = fork();
  assert(child >= 0);
  if (child == 0) {
    operation();
    _exit(0);
  }
  int status = 0;
  assert(waitpid(child, &status, 0) == child);
  assert(WIFSIGNALED(status));
  assert(WTERMSIG(status) == SIGILL || WTERMSIG(status) == SIGTRAP);
}

static void assert_empty(RuntimeFirebloomTuple tuple) {
  assert(tuple.current == NULL);
  assert(tuple.lower_bound == NULL);
  assert(tuple.upper_bound == NULL);
  assert(tuple.type == NULL);
}

static void test_constructors(void) {
  assert_empty(runtime_firebloom_empty_span());
  assert_empty(runtime_firebloom_return_empty_tuple());
  assert_empty(runtime_firebloom_make_cstring_span(NULL));

  char text[] = "abcd";
  RuntimeFirebloomTuple span =
      runtime_firebloom_make_cstring_span_bounded(text, 2);
  assert(span.current == text && span.lower_bound == text);
  assert(span.upper_bound == text + 2);
  assert(span.type == &runtime_firebloom_tuple_character_type);
  span = runtime_firebloom_make_cstring_span_bounded(text, sizeof(text));
  assert(span.upper_bound == text + sizeof(text));
  span = runtime_firebloom_make_cstring_span(text);
  assert(span.upper_bound == text + sizeof(text));

  span = runtime_firebloom_make_five_byte_cstring_span(text);
  assert(span.current == text && span.upper_bound == text + 5);
  assert(span.type == &runtime_firebloom_tuple_byte_type);

  uint8_t byte_slot = 0;
  span = runtime_firebloom_make_byte_slot_span(&byte_slot);
  assert(span.current == &byte_slot);
  assert(span.upper_bound == &byte_slot + 1);
  assert(span.type == &runtime_firebloom_tuple_byte_type);

  uint64_t qword_slot = 0;
  span = runtime_firebloom_make_qword_slot_span(&qword_slot);
  assert(span.current == &qword_slot);
  assert(span.upper_bound == &qword_slot + 1);
  assert(span.type == &runtime_firebloom_tuple_qword_type);
  RuntimeFirebloomTuple clone =
      runtime_firebloom_make_qword_slot_span_clone(&qword_slot);
  assert(clone.current == span.current && clone.type == span.type);
}

static RuntimeFirebloomTuple sample_tuple(uint8_t storage[16]) {
  return (RuntimeFirebloomTuple){
      .current = storage + 4,
      .lower_bound = storage,
      .upper_bound = storage + 12,
      .type = &runtime_firebloom_tuple_byte_type,
  };
}

static void test_tuple_outliners(void) {
  assert_empty(runtime_firebloom_prepare_sp08_result_tuple());
  assert_empty(runtime_firebloom_prepare_primary_result_tuple());
  assert_empty(runtime_firebloom_prepare_secondary_result_tuple());
  assert_empty(runtime_firebloom_zero_primary_tuple_arguments());
  assert_empty(runtime_firebloom_prepare_frame_tuple_80());
  assert_empty(runtime_firebloom_prepare_frame_tuple_88());
  assert_empty(runtime_firebloom_prepare_sp50_result_tuple());
  assert_empty(runtime_firebloom_prepare_sp40_result_tuple());
  assert_empty(runtime_firebloom_prepare_sp78_result_tuple());
  assert_empty(runtime_firebloom_prepare_primary_result_tuple_clone());

  assert(runtime_firebloom_load_byte_type_to_x4() ==
         &runtime_firebloom_tuple_byte_type);
  assert(runtime_firebloom_load_byte_type_to_x7() ==
         &runtime_firebloom_tuple_byte_type);
  assert(runtime_firebloom_outlined_load_qword_type_to_x3() ==
         &runtime_firebloom_tuple_qword_type);
  assert(runtime_firebloom_prepare_16_byte_count() == 16);

  uint8_t storage[16] = {0};
  RuntimeFirebloomTuple tuple = sample_tuple(storage);
  assert(!runtime_firebloom_compare_primary_current_to_upper(&tuple));
  assert(!runtime_firebloom_compare_secondary_current_to_upper(&tuple));
  assert(!runtime_firebloom_compare_frame_tuple_80_current_to_upper(&tuple));
  assert(!runtime_firebloom_compare_sp50_current_to_upper(&tuple));
  assert(!runtime_firebloom_compare_primary_current_to_upper_clone(&tuple));
  assert(!runtime_firebloom_compare_frame_tuple_88_current_to_upper(&tuple));
  tuple.current = tuple.upper_bound;
  assert(runtime_firebloom_compare_primary_current_to_upper(&tuple));
  assert(runtime_firebloom_compare_secondary_current_to_upper(&tuple));
  assert(runtime_firebloom_compare_frame_tuple_80_current_to_upper(&tuple));
  assert(runtime_firebloom_compare_sp50_current_to_upper(&tuple));
  assert(runtime_firebloom_compare_primary_current_to_upper_clone(&tuple));
  assert(runtime_firebloom_compare_frame_tuple_88_current_to_upper(&tuple));

  tuple = sample_tuple(storage);
  RuntimeFirebloomTupleCurrentUpper pair =
      runtime_firebloom_load_primary_current_and_upper(&tuple);
  assert(pair.current == tuple.current && pair.upper_bound == tuple.upper_bound);
  RuntimeFirebloomByteSpanArguments arguments =
      runtime_firebloom_outlined_prepare_byte_span_and_clear_flag(
          storage + 2, storage + 10);
  assert(arguments.span.current == storage + 2);
  assert(arguments.span.lower_bound == storage + 2);
  assert(arguments.span.upper_bound == storage + 10);
  assert(arguments.span.type == &runtime_firebloom_tuple_byte_type);
  assert(!arguments.flag);

  RuntimeFirebloomTupleBounds bounds =
      runtime_firebloom_save_primary_bounds_x22_x24(tuple);
  RuntimeFirebloomTupleBounds restored =
      runtime_firebloom_restore_saved_primary_bounds(bounds);
  assert(restored.current == tuple.current);
  assert(restored.lower_bound == tuple.lower_bound);
  assert(restored.upper_bound == tuple.upper_bound);
  RuntimeFirebloomTuple rebuilt =
      runtime_firebloom_restore_x19_x21_x20_bounds(
          restored, &runtime_firebloom_tuple_byte_type);
  assert(rebuilt.current == tuple.current && rebuilt.type == tuple.type);
  rebuilt = runtime_firebloom_restore_x19_x23_x21_bounds(
      restored, &runtime_firebloom_tuple_byte_type);
  assert(rebuilt.lower_bound == tuple.lower_bound);

  rebuilt = runtime_firebloom_duplicate_x19_current_lower(
      storage + 3, storage + 9, &runtime_firebloom_tuple_byte_type);
  assert(rebuilt.current == storage + 3 && rebuilt.lower_bound == storage + 3);
  rebuilt = runtime_firebloom_restore_sp00_current_upper(
      storage + 3, storage + 9, &runtime_firebloom_tuple_byte_type);
  assert(rebuilt.lower_bound == rebuilt.current);
  rebuilt = runtime_firebloom_restore_frame_tuple_80_current_upper(
      storage + 3, storage + 9, &runtime_firebloom_tuple_byte_type);
  assert(rebuilt.lower_bound == rebuilt.current);

  rebuilt = runtime_firebloom_copy_primary_bounds_to_secondary(
      storage + 8, tuple);
  assert(rebuilt.current == storage + 8);
  assert(rebuilt.lower_bound == tuple.lower_bound);
  assert(rebuilt.upper_bound == tuple.upper_bound);
  RuntimeFirebloomTuple saved_secondary =
      runtime_firebloom_save_secondary_tuple(rebuilt);
  assert(saved_secondary.current == rebuilt.current);
}

static void test_validators(void) {
  uint64_t qwords[2] = {0};
  RuntimeFirebloomTuple tuple = runtime_firebloom_validate_qword_tuple(
      qwords, qwords, qwords + 2,
      &runtime_firebloom_tuple_qword_validation_type);
  assert(tuple.current == qwords);
  assert(tuple.type == &runtime_firebloom_tuple_qword_validation_type);

  uint8_t bytes[0xf8] = {0};
  tuple = runtime_firebloom_validate_byte_tuple(
      bytes + 1, bytes, bytes + sizeof(bytes),
      &runtime_firebloom_tuple_byte_type);
  assert(tuple.current == bytes + 1);
  assert(tuple.type == &runtime_firebloom_tuple_byte_type);
  assert(runtime_firebloom_validate_dispatch_tuple_compare_current_to_lower(
      bytes, bytes, bytes + sizeof(bytes),
      &runtime_firebloom_tuple_dispatch_type));

  RuntimeFirebloomRangeType byte_alias = runtime_firebloom_tuple_byte_type;
  byte_alias.relation.parent_and_kind =
      (uintptr_t)&runtime_firebloom_tuple_byte_type |
      RUNTIME_FIREBLOOM_RANGE_KIND_BYTEWISE;
  tuple = runtime_firebloom_validate_byte_tuple(
      bytes, bytes, bytes + sizeof(bytes), &byte_alias);
  assert(tuple.type == &runtime_firebloom_tuple_byte_type);
}

static uint8_t fatal_storage[16];

static void fatal_bad_type(void) {
  (void)runtime_firebloom_validate_byte_tuple(
      fatal_storage, fatal_storage, fatal_storage + sizeof(fatal_storage),
      &runtime_firebloom_tuple_qword_type);
}

static void fatal_bad_bounds(void) {
  (void)runtime_firebloom_validate_byte_tuple(
      fatal_storage + 8, fatal_storage + 9,
      fatal_storage + sizeof(fatal_storage),
      &runtime_firebloom_tuple_byte_type);
}

static void fatal_bad_slot(void) {
  (void)runtime_firebloom_make_qword_slot_span(NULL);
}

static void fatal_secondary_outside_bounds(void) {
  RuntimeFirebloomTuple tuple = sample_tuple(fatal_storage);
  (void)runtime_firebloom_copy_primary_bounds_to_secondary(
      fatal_storage + sizeof(fatal_storage), tuple);
}

static void fatal_bad_dispatch_type(void) {
  (void)runtime_firebloom_validate_dispatch_tuple_compare_current_to_lower(
      fatal_storage, fatal_storage, fatal_storage + sizeof(fatal_storage),
      &runtime_firebloom_tuple_byte_type);
}

int main(void) {
  RuntimeFirebloomRegisterTriplet restored =
      runtime_restore_saved_x19_x21_arguments(19, 20, 21);
  assert(restored.first == 19 && restored.second == 20 &&
         restored.third == 21);
  test_constructors();
  test_tuple_outliners();
  test_validators();
  expect_trap(fatal_bad_type);
  expect_trap(fatal_bad_bounds);
  expect_trap(fatal_bad_slot);
  expect_trap(fatal_secondary_outside_bounds);
  expect_trap(fatal_bad_dispatch_type);
  puts("runtime Firebloom tuple tests passed");
  return 0;
}
