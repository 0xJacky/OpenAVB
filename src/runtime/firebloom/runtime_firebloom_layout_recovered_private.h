#pragma once

#include "avp/runtime/firebloom/runtime_firebloom_layout_recovered.h"

typedef struct RuntimeFirebloomLayoutSuffix {
  struct RuntimeFirebloomLayoutSuffix *next;
  size_t offset;
  RuntimeFirebloomLayoutType type;
} RuntimeFirebloomLayoutSuffix;

struct RuntimeFirebloomLayoutCache {
  RuntimeFirebloomLayoutType *owner;
  RuntimeFirebloomLayoutSuffix *suffixes;
};

extern RuntimeFirebloomLayoutRuntime runtime_firebloom_layout_runtime;
extern bool runtime_firebloom_layout_mutation_active;

_Noreturn void runtime_firebloom_layout_contract_fail(void);

static inline uint32_t runtime_firebloom_layout_entry_offset(
    const RuntimeFirebloomLayoutEntry *entry) {
  return (uint32_t)entry->encoded;
}

static inline uint32_t runtime_firebloom_layout_entry_count(
    const RuntimeFirebloomLayoutEntry *entry) {
  return (uint32_t)((entry->encoded >> 32U) & UINT64_C(0x0fffffff));
}

static inline uint8_t runtime_firebloom_layout_entry_kind(
    const RuntimeFirebloomLayoutEntry *entry) {
  return (uint8_t)((entry->encoded >> 60U) & UINT64_C(7));
}

static inline bool runtime_firebloom_layout_entry_is_structural(
    const RuntimeFirebloomLayoutEntry *entry) {
  return (entry->encoded >> 63U) != 0U;
}

