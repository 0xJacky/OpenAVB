#include "avp/runtime/firebloom/runtime_firebloom_ranges_recovered.h"

#include <assert.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

static void *relation_allocate(size_t size, void *context) {
  (void)context;
  return calloc(1, size);
}

static void relation_deallocate(void *pointer, void *context) {
  (void)context;
  free(pointer);
}

static RuntimeFirebloomRelationResult
relation_unknown(const RuntimeFirebloomRelationType *left,
                 const RuntimeFirebloomRelationType *right, bool directed,
                 void *context) {
  (void)left;
  (void)right;
  (void)directed;
  (void)context;
  return RUNTIME_FIREBLOOM_RELATION_UNKNOWN;
}

static RuntimeFirebloomRangeType range_type(RuntimeFirebloomRangeKind kind,
                                            uint64_t identity,
                                            uint32_t element_size,
                                            uint32_t pointer_count) {
  return (RuntimeFirebloomRangeType){
      .relation =
          {
              .parent_and_kind = kind,
              .identity = identity,
          },
      .element_size = element_size,
      .pointer_count = pointer_count,
      .alignment = element_size == 0 ? 1 : element_size,
  };
}

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

static RuntimeFirebloomRangeType byte_type;
static RuntimeFirebloomRangeType aggregate_type;

static void required_failure(void) {
  runtime_firebloom_required_retype_failure();
}

static void invalid_span(void) {
  uint8_t bytes[8];
  RuntimeFirebloomRetypeSpan span = {
      .current = bytes + 7,
      .lower_bound = bytes,
      .upper_bound = bytes + 4,
      .type = &byte_type,
  };
  (void)runtime_firebloom_tuple_validate(span);
}

static void invalid_indirect(void) {
  (void)runtime_firebloom_require_indirect_type_header(&aggregate_type);
}

static void test_type_geometry_and_canonicalization(void) {
  RuntimeFirebloomRangeType bytes =
      range_type(RUNTIME_FIREBLOOM_RANGE_KIND_BYTEWISE, 1, 1, 0);
  RuntimeFirebloomRangeType words =
      range_type(RUNTIME_FIREBLOOM_RANGE_KIND_BYTEWISE, 2, 4, 0);
  RuntimeFirebloomRangeType zero =
      range_type(RUNTIME_FIREBLOOM_RANGE_KIND_OPAQUE, 3, 0, 0);
  RuntimeFirebloomRangeType alias =
      range_type(RUNTIME_FIREBLOOM_RANGE_KIND_BYTEWISE, 4, 1, 0);
  alias.relation.parent_and_kind =
      (uintptr_t)&bytes | RUNTIME_FIREBLOOM_RANGE_KIND_BYTEWISE;

  assert(runtime_firebloom_type_canonicalize(&alias) == &bytes);
  assert(runtime_firebloom_type_element_size(&alias) == 1);
  assert(runtime_firebloom_type_element_size_clone(&words) == 4);
  assert(runtime_firebloom_type_is_bytewise_clone(&bytes));
  assert(runtime_firebloom_type_is_zero_sized(&zero));
  assert(!runtime_firebloom_type_is_zero_sized(&words));
}

static void test_span_outliners_and_retype(void) {
  uint8_t bytes[32] = {0};
  RuntimeFirebloomRangeType byte =
      range_type(RUNTIME_FIREBLOOM_RANGE_KIND_BYTEWISE, 0x10, 1, 0);
  RuntimeFirebloomRangeType word =
      range_type(RUNTIME_FIREBLOOM_RANGE_KIND_BYTEWISE, 0x20, 4, 0);
  RuntimeFirebloomRetypeSpan span = {
      .current = bytes,
      .lower_bound = bytes,
      .upper_bound = bytes + sizeof(bytes),
      .type = &byte,
  };

  RuntimeFirebloomRetypeSpan saved =
      runtime_firebloom_outlined_save_span_bounds_x21_x19(span);
  saved = runtime_firebloom_outlined_restore_span_bounds_x21_x19(saved);
  assert(saved.current == bytes && saved.upper_bound == bytes + sizeof(bytes));
  assert(
      runtime_firebloom_outlined_validate_saved_span_x22_x19(saved, 8).type ==
      &byte);
  assert(runtime_firebloom_outlined_validate_source_span_x23_x25(saved, 8)
             .current == bytes);
  assert(runtime_firebloom_tuple_validate(saved).lower_bound == bytes);

  RuntimeFirebloomRetypeCandidate structural =
      runtime_firebloom_try_retype_span(saved, 4, 8, &word, NULL, false);
  assert(structural.kind == RUNTIME_FIREBLOOM_RETYPE_STRUCTURAL);
  assert(structural.span.current == bytes + 4);
  assert(structural.span.upper_bound == bytes + 12);
  assert(structural.span.type == &word);

  RuntimeFirebloomRetypeCandidate exact =
      runtime_firebloom_tuple_retype_slow(saved, &byte, true);
  assert(exact.kind == RUNTIME_FIREBLOOM_RETYPE_EXACT);
  RuntimeFirebloomRetypeCandidate invalid =
      runtime_firebloom_try_retype_span(saved, 30, 4, &word, NULL, false);
  assert(invalid.kind == RUNTIME_FIREBLOOM_RETYPE_INVALID);
  invalid = runtime_firebloom_prepare_invalid_span_candidate();
  assert(invalid.kind == RUNTIME_FIREBLOOM_RETYPE_INVALID);
  structural = runtime_firebloom_outlined_prepare_structural_retype_attempt(
      saved, &word);
  assert(structural.kind == RUNTIME_FIREBLOOM_RETYPE_STRUCTURAL);
  assert(runtime_firebloom_outlined_return_null_pointer() == NULL);
}

static void test_aggregate_and_indirect_resolution(void) {
  RuntimeFirebloomRangeType byte =
      range_type(RUNTIME_FIREBLOOM_RANGE_KIND_BYTEWISE, 1, 1, 0);
  RuntimeFirebloomRangeType word =
      range_type(RUNTIME_FIREBLOOM_RANGE_KIND_BYTEWISE, 2, 4, 0);
  RuntimeFirebloomRangeMember members[] = {
      {.offset = 0, .size = 4, .type = &word},
      {.offset = 8, .size = 1, .type = &byte},
  };
  RuntimeFirebloomRangeType aggregate =
      range_type(RUNTIME_FIREBLOOM_RANGE_KIND_AGGREGATE, 3, 16, 1);
  aggregate.members = members;
  aggregate.member_count = 2;
  RuntimeFirebloomRangeType indirect =
      range_type(RUNTIME_FIREBLOOM_RANGE_KIND_INDIRECT, 4, 16, 1);
  indirect.indirect_header = &aggregate;

  assert(runtime_firebloom_aggregate_type_resolve_byte_offset(
             &aggregate, 0, true) == &aggregate);
  assert(runtime_firebloom_aggregate_type_resolve_byte_offset(&aggregate, 8,
                                                              true) == &byte);
  assert(runtime_firebloom_aggregate_type_resolve_byte_offset(&aggregate, 7,
                                                              false) == NULL);
  assert(runtime_firebloom_require_indirect_type_header(&indirect) ==
         &aggregate);
  assert(runtime_firebloom_indirect_type_resolve_member(&indirect, 8, true) ==
         &byte);
  assert(runtime_firebloom_type_element_size(&indirect) == 16);

  uint8_t storage[16];
  RuntimeFirebloomRetypeSpan source = {
      .current = storage,
      .lower_bound = storage,
      .upper_bound = storage + sizeof(storage),
      .type = &aggregate,
  };
  RuntimeFirebloomRetypeCandidate member =
      runtime_firebloom_try_retype_span(source, 8, 1, &byte, NULL, true);
  assert(member.kind == RUNTIME_FIREBLOOM_RETYPE_STRUCTURAL);
}

static void test_relation_insert_resolve_and_reduce(void) {
  RuntimeFirebloomTypeRelationSet set;
  runtime_firebloom_type_relation_set_initialize(
      &set, relation_allocate, relation_deallocate, relation_unknown, NULL);
  RuntimeFirebloomRangeType left_leaf =
      range_type(RUNTIME_FIREBLOOM_RANGE_KIND_BYTEWISE, 0x11, 4, 0);
  RuntimeFirebloomRangeType right_leaf =
      range_type(RUNTIME_FIREBLOOM_RANGE_KIND_BYTEWISE, 0x22, 4, 0);
  RuntimeFirebloomRangeMember left_members[] = {
      {.offset = 0, .size = 4, .type = &left_leaf},
  };
  RuntimeFirebloomRangeMember right_members[] = {
      {.offset = 0, .size = 4, .type = &right_leaf},
  };
  RuntimeFirebloomRangeType left =
      range_type(RUNTIME_FIREBLOOM_RANGE_KIND_AGGREGATE, 0x33, 4, 0);
  RuntimeFirebloomRangeType right =
      range_type(RUNTIME_FIREBLOOM_RANGE_KIND_AGGREGATE, 0x44, 4, 0);
  left.members = left_members;
  left.member_count = 1;
  right.members = right_members;
  right.member_count = 1;

  assert(runtime_firebloom_type_relation_set_insert(
             &set, &left, &right, RUNTIME_FIREBLOOM_RELATION_UNKNOWN, false) ==
         RUNTIME_FIREBLOOM_RELATION_UNKNOWN);
  assert(runtime_firebloom_type_relation_reduce(&set));
  assert(runtime_firebloom_type_canonicalize(&left) ==
         runtime_firebloom_type_canonicalize(&right));
  assert(runtime_firebloom_type_canonicalize(&left_leaf) ==
         runtime_firebloom_type_canonicalize(&right_leaf));

  RuntimeFirebloomRangeType pointer_left =
      range_type(RUNTIME_FIREBLOOM_RANGE_KIND_POINTER, 0x55, 8, 1);
  RuntimeFirebloomRangeType pointer_right =
      range_type(RUNTIME_FIREBLOOM_RANGE_KIND_POINTER, 0x66, 8, 1);
  assert(runtime_firebloom_type_relation_resolve(&set, &pointer_left,
                                                 &pointer_right, true) ==
         RUNTIME_FIREBLOOM_RELATION_EQUIVALENT);
  assert(runtime_firebloom_type_relation_set_insert(
             &set, &pointer_left, &left, RUNTIME_FIREBLOOM_RELATION_DIRECTED,
             true) == RUNTIME_FIREBLOOM_RELATION_DIRECTED);
  runtime_firebloom_type_relation_set_destroy(&set);
}

int main(void) {
  byte_type = range_type(RUNTIME_FIREBLOOM_RANGE_KIND_BYTEWISE, 0x90, 1, 0);
  aggregate_type =
      range_type(RUNTIME_FIREBLOOM_RANGE_KIND_AGGREGATE, 0x91, 8, 0);
  test_type_geometry_and_canonicalization();
  test_span_outliners_and_retype();
  test_aggregate_and_indirect_resolution();
  test_relation_insert_resolve_and_reduce();
  expect_trap(required_failure);
  expect_trap(invalid_span);
  expect_trap(invalid_indirect);
  return 0;
}
