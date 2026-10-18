#include "avp/runtime/firebloom/runtime_firebloom_relations_recovered.h"

#include <assert.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct {
  size_t allocations;
  size_t deallocations;
  size_t evaluations;
  bool last_directed;
} RelationProbe;

static RelationProbe probe;

static void *relation_allocate(size_t size, void *context) {
  RelationProbe *active_probe = context;
  ++active_probe->allocations;
  return calloc(1, size);
}

static void relation_deallocate(void *pointer, void *context) {
  RelationProbe *active_probe = context;
  ++active_probe->deallocations;
  free(pointer);
}

static RuntimeFirebloomRelationResult
relation_evaluate(const RuntimeFirebloomRelationType *left,
                  const RuntimeFirebloomRelationType *right, bool directed,
                  void *context) {
  RelationProbe *active_probe = context;
  ++active_probe->evaluations;
  active_probe->last_directed = directed;
  if ((left->identity == UINT64_C(0x11) && right->identity == UINT64_C(0x22)) ||
      (left->identity == UINT64_C(0x22) && right->identity == UINT64_C(0x11))) {
    return directed ? RUNTIME_FIREBLOOM_RELATION_DIRECTED
                    : RUNTIME_FIREBLOOM_RELATION_UNKNOWN;
  }
  return RUNTIME_FIREBLOOM_RELATION_UNKNOWN;
}

static RuntimeFirebloomRelationType type(uint64_t identity, uint8_t kind) {
  return (RuntimeFirebloomRelationType){
      .parent_and_kind = kind,
      .identity = identity,
  };
}

static void initialize_set(RuntimeFirebloomTypeRelationSet *set) {
  memset(&probe, 0, sizeof(probe));
  runtime_firebloom_type_relation_set_initialize(
      set, relation_allocate, relation_deallocate, relation_evaluate, &probe);
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

static void invalid_state(void) {
  RuntimeFirebloomTypeRelationSet set = {0};
  (void)runtime_firebloom_type_relation_state_require(&set);
}

static void invalid_slots(void) {
  RuntimeFirebloomTypeRelationSet set;
  initialize_set(&set);
  uint32_t slots[3];
  runtime_firebloom_type_relation_slots_set(&set, slots, 3);
}

static void invalid_link_detach(void) {
  RuntimeFirebloomTypeRelationLink link = {0};
  runtime_firebloom_type_relation_link_detach(&link);
}

static void test_hash_contract(void) {
  assert(runtime_firebloom_hash_u64(0) == UINT32_C(0));
  assert(runtime_firebloom_hash_u64(UINT64_C(0x0123456789abcdef)) ==
         UINT32_C(0x80100000));
  assert(runtime_firebloom_type_relation_pair_hash(
             UINT64_C(0x1000), UINT64_C(0x2000)) == UINT32_C(0x001e11ca));
  assert(runtime_firebloom_type_relation_pair_hash(UINT64_C(0x1000),
                                                   UINT64_C(0x2001)) ==
         (runtime_firebloom_type_relation_pair_hash(UINT64_C(0x1000),
                                                    UINT64_C(0x2000)) ^
          UINT32_C(1)));
}

static void test_canonicalization_and_merge(void) {
  RuntimeFirebloomRelationType first = type(1, 2);
  RuntimeFirebloomRelationType second = type(2, 6);
  RuntimeFirebloomRelationType third = type(3, 1);
  assert(runtime_firebloom_compare_type_kind_to_indirect(&second));
  assert(!runtime_firebloom_compare_type_kind_to_indirect(&first));

  RuntimeFirebloomRelationType *root =
      runtime_firebloom_type_relation_merge_classes(&first, &second);
  assert(root == &first);
  assert(first.rank == 1);
  assert(runtime_firebloom_type_follow_identity_chain(&second) == &first);
  assert((second.parent_and_kind & RUNTIME_FIREBLOOM_TYPE_KIND_MASK) == 6);

  third.rank = 3;
  root = runtime_firebloom_type_relation_merge_classes(&first, &third);
  assert(root == &third);
  assert(runtime_firebloom_type_canonicalize_clone(&second) == &third);
  assert(runtime_firebloom_type_relation_canonicalize_operand(&first) ==
         &third);
  assert((second.parent_and_kind & ~RUNTIME_FIREBLOOM_TYPE_KIND_MASK) ==
         (uintptr_t)&third);

  RuntimeFirebloomTypePair pair =
      runtime_firebloom_save_type_pair_x20_x19(&first, &second);
  assert(pair.left == &first && pair.right == &second);
  runtime_firebloom_type_relation_index_set(&first, 9);
  assert(first.relation_index == 9);
  runtime_firebloom_type_relation_index_store(&first, 14);
  assert(first.relation_index == 14);
}

static void test_relation_cache_and_growth(void) {
  RuntimeFirebloomTypeRelationSet set;
  initialize_set(&set);
  assert(runtime_firebloom_type_relation_state_require(&set) == &set);
  assert(set.capacity == 4 && set.slot_count == 8);
  assert(probe.allocations == 2);

  RuntimeFirebloomRelationType left = type(0x11, 0);
  RuntimeFirebloomRelationType right = type(0x22, 0);
  assert(runtime_firebloom_type_relation_evaluate_undirected(
             &set, &left, &right) == RUNTIME_FIREBLOOM_RELATION_UNKNOWN);
  assert(probe.evaluations == 1 && !probe.last_directed);
  assert(runtime_firebloom_type_relation_evaluate_undirected(
             &set, &right, &left) == RUNTIME_FIREBLOOM_RELATION_UNKNOWN);
  assert(probe.evaluations == 1);
  assert(runtime_firebloom_type_relation_resolve_and_cache(
             &set, &left, &right) == RUNTIME_FIREBLOOM_RELATION_DIRECTED);
  assert(probe.evaluations == 2 && probe.last_directed);
  assert(runtime_firebloom_type_relation_resolve_and_cache(
             &set, &left, &right) == RUNTIME_FIREBLOOM_RELATION_DIRECTED);
  assert(probe.evaluations == 2);

  RuntimeFirebloomRelationType same_identity = type(0x11, 4);
  assert(runtime_firebloom_type_relation_evaluate(&set, &left, &same_identity,
                                                  false) ==
         RUNTIME_FIREBLOOM_RELATION_EQUIVALENT);

  RuntimeFirebloomRelationType extra[8];
  for (size_t index = 0; index < 8; ++index) {
    extra[index] = type(0x100 + index, 0);
    (void)runtime_firebloom_type_relation_evaluate(&set, &left, &extra[index],
                                                   true);
  }
  assert(set.capacity >= 8);
  assert(set.slot_count >= set.capacity * 2);
  RuntimeFirebloomRelationSlots loaded =
      runtime_firebloom_load_type_relation_slots_to_x8(&set);
  assert(loaded.slots == set.slots);
  assert(loaded.slot_count == set.slot_count);
  assert(loaded.slot_mask == set.slot_mask);

  RuntimeFirebloomTypeRelationEntry *occupied = NULL;
  for (uint32_t index = 0; index < set.count; ++index) {
    if (set.entries[index].occupied) {
      occupied = &set.entries[index];
      break;
    }
  }
  assert(occupied != NULL);
  runtime_firebloom_type_relation_metadata_store(occupied,
                                                 UINT64_C(0x123456789abcdef0));
  assert(occupied->metadata == UINT64_C(0x123456789abcdef0));

  const size_t allocations = probe.allocations;
  runtime_firebloom_type_relation_set_destroy(&set);
  assert(set.state == 0);
  assert(probe.deallocations == allocations);
}

static void test_slots_layout(void) {
  uint32_t slots[16];
  RuntimeFirebloomRelationSlots layout =
      runtime_firebloom_prepare_type_relation_metadata_layout(8, slots);
  assert(layout.slots == slots);
  assert(layout.slot_count == 16 && layout.slot_mask == 15);
  for (size_t index = 0; index < 16; ++index) {
    assert(slots[index] == UINT32_MAX);
  }

  RuntimeFirebloomTypeRelationSet set;
  initialize_set(&set);
  uint32_t *old_slots = set.slots;
  set.deallocate(old_slots, set.context);
  runtime_firebloom_type_relation_slots_store(&set, layout);
  assert(set.slots == slots);
  set.slots = NULL;
  runtime_firebloom_type_relation_set_destroy(&set);
}

static void test_link_lifecycle(void) {
  RuntimeFirebloomRelationType owner = type(1, 0);
  RuntimeFirebloomRelationType first_target = type(2, 0);
  RuntimeFirebloomRelationType second_target = type(3, 0);
  RuntimeFirebloomTypeRelationLink first = {0};
  RuntimeFirebloomTypeRelationLink second = {0};
  runtime_firebloom_type_relation_link_attach(&owner, &first, &first_target);
  runtime_firebloom_type_relation_link_attach(&owner, &second, &second_target);
  assert(owner.links == &second);
  assert(second.next == &first && first.previous == &second);
  assert(runtime_firebloom_type_relation_link_target(&first) == &first_target);
  runtime_firebloom_type_relation_link_detach(&second);
  assert(owner.links == &first && first.previous == NULL);
  runtime_firebloom_type_relation_link_detach(&first);
  assert(owner.links == NULL);
}

int main(void) {
  test_hash_contract();
  test_canonicalization_and_merge();
  test_relation_cache_and_growth();
  test_slots_layout();
  test_link_lifecycle();
  expect_trap(invalid_state);
  expect_trap(invalid_slots);
  expect_trap(invalid_link_detach);
  return 0;
}
