#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "avp/runtime/firebloom/runtime_firebloom.h"


typedef enum {
    RUNTIME_BITMAP_RANGE_MIXED = -1,
    RUNTIME_BITMAP_RANGE_CLEAR = 0,
    RUNTIME_BITMAP_RANGE_SET = 1,
} RuntimeBitmapRangeClass;

typedef uint64_t (*RuntimeBitmapRandom)(void);

#define RUNTIME_BOUNDED_BITMAP_PUBLIC __attribute__((used, retain))

RUNTIME_BOUNDED_BITMAP_PUBLIC void runtime_bitmap_set_range(
    RuntimeFirebloomSpan bitmap,
    uint32_t bit_capacity,
    uint32_t begin,
    uint32_t end
);
RUNTIME_BOUNDED_BITMAP_PUBLIC void runtime_bitmap_clear_range(
    RuntimeFirebloomSpan bitmap,
    uint32_t bit_capacity,
    uint32_t begin,
    uint32_t end
);
RUNTIME_BOUNDED_BITMAP_PUBLIC RuntimeBitmapRangeClass
runtime_bitmap_classify_range(
    RuntimeFirebloomSpan bitmap,
    uint32_t bit_capacity,
    uint32_t begin,
    uint32_t end
);
RUNTIME_BOUNDED_BITMAP_PUBLIC uint32_t runtime_bitmap_find_clear_index(
    RuntimeFirebloomSpan bitmap,
    uint32_t bit_capacity,
    RuntimeBitmapRandom random
);
RUNTIME_BOUNDED_BITMAP_PUBLIC _Noreturn void
runtime_bitmap_find_clear_index_bounds_panic(void);
RUNTIME_BOUNDED_BITMAP_PUBLIC uint64_t runtime_bitmap_source_token(void);
RUNTIME_BOUNDED_BITMAP_PUBLIC bool
runtime_bitmap_find_clear_index_test_candidate_outlined(
    RuntimeFirebloomSpan bitmap,
    uint32_t candidate
);

RUNTIME_BOUNDED_BITMAP_PUBLIC void runtime_firebloom_store_u64(
    RuntimeFirebloomSpan destination,
    uint64_t value
);
RUNTIME_BOUNDED_BITMAP_PUBLIC _Noreturn void
runtime_firebloom_store_u64_bounds_panic(void);
RUNTIME_BOUNDED_BITMAP_PUBLIC void runtime_firebloom_store_u64_pair(
    RuntimeFirebloomSpan destination,
    uint64_t first,
    uint64_t second
);
RUNTIME_BOUNDED_BITMAP_PUBLIC _Noreturn void
runtime_firebloom_store_u64_pair_bounds_panic(void);
RUNTIME_BOUNDED_BITMAP_PUBLIC void runtime_firebloom_store_u64_triple(
    RuntimeFirebloomSpan destination,
    uint64_t first,
    uint64_t second,
    uint64_t third
);
RUNTIME_BOUNDED_BITMAP_PUBLIC _Noreturn void
runtime_firebloom_store_u64_triple_bounds_panic(void);
