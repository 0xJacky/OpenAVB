#include "runtime_firebloom_ranges_recovered_private.h"

#include <limits.h>

static uint32_t runtime_firebloom_range_relation_slot(
    RuntimeFirebloomTypeRelationSet *set,
    RuntimeFirebloomRelationType *left,
    RuntimeFirebloomRelationType *right,
    bool directed
) {
  uint32_t slot = runtime_firebloom_type_relation_pair_hash(
      (uintptr_t)left,
      (uintptr_t)right | (uintptr_t)directed
  ) &
                  set->slot_mask;
  while (set->slots[slot] != UINT32_MAX) {
    RuntimeFirebloomTypeRelationEntry *entry = &set->entries[set->slots[slot]];
    if (entry->occupied && entry->left == left && entry->right == right &&
        entry->directed == directed) {
      break;
    }
    slot = (slot + 1) & set->slot_mask;
  }
  return slot;
}

RuntimeFirebloomRelationResult RUNTIME_FIREBLOOM_RANGES_PUBLIC
runtime_firebloom_type_relation_set_insert(
    RuntimeFirebloomTypeRelationSet *set,
    RuntimeFirebloomRangeType *left,
    RuntimeFirebloomRangeType *right,
    RuntimeFirebloomRelationResult result,
    bool directed
) {
  set = runtime_firebloom_type_relation_state_require(set);
  left = runtime_firebloom_type_canonicalize(left);
  right = runtime_firebloom_type_canonicalize(right);
  if (result > RUNTIME_FIREBLOOM_RELATION_DIRECTED) {
    runtime_firebloom_ranges_trap();
  }
  if (!directed && (uintptr_t)left > (uintptr_t)right) {
    RuntimeFirebloomRangeType *swap = left;
    left = right;
    right = swap;
  }

  uint32_t slot = runtime_firebloom_range_relation_slot(
      set,
      &left->relation,
      &right->relation,
      directed
  );
  if (set->slots[slot] != UINT32_MAX) {
    RuntimeFirebloomTypeRelationEntry *entry = &set->entries[set->slots[slot]];
    if (entry->result == RUNTIME_FIREBLOOM_RELATION_UNKNOWN) {
      entry->result = result;
    } else if (result != RUNTIME_FIREBLOOM_RELATION_UNKNOWN &&
               entry->result != result) {
      runtime_firebloom_ranges_trap();
    }
    return entry->result;
  }

  if (set->count == set->capacity) {
    runtime_firebloom_type_relation_grow_capacity(set, set->capacity + 1);
    slot = runtime_firebloom_range_relation_slot(
        set,
        &left->relation,
        &right->relation,
        directed
    );
  }
  const uint32_t index = set->count++;
  set->entries[index] = (RuntimeFirebloomTypeRelationEntry){
      .left = &left->relation,
      .right = &right->relation,
      .result = result,
      .directed = directed,
      .occupied = true,
  };
  set->slots[slot] = index;
  return result;
}

static bool
runtime_firebloom_range_types_match(
    const RuntimeFirebloomRangeType *left,
    const RuntimeFirebloomRangeType *right
) {
  return runtime_firebloom_range_kind(left) ==
             runtime_firebloom_range_kind(right) &&
         left->element_size == right->element_size &&
         left->pointer_count == right->pointer_count &&
         left->member_count == right->member_count &&
         left->alignment == right->alignment;
}

bool RUNTIME_FIREBLOOM_RANGES_PUBLIC
runtime_firebloom_type_relation_reduce(RuntimeFirebloomTypeRelationSet *set) {
  set = runtime_firebloom_type_relation_state_require(set);
  bool compatible = true;
  for (uint32_t index = 0; index < set->count; ++index) {
    RuntimeFirebloomTypeRelationEntry snapshot = set->entries[index];
    if (!snapshot.occupied || snapshot.directed ||
        snapshot.result == RUNTIME_FIREBLOOM_RELATION_DIRECTED) {
      continue;
    }
    RuntimeFirebloomRangeType *left = runtime_firebloom_type_canonicalize(
        runtime_firebloom_range_from_relation(snapshot.left));
    RuntimeFirebloomRangeType *right = runtime_firebloom_type_canonicalize(
        runtime_firebloom_range_from_relation(snapshot.right));
    if (left == right) {
      set->entries[index].result = RUNTIME_FIREBLOOM_RELATION_EQUIVALENT;
      continue;
    }
    if (!runtime_firebloom_range_types_match(left, right)) {
      set->entries[index].result = RUNTIME_FIREBLOOM_RELATION_UNKNOWN;
      compatible = false;
      continue;
    }

    bool members_match = true;
    for (uint32_t member = 0; member < left->member_count; ++member) {
      const RuntimeFirebloomRangeMember *left_member = &left->members[member];
      const RuntimeFirebloomRangeMember *right_member = &right->members[member];
      if (left_member->offset != right_member->offset ||
          left_member->size != right_member->size ||
          left_member->type == NULL || right_member->type == NULL) {
        members_match = false;
        break;
      }
      (void)runtime_firebloom_type_relation_set_insert(
          set,
          left_member->type,
          right_member->type,
          RUNTIME_FIREBLOOM_RELATION_EQUIVALENT,
          false
      );
    }
    if (!members_match) {
      set->entries[index].result = RUNTIME_FIREBLOOM_RELATION_UNKNOWN;
      compatible = false;
      continue;
    }
    set->entries[index].result = RUNTIME_FIREBLOOM_RELATION_EQUIVALENT;
    (void)runtime_firebloom_type_relation_merge_classes(
        &left->relation,
        &right->relation
    );
  }
  return compatible;
}

RuntimeFirebloomRelationResult RUNTIME_FIREBLOOM_RANGES_PUBLIC
runtime_firebloom_type_relation_resolve(
    RuntimeFirebloomTypeRelationSet *set,
    RuntimeFirebloomRangeType *left,
    RuntimeFirebloomRangeType *right,
    bool structural
) {
  left = runtime_firebloom_type_canonicalize(left);
  right = runtime_firebloom_type_canonicalize(right);
  if (left == right || (left->relation.identity != 0 &&
                        left->relation.identity == right->relation.identity)) {
    return RUNTIME_FIREBLOOM_RELATION_EQUIVALENT;
  }

  RuntimeFirebloomRelationResult result =
      runtime_firebloom_type_relation_resolve_and_cache(
          set,
          &left->relation,
          &right->relation
      );
  if (result != RUNTIME_FIREBLOOM_RELATION_UNKNOWN || !structural) {
    return result;
  }
  (void)runtime_firebloom_type_relation_set_insert(
      set,
      left,
      right,
      RUNTIME_FIREBLOOM_RELATION_UNKNOWN,
      false
  );
  (void)runtime_firebloom_type_relation_reduce(set);
  left = runtime_firebloom_type_canonicalize(left);
  right = runtime_firebloom_type_canonicalize(right);
  return left == right ? RUNTIME_FIREBLOOM_RELATION_EQUIVALENT
                       : RUNTIME_FIREBLOOM_RELATION_UNKNOWN;
}
