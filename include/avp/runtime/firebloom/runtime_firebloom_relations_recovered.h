#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if __has_attribute(retain)
#define RUNTIME_FIREBLOOM_RELATIONS_PUBLIC __attribute__((used, retain))
#else
#define RUNTIME_FIREBLOOM_RELATIONS_PUBLIC __attribute__((used))
#endif

#define RUNTIME_FIREBLOOM_TYPE_KIND_MASK ((uintptr_t)7)
#define RUNTIME_FIREBLOOM_TYPE_KIND_INDIRECT ((uint8_t)6)

typedef struct RuntimeFirebloomRelationType RuntimeFirebloomRelationType;
typedef struct RuntimeFirebloomTypeRelationLink
    RuntimeFirebloomTypeRelationLink;

struct RuntimeFirebloomRelationType {
  uintptr_t parent_and_kind;
  uint64_t identity;
  uint32_t rank;
  uint32_t relation_index;
  RuntimeFirebloomTypeRelationLink *links;
};

struct RuntimeFirebloomTypeRelationLink {
  RuntimeFirebloomTypeRelationLink *previous;
  RuntimeFirebloomTypeRelationLink *next;
  RuntimeFirebloomRelationType *target;
  RuntimeFirebloomRelationType *owner;
};

typedef enum {
  RUNTIME_FIREBLOOM_RELATION_UNKNOWN = 0,
  RUNTIME_FIREBLOOM_RELATION_EQUIVALENT = 1,
  RUNTIME_FIREBLOOM_RELATION_DIRECTED = 2,
} RuntimeFirebloomRelationResult;

typedef struct {
  RuntimeFirebloomRelationType *left;
  RuntimeFirebloomRelationType *right;
} RuntimeFirebloomTypePair;

typedef struct {
  RuntimeFirebloomRelationType *left;
  RuntimeFirebloomRelationType *right;
  RuntimeFirebloomRelationResult result;
  uint64_t metadata;
  bool directed;
  bool occupied;
} RuntimeFirebloomTypeRelationEntry;

typedef void *(*RuntimeFirebloomRelationAllocate)(size_t size, void *context);
typedef void (*RuntimeFirebloomRelationDeallocate)(
  void *pointer,
  void *context
);
typedef RuntimeFirebloomRelationResult (*RuntimeFirebloomRelationEvaluate)(
  const RuntimeFirebloomRelationType *left,
  const RuntimeFirebloomRelationType *right,
  bool directed,
  void *context
);

typedef struct {
  RuntimeFirebloomTypeRelationEntry *entries;
  uint32_t count;
  uint32_t capacity;
  uint32_t *slots;
  uint32_t slot_count;
  uint32_t slot_mask;
  uint8_t state;
  uint8_t reserved_021[7];
  RuntimeFirebloomRelationAllocate allocate;
  RuntimeFirebloomRelationDeallocate deallocate;
  RuntimeFirebloomRelationEvaluate evaluate;
  void *context;
} RuntimeFirebloomTypeRelationSet;

typedef struct {
  uint32_t *slots;
  uint32_t slot_count;
  uint32_t slot_mask;
} RuntimeFirebloomRelationSlots;

RUNTIME_FIREBLOOM_RELATIONS_PUBLIC RuntimeFirebloomRelationType *
runtime_firebloom_type_follow_identity_chain(
    RuntimeFirebloomRelationType *type);
RUNTIME_FIREBLOOM_RELATIONS_PUBLIC RuntimeFirebloomRelationResult
runtime_firebloom_type_relation_evaluate_undirected(
  RuntimeFirebloomTypeRelationSet *set,
  RuntimeFirebloomRelationType *left,
  RuntimeFirebloomRelationType *right
);
RUNTIME_FIREBLOOM_RELATIONS_PUBLIC RuntimeFirebloomRelationResult
runtime_firebloom_type_relation_evaluate(
  RuntimeFirebloomTypeRelationSet *set,
  RuntimeFirebloomRelationType *left,
  RuntimeFirebloomRelationType *right,
  bool directed
);
RUNTIME_FIREBLOOM_RELATIONS_PUBLIC RuntimeFirebloomRelationResult
runtime_firebloom_type_relation_resolve_and_cache(
  RuntimeFirebloomTypeRelationSet *set,
  RuntimeFirebloomRelationType *left,
  RuntimeFirebloomRelationType *right
);
RUNTIME_FIREBLOOM_RELATIONS_PUBLIC RuntimeFirebloomRelationType *
runtime_firebloom_type_canonicalize_clone(RuntimeFirebloomRelationType *type);
RUNTIME_FIREBLOOM_RELATIONS_PUBLIC bool
runtime_firebloom_compare_type_kind_to_indirect(
    const RuntimeFirebloomRelationType *type);
RUNTIME_FIREBLOOM_RELATIONS_PUBLIC RuntimeFirebloomTypePair
runtime_firebloom_save_type_pair_x20_x19(
  RuntimeFirebloomRelationType *left,
  RuntimeFirebloomRelationType *right
);
RUNTIME_FIREBLOOM_RELATIONS_PUBLIC RuntimeFirebloomRelationType *
runtime_firebloom_type_relation_link_target(
    const RuntimeFirebloomTypeRelationLink *link);
RUNTIME_FIREBLOOM_RELATIONS_PUBLIC void
runtime_firebloom_type_relation_link_detach(
    RuntimeFirebloomTypeRelationLink *link);
RUNTIME_FIREBLOOM_RELATIONS_PUBLIC void
runtime_firebloom_type_relation_index_set(
  RuntimeFirebloomRelationType *type,
  uint32_t index
);
RUNTIME_FIREBLOOM_RELATIONS_PUBLIC void
runtime_firebloom_type_relation_index_store(
  RuntimeFirebloomRelationType *type,
  uint32_t index
);
RUNTIME_FIREBLOOM_RELATIONS_PUBLIC void
runtime_firebloom_type_relation_slots_set(
  RuntimeFirebloomTypeRelationSet *set,
  uint32_t *slots,
  uint32_t slot_count
);
RUNTIME_FIREBLOOM_RELATIONS_PUBLIC void
runtime_firebloom_type_relation_slots_store(
  RuntimeFirebloomTypeRelationSet *set,
  RuntimeFirebloomRelationSlots slots
);
RUNTIME_FIREBLOOM_RELATIONS_PUBLIC RuntimeFirebloomRelationType *
runtime_firebloom_type_relation_merge_classes(
  RuntimeFirebloomRelationType *left,
  RuntimeFirebloomRelationType *right
);
RUNTIME_FIREBLOOM_RELATIONS_PUBLIC RuntimeFirebloomTypeRelationSet *
runtime_firebloom_type_relation_state_require(
    RuntimeFirebloomTypeRelationSet *set);
RUNTIME_FIREBLOOM_RELATIONS_PUBLIC _Noreturn void
runtime_firebloom_type_relation_state_access_panic(void);
RUNTIME_FIREBLOOM_RELATIONS_PUBLIC void
runtime_firebloom_type_relation_link_attach(
  RuntimeFirebloomRelationType *owner,
  RuntimeFirebloomTypeRelationLink *link,
  RuntimeFirebloomRelationType *target
);
RUNTIME_FIREBLOOM_RELATIONS_PUBLIC void
runtime_firebloom_type_relation_metadata_store(
  RuntimeFirebloomTypeRelationEntry *entry,
  uint64_t metadata
);
RUNTIME_FIREBLOOM_RELATIONS_PUBLIC RuntimeFirebloomRelationSlots
runtime_firebloom_load_type_relation_slots_to_x8(
    RuntimeFirebloomTypeRelationSet *set);
RUNTIME_FIREBLOOM_RELATIONS_PUBLIC RuntimeFirebloomRelationSlots
runtime_firebloom_prepare_type_relation_metadata_layout(
  uint32_t capacity,
  uint32_t *slots
);
RUNTIME_FIREBLOOM_RELATIONS_PUBLIC void
runtime_firebloom_type_relation_grow_capacity(
  RuntimeFirebloomTypeRelationSet *set,
  uint32_t minimum_capacity
);
RUNTIME_FIREBLOOM_RELATIONS_PUBLIC void
runtime_firebloom_type_relation_set_initialize(
  RuntimeFirebloomTypeRelationSet *set,
  RuntimeFirebloomRelationAllocate allocate,
  RuntimeFirebloomRelationDeallocate deallocate,
  RuntimeFirebloomRelationEvaluate evaluate,
  void *context
);
RUNTIME_FIREBLOOM_RELATIONS_PUBLIC void
runtime_firebloom_type_relation_set_destroy(
    RuntimeFirebloomTypeRelationSet *set);
RUNTIME_FIREBLOOM_RELATIONS_PUBLIC RuntimeFirebloomRelationType *
runtime_firebloom_type_relation_canonicalize_operand(
    RuntimeFirebloomRelationType *type);
RUNTIME_FIREBLOOM_RELATIONS_PUBLIC uint32_t
runtime_firebloom_type_relation_pair_hash(uintptr_t left, uintptr_t right);
RUNTIME_FIREBLOOM_RELATIONS_PUBLIC uint32_t
runtime_firebloom_hash_u64(uint64_t value);
