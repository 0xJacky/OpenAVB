#include "avp/runtime/firebloom/runtime_firebloom_layout_recovered.h"

#include <assert.h>
#include <setjmp.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

static uint8_t arena[4096];
static size_t arena_used;
static unsigned yields;

static void *arena_allocate(size_t size, void *context) {
  (void)context;
  size = (size + 7U) & ~(size_t)7U;
  assert(arena_used + size <= sizeof(arena));
  void *result = &arena[arena_used];
  arena_used += size;
  return result;
}

static void arena_deallocate(void *pointer, void *context) {
  (void)pointer;
  (void)context;
}

static void record_yield(void *context) {
  (void)context;
  ++yields;
}

static uint64_t encode(uint32_t offset, uint32_t count, uint8_t kind,
                       bool structural) {
  return offset | ((uint64_t)count << 32U) | ((uint64_t)kind << 60U) |
         ((uint64_t)structural << 63U);
}

static RuntimeFirebloomRelationResult evaluate(
    const RuntimeFirebloomRelationType *left,
    const RuntimeFirebloomRelationType *right, bool directed, void *context) {
  (void)left;
  (void)right;
  (void)context;
  return directed ? RUNTIME_FIREBLOOM_RELATION_DIRECTED
                  : RUNTIME_FIREBLOOM_RELATION_EQUIVALENT;
}

static void expect_trap(void (*operation)(void)) {
  pid_t child = fork();
  assert(child >= 0);
  if (child == 0) {
    operation();
    _Exit(0);
  }
  int status = 0;
  assert(waitpid(child, &status, 0) == child);
  assert(WIFSIGNALED(status));
}

static void invalid_kind(void) {
  (void)runtime_firebloom_layout_kind_stride(5U);
}

static void invalid_range(void) {
  RuntimeFirebloomLayoutEntry entry = {encode(24U, 1U, 0U, false), NULL};
  RuntimeFirebloomLayoutType type = {.byte_size = 32U,
                                     .entries = &entry,
                                     .entry_count = 1U};
  runtime_firebloom_type_layout_validate_ranges(&type);
}

int main(void) {
  runtime_firebloom_layout_bind_runtime(
      (RuntimeFirebloomLayoutRuntime){arena_allocate, record_yield, NULL});
  assert(runtime_firebloom_layout_kind_stride(0U) == 32U);
  assert(runtime_firebloom_layout_kind_stride(1U) == 16U);
  assert(runtime_firebloom_layout_kind_stride(2U) == 24U);
  assert(runtime_firebloom_layout_kind_stride(3U) == 8U);
  assert(runtime_firebloom_layout_kind_stride(4U) == 8U);
  expect_trap(invalid_kind);
  expect_trap(invalid_range);

  RuntimeFirebloomRangeType leaf_a = {0};
  RuntimeFirebloomRangeType leaf_b = {0};
  RuntimeFirebloomLayoutEntry left_entries[] = {
      {encode(0U, 1U, 0U, false), &leaf_a},
      {encode(32U, 2U, 1U, true), &leaf_a},
  };
  RuntimeFirebloomLayoutEntry right_entries[] = {
      {encode(0U, 1U, 0U, false), &leaf_b},
      {encode(32U, 2U, 1U, true), &leaf_b},
  };
  RuntimeFirebloomLayoutType left = {.byte_size = 64U,
                                     .entries = left_entries,
                                     .entry_count = 2U};
  RuntimeFirebloomLayoutType right = {.byte_size = 64U,
                                      .entries = right_entries,
                                      .entry_count = 2U};
  RuntimeFirebloomTypeRelationSet relations;
  runtime_firebloom_type_relation_set_initialize(
      &relations, arena_allocate, arena_deallocate, evaluate, NULL);
  RuntimeFirebloomLayoutReduction reduction = {
      .structural = true, .saved_state = &left};
  assert(runtime_firebloom_type_relation_reduce_layout_pair(
      &relations, &reduction, &left, &right));
  assert(reduction.succeeded);
  assert(relations.count == 2U);

  right_entries[1].encoded = encode(40U, 2U, 1U, true);
  assert(!runtime_firebloom_type_relation_validate_layout_entry_pair(
      &reduction, &left_entries[1], &right_entries[1]));
  assert(reduction.failure_source == &left);
  right_entries[1].encoded = left_entries[1].encoded;
  right_entries[1].encoded = encode(32U, 2U, 1U, false);
  reduction.succeeded = true;
  assert(!runtime_firebloom_type_relation_validate_layout_entry_pair(
      &reduction, &left_entries[1], &right_entries[1]));
  right_entries[1].encoded = left_entries[1].encoded;

  runtime_firebloom_type_cache_mutation_enter();
  RuntimeFirebloomLayoutType *suffix =
      runtime_firebloom_type_suffix_create_cached(&left, 32U);
  assert(suffix->byte_size == 32U);
  assert(suffix->entry_count == 1U);
  assert((uint32_t)suffix->entries[0].encoded == 0U);
  assert(runtime_firebloom_type_suffix_create_cached(&left, 32U) == suffix);
  runtime_firebloom_type_cache_mutation_leave();
  assert(!runtime_firebloom_type_cache_mutation_is_active());
  assert(yields == 1U);

  return 0;
}
