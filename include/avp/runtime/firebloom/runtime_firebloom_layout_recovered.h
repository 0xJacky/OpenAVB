#pragma once

#include "avp/runtime/firebloom/runtime_firebloom_ranges_recovered.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if __has_attribute(retain)
#define RUNTIME_FIREBLOOM_LAYOUT_PUBLIC __attribute__((used, retain))
#else
#define RUNTIME_FIREBLOOM_LAYOUT_PUBLIC __attribute__((used))
#endif

typedef struct {
  uint64_t encoded;
  RuntimeFirebloomRangeType *type;
} RuntimeFirebloomLayoutEntry;

typedef struct RuntimeFirebloomLayoutCache RuntimeFirebloomLayoutCache;

typedef struct {
  RuntimeFirebloomRangeType relation_type;
  uint32_t byte_size;
  uint32_t flags;
  RuntimeFirebloomLayoutEntry *entries;
  size_t entry_count;
  RuntimeFirebloomLayoutCache *cache;
} RuntimeFirebloomLayoutType;

typedef struct {
  RuntimeFirebloomTypeRelationSet *relations;
  bool structural;
  bool succeeded;
  const void *saved_state;
  const void *failure_source;
} RuntimeFirebloomLayoutReduction;

typedef void *(*RuntimeFirebloomLayoutAllocate)(size_t size, void *context);
typedef void (*RuntimeFirebloomLayoutYield)(void *context);

typedef struct {
  RuntimeFirebloomLayoutAllocate allocate;
  RuntimeFirebloomLayoutYield yield;
  void *context;
} RuntimeFirebloomLayoutRuntime;

void runtime_firebloom_layout_bind_runtime(
    RuntimeFirebloomLayoutRuntime runtime);
void runtime_firebloom_type_cache_mutation_enter(void);
bool runtime_firebloom_type_cache_mutation_is_active(void);

RUNTIME_FIREBLOOM_LAYOUT_PUBLIC bool
runtime_firebloom_type_relation_reduce_layout_pair(
  RuntimeFirebloomTypeRelationSet *relations,
  RuntimeFirebloomLayoutReduction *reduction,
  RuntimeFirebloomLayoutType *left,
  RuntimeFirebloomLayoutType *right
);
RUNTIME_FIREBLOOM_LAYOUT_PUBLIC void
runtime_firebloom_type_layout_validate_ranges(
    const RuntimeFirebloomLayoutType *type);
RUNTIME_FIREBLOOM_LAYOUT_PUBLIC bool
runtime_firebloom_type_relation_validate_layout_entry_pair(
    RuntimeFirebloomLayoutReduction *reduction,
    const RuntimeFirebloomLayoutEntry *left,
    const RuntimeFirebloomLayoutEntry *right);
RUNTIME_FIREBLOOM_LAYOUT_PUBLIC size_t
runtime_firebloom_layout_entry_end_offset(
    const RuntimeFirebloomLayoutEntry *entry);
RUNTIME_FIREBLOOM_LAYOUT_PUBLIC _Noreturn void
runtime_firebloom_layout_entry_end_offset_invalid_kind(void);
RUNTIME_FIREBLOOM_LAYOUT_PUBLIC void
runtime_firebloom_type_relation_mark_failure_from_saved_state(
    RuntimeFirebloomLayoutReduction *reduction);
RUNTIME_FIREBLOOM_LAYOUT_PUBLIC bool
runtime_firebloom_type_relation_validate_saved_layout_entry_pair(
    RuntimeFirebloomLayoutReduction *reduction,
    const RuntimeFirebloomLayoutEntry *left,
    const RuntimeFirebloomLayoutEntry *right);
RUNTIME_FIREBLOOM_LAYOUT_PUBLIC RuntimeFirebloomLayoutCache *
runtime_firebloom_type_cache_create(RuntimeFirebloomLayoutType *type);
RUNTIME_FIREBLOOM_LAYOUT_PUBLIC RuntimeFirebloomLayoutType *
runtime_firebloom_type_suffix_create_cached(
  RuntimeFirebloomLayoutType *type,
  size_t offset
);
RUNTIME_FIREBLOOM_LAYOUT_PUBLIC size_t
runtime_firebloom_layout_kind_stride(uint8_t kind);
RUNTIME_FIREBLOOM_LAYOUT_PUBLIC _Noreturn void
runtime_firebloom_layout_kind_stride_invalid_kind(void);
RUNTIME_FIREBLOOM_LAYOUT_PUBLIC void
runtime_firebloom_type_cache_mutation_leave(void);

