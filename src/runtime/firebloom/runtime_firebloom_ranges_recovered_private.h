#pragma once

#include "avp/runtime/firebloom/runtime_firebloom_ranges_recovered.h"

#include <stdint.h>

static inline _Noreturn void runtime_firebloom_ranges_trap(void) {
  __builtin_trap();
}

static inline RuntimeFirebloomRangeKind
runtime_firebloom_range_kind(const RuntimeFirebloomRangeType *type) {
  if (type == NULL) {
    runtime_firebloom_ranges_trap();
  }
  return (RuntimeFirebloomRangeKind)(type->relation.parent_and_kind &
                                     RUNTIME_FIREBLOOM_TYPE_KIND_MASK);
}

static inline uintptr_t runtime_firebloom_range_address(const void *pointer) {
  return (uintptr_t)pointer;
}

static inline RuntimeFirebloomRelationType *
runtime_firebloom_range_relation(RuntimeFirebloomRangeType *type) {
  return type == NULL ? NULL : &type->relation;
}

static inline RuntimeFirebloomRangeType *
runtime_firebloom_range_from_relation(RuntimeFirebloomRelationType *type) {
  return (RuntimeFirebloomRangeType *)type;
}
