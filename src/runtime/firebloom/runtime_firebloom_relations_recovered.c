#include "avp/runtime/firebloom/runtime_firebloom_relations_recovered.h"

#include <limits.h>

#define RUNTIME_FIREBLOOM_RELATION_EMPTY_SLOT UINT32_MAX

static _Noreturn void runtime_firebloom_relations_trap(void) {
  __builtin_trap();
  __builtin_unreachable();
}

static RuntimeFirebloomRelationType *
runtime_firebloom_parent(RuntimeFirebloomRelationType *type) {
  if (type == NULL) {
    runtime_firebloom_relations_trap();
  }
  const uintptr_t parent =
      type->parent_and_kind & ~RUNTIME_FIREBLOOM_TYPE_KIND_MASK;
  return parent == 0 ? type : (RuntimeFirebloomRelationType *)parent;
}

RuntimeFirebloomRelationType *RUNTIME_FIREBLOOM_RELATIONS_PUBLIC
runtime_firebloom_type_follow_identity_chain(
    RuntimeFirebloomRelationType *type) {
  RuntimeFirebloomRelationType *root = type;
  while (runtime_firebloom_parent(root) != root) {
    root = runtime_firebloom_parent(root);
  }
  while (type != root) {
    RuntimeFirebloomRelationType *parent = runtime_firebloom_parent(type);
    type->parent_and_kind =
        (uintptr_t)root |
        (type->parent_and_kind & RUNTIME_FIREBLOOM_TYPE_KIND_MASK);
    type = parent;
  }
  return root;
}

RuntimeFirebloomRelationType *RUNTIME_FIREBLOOM_RELATIONS_PUBLIC
runtime_firebloom_type_canonicalize_clone(RuntimeFirebloomRelationType *type) {
  return runtime_firebloom_type_follow_identity_chain(type);
}

RuntimeFirebloomRelationType *RUNTIME_FIREBLOOM_RELATIONS_PUBLIC
runtime_firebloom_type_relation_canonicalize_operand(
    RuntimeFirebloomRelationType *type) {
  return runtime_firebloom_type_follow_identity_chain(type);
}

bool RUNTIME_FIREBLOOM_RELATIONS_PUBLIC
runtime_firebloom_compare_type_kind_to_indirect(
    const RuntimeFirebloomRelationType *type) {
  if (type == NULL) {
    runtime_firebloom_relations_trap();
  }
  return (type->parent_and_kind & RUNTIME_FIREBLOOM_TYPE_KIND_MASK) ==
         RUNTIME_FIREBLOOM_TYPE_KIND_INDIRECT;
}

RuntimeFirebloomTypePair RUNTIME_FIREBLOOM_RELATIONS_PUBLIC
runtime_firebloom_save_type_pair_x20_x19(
    RuntimeFirebloomRelationType *left,
    RuntimeFirebloomRelationType *right
) {
  return (RuntimeFirebloomTypePair){.left = left, .right = right};
}

uint32_t RUNTIME_FIREBLOOM_RELATIONS_PUBLIC
runtime_firebloom_hash_u64(uint64_t value) {
  const uint32_t low = (uint32_t)value;
  const uint32_t low_fold = low ^ (low >> 4);
  const uint32_t seed = UINT32_C(0xdeadbeef);
  const uint32_t low_mix = (low_fold ^ seed) + (low_fold << 5);
  const uint32_t high = (uint32_t)(value >> 32);
  const uint32_t high_fold = high ^ (uint32_t)(value >> 36);
  const uint32_t high_mix = (high_fold ^ seed) + (high_fold << 5);
  const uint32_t combined = high_mix ^ low_mix;
  return low_mix ^ (combined >> 11) ^ high_mix;
}

uint32_t RUNTIME_FIREBLOOM_RELATIONS_PUBLIC
runtime_firebloom_type_relation_pair_hash(uintptr_t left, uintptr_t right) {
  const uint32_t left_hash = runtime_firebloom_hash_u64(left);
  const uintptr_t tagged = right & 1;
  const uint32_t right_hash =
      runtime_firebloom_hash_u64((right & ~(uintptr_t)1) + 8);
  return left_hash ^ (right_hash + (uint32_t)tagged);
}

RuntimeFirebloomTypeRelationSet *RUNTIME_FIREBLOOM_RELATIONS_PUBLIC
runtime_firebloom_type_relation_state_require(
    RuntimeFirebloomTypeRelationSet *set) {
  if (set == NULL || set->state != 1 || set->allocate == NULL ||
      set->deallocate == NULL) {
    runtime_firebloom_type_relation_state_access_panic();
  }
  return set;
}

_Noreturn void RUNTIME_FIREBLOOM_RELATIONS_PUBLIC
runtime_firebloom_type_relation_state_access_panic(void) {
  runtime_firebloom_relations_trap();
}

static bool runtime_firebloom_is_power_of_two(uint32_t value) {
  return value != 0 && (value & (value - 1)) == 0;
}

void RUNTIME_FIREBLOOM_RELATIONS_PUBLIC
runtime_firebloom_type_relation_slots_set(
    RuntimeFirebloomTypeRelationSet *set,
    uint32_t *slots,
    uint32_t slot_count
) {
  set = runtime_firebloom_type_relation_state_require(set);
  if (slots == NULL || !runtime_firebloom_is_power_of_two(slot_count)) {
    runtime_firebloom_relations_trap();
  }
  set->slots = slots;
  set->slot_count = slot_count;
  set->slot_mask = slot_count - 1;
}

void RUNTIME_FIREBLOOM_RELATIONS_PUBLIC
runtime_firebloom_type_relation_slots_store(
    RuntimeFirebloomTypeRelationSet *set,
    RuntimeFirebloomRelationSlots slots
) {
  if (slots.slot_mask != slots.slot_count - 1) {
    runtime_firebloom_relations_trap();
  }
  runtime_firebloom_type_relation_slots_set(set, slots.slots, slots.slot_count);
}

RuntimeFirebloomRelationSlots RUNTIME_FIREBLOOM_RELATIONS_PUBLIC
runtime_firebloom_load_type_relation_slots_to_x8(
    RuntimeFirebloomTypeRelationSet *set) {
  set = runtime_firebloom_type_relation_state_require(set);
  return (RuntimeFirebloomRelationSlots){
      .slots = set->slots,
      .slot_count = set->slot_count,
      .slot_mask = set->slot_mask,
  };
}

RuntimeFirebloomRelationSlots RUNTIME_FIREBLOOM_RELATIONS_PUBLIC
runtime_firebloom_prepare_type_relation_metadata_layout(
    uint32_t capacity,
    uint32_t *slots
) {
  if (capacity == 0 || slots == NULL) {
    runtime_firebloom_relations_trap();
  }
  if (capacity > UINT32_MAX / 2) {
    runtime_firebloom_relations_trap();
  }
  uint32_t slot_count = 2;
  while (slot_count < capacity * 2) {
    if (slot_count > UINT32_MAX / 2) {
      runtime_firebloom_relations_trap();
    }
    slot_count *= 2;
  }
  for (uint32_t index = 0; index < slot_count; ++index) {
    slots[index] = RUNTIME_FIREBLOOM_RELATION_EMPTY_SLOT;
  }
  return (RuntimeFirebloomRelationSlots){
      .slots = slots,
      .slot_count = slot_count,
      .slot_mask = slot_count - 1,
  };
}

static uint32_t runtime_firebloom_relation_slot(
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
  while (set->slots[slot] != RUNTIME_FIREBLOOM_RELATION_EMPTY_SLOT) {
    RuntimeFirebloomTypeRelationEntry *entry = &set->entries[set->slots[slot]];
    if (entry->occupied && entry->left == left && entry->right == right &&
        entry->directed == directed) {
      break;
    }
    slot = (slot + 1) & set->slot_mask;
  }
  return slot;
}

void RUNTIME_FIREBLOOM_RELATIONS_PUBLIC
runtime_firebloom_type_relation_grow_capacity(
    RuntimeFirebloomTypeRelationSet *set,
    uint32_t minimum_capacity
) {
  set = runtime_firebloom_type_relation_state_require(set);
  uint32_t capacity = set->capacity == 0 ? 4 : set->capacity;
  while (capacity < minimum_capacity) {
    if (capacity > UINT32_MAX / 2) {
      runtime_firebloom_relations_trap();
    }
    capacity *= 2;
  }
  if (capacity == set->capacity) {
    return;
  }
  if (capacity > UINT32_MAX / 2) {
    runtime_firebloom_relations_trap();
  }

  RuntimeFirebloomTypeRelationEntry *entries =
      set->allocate((size_t)capacity * sizeof(*entries), set->context);
  uint32_t slot_count = 2;
  while (slot_count < capacity * 2) {
    slot_count *= 2;
  }
  uint32_t *slots =
      set->allocate((size_t)slot_count * sizeof(*slots), set->context);
  if (entries == NULL || slots == NULL) {
    runtime_firebloom_relations_trap();
  }
  for (uint32_t index = 0; index < capacity; ++index) {
    entries[index] = (RuntimeFirebloomTypeRelationEntry){0};
  }
  for (uint32_t index = 0; index < slot_count; ++index) {
    slots[index] = RUNTIME_FIREBLOOM_RELATION_EMPTY_SLOT;
  }

  RuntimeFirebloomTypeRelationEntry *old_entries = set->entries;
  uint32_t *old_slots = set->slots;
  const uint32_t old_count = set->count;
  set->entries = entries;
  set->capacity = capacity;
  set->slots = slots;
  set->slot_count = slot_count;
  set->slot_mask = slot_count - 1;
  for (uint32_t index = 0; index < old_count; ++index) {
    entries[index] = old_entries[index];
    const uint32_t slot = runtime_firebloom_relation_slot(
        set,
        entries[index].left,
        entries[index].right,
        entries[index].directed
    );
    slots[slot] = index;
  }
  if (old_entries != NULL) {
    set->deallocate(old_entries, set->context);
  }
  if (old_slots != NULL) {
    set->deallocate(old_slots, set->context);
  }
}

void RUNTIME_FIREBLOOM_RELATIONS_PUBLIC
runtime_firebloom_type_relation_set_initialize(
    RuntimeFirebloomTypeRelationSet *set,
    RuntimeFirebloomRelationAllocate allocate,
    RuntimeFirebloomRelationDeallocate deallocate,
    RuntimeFirebloomRelationEvaluate evaluate,
    void *context
) {
  if (set == NULL || allocate == NULL || deallocate == NULL) {
    runtime_firebloom_relations_trap();
  }
  *set = (RuntimeFirebloomTypeRelationSet){
      .state = 1,
      .allocate = allocate,
      .deallocate = deallocate,
      .evaluate = evaluate,
      .context = context,
  };
  runtime_firebloom_type_relation_grow_capacity(set, 4);
}

void RUNTIME_FIREBLOOM_RELATIONS_PUBLIC
runtime_firebloom_type_relation_set_destroy(
    RuntimeFirebloomTypeRelationSet *set) {
  set = runtime_firebloom_type_relation_state_require(set);
  set->deallocate(set->entries, set->context);
  set->deallocate(set->slots, set->context);
  *set = (RuntimeFirebloomTypeRelationSet){0};
}

static RuntimeFirebloomTypeRelationEntry *runtime_firebloom_relation_find(
    RuntimeFirebloomTypeRelationSet *set,
    RuntimeFirebloomRelationType *left,
    RuntimeFirebloomRelationType *right,
    bool directed
) {
  const uint32_t slot =
      runtime_firebloom_relation_slot(set, left, right, directed);
  const uint32_t index = set->slots[slot];
  return index == RUNTIME_FIREBLOOM_RELATION_EMPTY_SLOT ? NULL
                                                        : &set->entries[index];
}

static RuntimeFirebloomTypeRelationEntry *runtime_firebloom_relation_insert(
    RuntimeFirebloomTypeRelationSet *set,
    RuntimeFirebloomRelationType *left,
    RuntimeFirebloomRelationType *right,
    RuntimeFirebloomRelationResult result,
    bool directed
) {
  if (set->count == set->capacity) {
    runtime_firebloom_type_relation_grow_capacity(set, set->capacity + 1);
  }
  const uint32_t slot =
      runtime_firebloom_relation_slot(set, left, right, directed);
  RuntimeFirebloomTypeRelationEntry *entry = &set->entries[set->count];
  *entry = (RuntimeFirebloomTypeRelationEntry){
      .left = left,
      .right = right,
      .result = result,
      .directed = directed,
      .occupied = true,
  };
  set->slots[slot] = set->count++;
  return entry;
}

RuntimeFirebloomRelationResult RUNTIME_FIREBLOOM_RELATIONS_PUBLIC
runtime_firebloom_type_relation_evaluate(
    RuntimeFirebloomTypeRelationSet *set,
    RuntimeFirebloomRelationType *left,
    RuntimeFirebloomRelationType *right,
    bool directed
) {
  set = runtime_firebloom_type_relation_state_require(set);
  left = runtime_firebloom_type_relation_canonicalize_operand(left);
  right = runtime_firebloom_type_relation_canonicalize_operand(right);
  if (left == right ||
      (left->identity != 0 && left->identity == right->identity)) {
    return RUNTIME_FIREBLOOM_RELATION_EQUIVALENT;
  }
  if (!directed && (uintptr_t)left > (uintptr_t)right) {
    RuntimeFirebloomRelationType *swap = left;
    left = right;
    right = swap;
  }
  RuntimeFirebloomTypeRelationEntry *entry =
      runtime_firebloom_relation_find(set, left, right, directed);
  if (entry != NULL) {
    return entry->result;
  }
  RuntimeFirebloomRelationResult result = RUNTIME_FIREBLOOM_RELATION_UNKNOWN;
  if (set->evaluate != NULL) {
    result = set->evaluate(left, right, directed, set->context);
  }
  if (result > RUNTIME_FIREBLOOM_RELATION_DIRECTED) {
    runtime_firebloom_relations_trap();
  }
  (void)runtime_firebloom_relation_insert(set, left, right, result, directed);
  return result;
}

RuntimeFirebloomRelationResult RUNTIME_FIREBLOOM_RELATIONS_PUBLIC
runtime_firebloom_type_relation_evaluate_undirected(
    RuntimeFirebloomTypeRelationSet *set,
    RuntimeFirebloomRelationType *left,
    RuntimeFirebloomRelationType *right
) {
  return runtime_firebloom_type_relation_evaluate(set, left, right, false);
}

RuntimeFirebloomRelationResult RUNTIME_FIREBLOOM_RELATIONS_PUBLIC
runtime_firebloom_type_relation_resolve_and_cache(
    RuntimeFirebloomTypeRelationSet *set,
    RuntimeFirebloomRelationType *left,
    RuntimeFirebloomRelationType *right
) {
  left = runtime_firebloom_type_canonicalize_clone(left);
  right = runtime_firebloom_type_canonicalize_clone(right);
  RuntimeFirebloomRelationResult result =
      runtime_firebloom_type_relation_evaluate_undirected(set, left, right);
  if (result == RUNTIME_FIREBLOOM_RELATION_UNKNOWN) {
    result = runtime_firebloom_type_relation_evaluate(set, left, right, true);
  }
  if (result == RUNTIME_FIREBLOOM_RELATION_EQUIVALENT) {
    (void)runtime_firebloom_type_relation_merge_classes(left, right);
  }
  return result;
}

void RUNTIME_FIREBLOOM_RELATIONS_PUBLIC
runtime_firebloom_type_relation_index_set(
    RuntimeFirebloomRelationType *type,
    uint32_t index
) {
  if (type == NULL) {
    runtime_firebloom_relations_trap();
  }
  type->relation_index = index;
}

void RUNTIME_FIREBLOOM_RELATIONS_PUBLIC
runtime_firebloom_type_relation_index_store(
    RuntimeFirebloomRelationType *type,
    uint32_t index
) {
  runtime_firebloom_type_relation_index_set(type, index);
}

RuntimeFirebloomRelationType *RUNTIME_FIREBLOOM_RELATIONS_PUBLIC
runtime_firebloom_type_relation_merge_classes(
    RuntimeFirebloomRelationType *left,
    RuntimeFirebloomRelationType *right
) {
  left = runtime_firebloom_type_follow_identity_chain(left);
  right = runtime_firebloom_type_follow_identity_chain(right);
  if (left == right) {
    return left;
  }
  if (left->rank < right->rank) {
    RuntimeFirebloomRelationType *swap = left;
    left = right;
    right = swap;
  }
  right->parent_and_kind = (uintptr_t)left | (right->parent_and_kind &
                                              RUNTIME_FIREBLOOM_TYPE_KIND_MASK);
  if (left->rank == right->rank) {
    if (left->rank == UINT32_MAX) {
      runtime_firebloom_relations_trap();
    }
    ++left->rank;
  }
  return left;
}

RuntimeFirebloomRelationType *RUNTIME_FIREBLOOM_RELATIONS_PUBLIC
runtime_firebloom_type_relation_link_target(
    const RuntimeFirebloomTypeRelationLink *link) {
  if (link == NULL || link->owner == NULL || link->target == NULL) {
    runtime_firebloom_relations_trap();
  }
  return runtime_firebloom_type_follow_identity_chain(link->target);
}

void RUNTIME_FIREBLOOM_RELATIONS_PUBLIC
runtime_firebloom_type_relation_link_attach(
    RuntimeFirebloomRelationType *owner,
    RuntimeFirebloomTypeRelationLink *link,
    RuntimeFirebloomRelationType *target
) {
  if (owner == NULL || link == NULL || target == NULL || link->owner != NULL) {
    runtime_firebloom_relations_trap();
  }
  link->owner = owner;
  link->target = target;
  link->previous = NULL;
  link->next = owner->links;
  if (owner->links != NULL) {
    owner->links->previous = link;
  }
  owner->links = link;
}

void RUNTIME_FIREBLOOM_RELATIONS_PUBLIC
runtime_firebloom_type_relation_link_detach(
    RuntimeFirebloomTypeRelationLink *link) {
  if (link == NULL || link->owner == NULL) {
    runtime_firebloom_relations_trap();
  }
  if (link->previous != NULL) {
    link->previous->next = link->next;
  } else {
    link->owner->links = link->next;
  }
  if (link->next != NULL) {
    link->next->previous = link->previous;
  }
  *link = (RuntimeFirebloomTypeRelationLink){0};
}

void RUNTIME_FIREBLOOM_RELATIONS_PUBLIC
runtime_firebloom_type_relation_metadata_store(
    RuntimeFirebloomTypeRelationEntry *entry,
    uint64_t metadata
) {
  if (entry == NULL || !entry->occupied) {
    runtime_firebloom_relations_trap();
  }
  entry->metadata = metadata;
}
