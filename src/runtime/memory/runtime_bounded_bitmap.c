#include "avp/runtime/memory/runtime_bounded_bitmap.h"

#include <stddef.h>


/*
 * Related public bitmap implementations:
 * FreeBSD 3aac283 bitstring.h (BSD-3-Clause):
 * https://github.com/freebsd/freebsd-src/commit/3aac283613bd3fd0228a06d6c854ca0bf190ecfb
 * Zephyr 0d65ce4 bitarray.c (Apache-2.0):
 * https://github.com/zephyrproject-rtos/zephyr/commit/0d65ce46dda0626164503c50f37e6e1f59d310a9
 * The range-mask decomposition follows those public algorithms. AVP's
 * Firebloom bounds and randomized-first clear search remain
 * AVPBooter-compatible ABI.
 */

enum {
    RUNTIME_BITMAP_WORD_BITS = 64,
    RUNTIME_BITMAP_WORD_BYTES = 8,
    RUNTIME_BITMAP_RANDOM_ATTEMPT_LIMIT = 128,
};

typedef struct {
    uint32_t first_word;
    uint32_t last_word;
    uint64_t first_mask;
    uint64_t last_mask;
} RuntimeBitmapRange;

static _Noreturn void runtime_bounded_bitmap_fail(void) {
    __builtin_trap();
}

static uint8_t *runtime_bounded_storage_require(
    RuntimeFirebloomSpan storage,
    size_t size
) {
    const uintptr_t current = (uintptr_t)storage.current;
    const uintptr_t lower = (uintptr_t)storage.lower_bound;
    const uintptr_t upper = (uintptr_t)storage.upper_bound;
    if (storage.current == 0 || current < lower || current >= upper ||
        size > upper - current) {
        runtime_bounded_bitmap_fail();
    }
    return storage.current;
}

static size_t runtime_bitmap_storage_size(uint32_t bit_capacity) {
    if (bit_capacity == 0) {
        runtime_bounded_bitmap_fail();
    }
    return ((size_t)bit_capacity + RUNTIME_BITMAP_WORD_BITS - 1) /
        RUNTIME_BITMAP_WORD_BITS * RUNTIME_BITMAP_WORD_BYTES;
}

static uint64_t runtime_bitmap_load_word(
    const uint8_t *storage,
    uint32_t word_index
) {
    const uint8_t *word = storage +
        (size_t)word_index * RUNTIME_BITMAP_WORD_BYTES;
    uint64_t value = 0;
    for (uint32_t byte = 0; byte < RUNTIME_BITMAP_WORD_BYTES; ++byte) {
        value |= (uint64_t)word[byte] << (byte * 8);
    }
    return value;
}

static void runtime_bitmap_store_word(
    uint8_t *storage,
    uint32_t word_index,
    uint64_t value
) {
    uint8_t *word = storage +
        (size_t)word_index * RUNTIME_BITMAP_WORD_BYTES;
    for (uint32_t byte = 0; byte < RUNTIME_BITMAP_WORD_BYTES; ++byte) {
        word[byte] = (uint8_t)(value >> (byte * 8));
    }
}

static uint64_t runtime_bitmap_low_mask(uint32_t bit_count) {
    return bit_count == RUNTIME_BITMAP_WORD_BITS
        ? UINT64_MAX
        : (UINT64_C(1) << bit_count) - 1;
}

static RuntimeBitmapRange runtime_bitmap_require_range(
    RuntimeFirebloomSpan bitmap,
    uint32_t bit_capacity,
    uint32_t begin,
    uint32_t end,
    uint8_t **storage
) {
    if (begin >= end || end > bit_capacity || storage == 0) {
        runtime_bounded_bitmap_fail();
    }
    *storage = runtime_bounded_storage_require(
        bitmap,
        runtime_bitmap_storage_size(bit_capacity)
    );
    const uint32_t first_offset = begin % RUNTIME_BITMAP_WORD_BITS;
    const uint32_t last_offset = (end - 1) % RUNTIME_BITMAP_WORD_BITS;
    RuntimeBitmapRange range = {
        .first_word = begin / RUNTIME_BITMAP_WORD_BITS,
        .last_word = (end - 1) / RUNTIME_BITMAP_WORD_BITS,
        .first_mask = UINT64_MAX << first_offset,
        .last_mask = runtime_bitmap_low_mask(last_offset + 1),
    };
    if (range.first_word == range.last_word) {
        range.first_mask &= range.last_mask;
        range.last_mask = range.first_mask;
    }
    return range;
}

static void runtime_bitmap_update_range(
    RuntimeFirebloomSpan bitmap,
    uint32_t bit_capacity,
    uint32_t begin,
    uint32_t end,
    bool set
) {
    uint8_t *storage = 0;
    const RuntimeBitmapRange range = runtime_bitmap_require_range(
        bitmap,
        bit_capacity,
        begin,
        end,
        &storage
    );
    for (uint32_t word = range.first_word;
         word <= range.last_word;
         ++word) {
        uint64_t mask = UINT64_MAX;
        if (word == range.first_word) {
            mask &= range.first_mask;
        }
        if (word == range.last_word) {
            mask &= range.last_mask;
        }
        uint64_t value = runtime_bitmap_load_word(storage, word);
        value = set ? value | mask : value & ~mask;
        runtime_bitmap_store_word(storage, word, value);
    }
}

void runtime_bitmap_set_range(
    RuntimeFirebloomSpan bitmap,
    uint32_t bit_capacity,
    uint32_t begin,
    uint32_t end
) {
    runtime_bitmap_update_range(bitmap, bit_capacity, begin, end, true);
}

void runtime_bitmap_clear_range(
    RuntimeFirebloomSpan bitmap,
    uint32_t bit_capacity,
    uint32_t begin,
    uint32_t end
) {
    runtime_bitmap_update_range(bitmap, bit_capacity, begin, end, false);
}

RuntimeBitmapRangeClass runtime_bitmap_classify_range(
    RuntimeFirebloomSpan bitmap,
    uint32_t bit_capacity,
    uint32_t begin,
    uint32_t end
) {
    uint8_t *storage = 0;
    const RuntimeBitmapRange range = runtime_bitmap_require_range(
        bitmap,
        bit_capacity,
        begin,
        end,
        &storage
    );
    bool any_set = false;
    bool all_set = true;
    for (uint32_t word = range.first_word;
         word <= range.last_word;
         ++word) {
        uint64_t mask = UINT64_MAX;
        if (word == range.first_word) {
            mask &= range.first_mask;
        }
        if (word == range.last_word) {
            mask &= range.last_mask;
        }
        const uint64_t selected =
            runtime_bitmap_load_word(storage, word) & mask;
        any_set |= selected != 0;
        all_set &= selected == mask;
    }
    if (all_set) {
        return RUNTIME_BITMAP_RANGE_SET;
    }
    return any_set ? RUNTIME_BITMAP_RANGE_MIXED :
        RUNTIME_BITMAP_RANGE_CLEAR;
}

bool runtime_bitmap_find_clear_index_test_candidate_outlined(
    RuntimeFirebloomSpan bitmap,
    uint32_t candidate
) {
    const size_t word_offset =
        (size_t)(candidate / RUNTIME_BITMAP_WORD_BITS) *
        RUNTIME_BITMAP_WORD_BYTES;
    uint8_t *storage = runtime_bounded_storage_require(
        bitmap,
        word_offset + RUNTIME_BITMAP_WORD_BYTES
    );
    const uint64_t word = runtime_bitmap_load_word(
        storage,
        candidate / RUNTIME_BITMAP_WORD_BITS
    );
    return ((word >> (candidate % RUNTIME_BITMAP_WORD_BITS)) &
        UINT64_C(1)) != 0;
}

uint32_t runtime_bitmap_find_clear_index(
    RuntimeFirebloomSpan bitmap,
    uint32_t bit_capacity,
    RuntimeBitmapRandom random
) {
    if (random == 0) {
        runtime_bounded_bitmap_fail();
    }
    (void)runtime_bounded_storage_require(
        bitmap,
        runtime_bitmap_storage_size(bit_capacity)
    );
    const uint32_t random_attempts =
        bit_capacity < RUNTIME_BITMAP_RANDOM_ATTEMPT_LIMIT
        ? bit_capacity
        : RUNTIME_BITMAP_RANDOM_ATTEMPT_LIMIT;
    for (uint32_t attempt = 0; attempt < random_attempts; ++attempt) {
        const uint32_t candidate = (uint32_t)(random() % bit_capacity);
        if (!runtime_bitmap_find_clear_index_test_candidate_outlined(
                bitmap,
                candidate
            )) {
            return candidate;
        }
    }
    for (uint32_t candidate = 0;
         candidate < bit_capacity;
         ++candidate) {
        if (!runtime_bitmap_find_clear_index_test_candidate_outlined(
                bitmap,
                candidate
            )) {
            return candidate;
        }
    }
    return UINT32_MAX;
}

_Noreturn void runtime_bitmap_find_clear_index_bounds_panic(void) {
    runtime_bounded_bitmap_fail();
}

uint64_t runtime_bitmap_source_token(void) {
    return UINT64_C(0x019e6e2aee6952b9);
}

static void runtime_firebloom_store_words(
    RuntimeFirebloomSpan destination,
    const uint64_t *values,
    size_t count
) {
    uint8_t *storage = runtime_bounded_storage_require(
        destination,
        count * sizeof(*values)
    );
    for (size_t index = 0; index < count; ++index) {
        runtime_bitmap_store_word(storage, (uint32_t)index, values[index]);
    }
}

void runtime_firebloom_store_u64(
    RuntimeFirebloomSpan destination,
    uint64_t value
) {
    runtime_firebloom_store_words(destination, &value, 1);
}

_Noreturn void runtime_firebloom_store_u64_bounds_panic(void) {
    runtime_bounded_bitmap_fail();
}

void runtime_firebloom_store_u64_pair(
    RuntimeFirebloomSpan destination,
    uint64_t first,
    uint64_t second
) {
    const uint64_t values[] = {first, second};
    runtime_firebloom_store_words(destination, values, 2);
}

_Noreturn void runtime_firebloom_store_u64_pair_bounds_panic(void) {
    runtime_bounded_bitmap_fail();
}

void runtime_firebloom_store_u64_triple(
    RuntimeFirebloomSpan destination,
    uint64_t first,
    uint64_t second,
    uint64_t third
) {
    const uint64_t values[] = {first, second, third};
    runtime_firebloom_store_words(destination, values, 3);
}

_Noreturn void runtime_firebloom_store_u64_triple_bounds_panic(void) {
    runtime_bounded_bitmap_fail();
}
