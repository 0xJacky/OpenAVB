#include "avp/runtime/memory/runtime_bounded_bitmap.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>


static const RuntimeFirebloomType bitmap_type = {
    .identity = UINT64_C(0x6269746d6170),
    .element_size = sizeof(uint64_t),
    .bytewise_copyable = false,
};

static uint8_t bitmap_storage[24];
static uint64_t random_value;
static uint32_t random_calls;

static RuntimeFirebloomSpan bitmap_span(size_t size) {
    return (RuntimeFirebloomSpan){
        .current = bitmap_storage,
        .lower_bound = bitmap_storage,
        .upper_bound = bitmap_storage + size,
        .type = &bitmap_type,
    };
}

static uint64_t mock_random(void) {
    ++random_calls;
    return random_value;
}

static void clear_storage(void) {
    for (size_t index = 0; index < sizeof(bitmap_storage); ++index) {
        bitmap_storage[index] = 0;
    }
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
}

static void invoke_empty_range(void) {
    runtime_bitmap_set_range(bitmap_span(sizeof(bitmap_storage)), 130, 7, 7);
}

static void invoke_range_above_capacity(void) {
    runtime_bitmap_clear_range(
        bitmap_span(sizeof(bitmap_storage)),
        130,
        120,
        131
    );
}

static void invoke_short_bitmap(void) {
    runtime_bitmap_set_range(bitmap_span(8), 130, 0, 1);
}

static void invoke_short_triple(void) {
    runtime_firebloom_store_u64_triple(
        bitmap_span(16),
        UINT64_C(1),
        UINT64_C(2),
        UINT64_C(3)
    );
}

static void test_range_mutation_and_classification(void) {
    clear_storage();
    const RuntimeFirebloomSpan bitmap = bitmap_span(sizeof(bitmap_storage));
    assert(runtime_bitmap_classify_range(bitmap, 130, 3, 70) ==
        RUNTIME_BITMAP_RANGE_CLEAR);

    runtime_bitmap_set_range(bitmap, 130, 3, 70);
    assert(runtime_bitmap_classify_range(bitmap, 130, 3, 70) ==
        RUNTIME_BITMAP_RANGE_SET);
    assert(runtime_bitmap_classify_range(bitmap, 130, 0, 3) ==
        RUNTIME_BITMAP_RANGE_CLEAR);
    assert(runtime_bitmap_classify_range(bitmap, 130, 0, 70) ==
        RUNTIME_BITMAP_RANGE_MIXED);

    runtime_bitmap_clear_range(bitmap, 130, 60, 66);
    assert(runtime_bitmap_classify_range(bitmap, 130, 3, 70) ==
        RUNTIME_BITMAP_RANGE_MIXED);
    assert(runtime_bitmap_classify_range(bitmap, 130, 60, 66) ==
        RUNTIME_BITMAP_RANGE_CLEAR);

    runtime_bitmap_set_range(bitmap, 130, 128, 130);
    assert(runtime_bitmap_classify_range(bitmap, 130, 128, 130) ==
        RUNTIME_BITMAP_RANGE_SET);
    assert(bitmap_storage[16] == UINT8_C(3));
    for (size_t index = 17; index < sizeof(bitmap_storage); ++index) {
        assert(bitmap_storage[index] == 0);
    }
}

static void test_clear_index_selection(void) {
    clear_storage();
    const RuntimeFirebloomSpan bitmap = bitmap_span(sizeof(bitmap_storage));
    runtime_bitmap_set_range(bitmap, 130, 0, 130);
    runtime_bitmap_clear_range(bitmap, 130, 77, 78);
    random_value = 77;
    random_calls = 0;
    assert(runtime_bitmap_find_clear_index(bitmap, 130, mock_random) == 77);
    assert(random_calls == 1);
    assert(!runtime_bitmap_find_clear_index_test_candidate_outlined(
        bitmap,
        77
    ));
    assert(runtime_bitmap_find_clear_index_test_candidate_outlined(
        bitmap,
        76
    ));

    runtime_bitmap_set_range(bitmap, 130, 77, 78);
    runtime_bitmap_clear_range(bitmap, 130, 129, 130);
    random_value = 0;
    random_calls = 0;
    assert(runtime_bitmap_find_clear_index(bitmap, 130, mock_random) == 129);
    assert(random_calls == 128);

    runtime_bitmap_set_range(bitmap, 130, 129, 130);
    random_calls = 0;
    assert(runtime_bitmap_find_clear_index(bitmap, 130, mock_random) ==
        UINT32_MAX);
    assert(random_calls == 128);
}

static uint64_t load_u64(const uint8_t *bytes) {
    uint64_t value = 0;
    for (uint32_t index = 0; index < 8; ++index) {
        value |= (uint64_t)bytes[index] << (index * 8);
    }
    return value;
}

static void test_bounded_u64_stores(void) {
    clear_storage();
    const RuntimeFirebloomSpan storage = bitmap_span(sizeof(bitmap_storage));
    runtime_firebloom_store_u64(storage, UINT64_C(0x0102030405060708));
    assert(load_u64(bitmap_storage) == UINT64_C(0x0102030405060708));

    runtime_firebloom_store_u64_pair(
        storage,
        UINT64_C(0x1112131415161718),
        UINT64_C(0x2122232425262728)
    );
    assert(load_u64(bitmap_storage) == UINT64_C(0x1112131415161718));
    assert(load_u64(bitmap_storage + 8) == UINT64_C(0x2122232425262728));

    runtime_firebloom_store_u64_triple(
        storage,
        UINT64_C(0x3132333435363738),
        UINT64_C(0x4142434445464748),
        UINT64_C(0x5152535455565758)
    );
    assert(load_u64(bitmap_storage) == UINT64_C(0x3132333435363738));
    assert(load_u64(bitmap_storage + 8) == UINT64_C(0x4142434445464748));
    assert(load_u64(bitmap_storage + 16) == UINT64_C(0x5152535455565758));
}

int main(void) {
    test_range_mutation_and_classification();
    test_clear_index_selection();
    test_bounded_u64_stores();
    assert(runtime_bitmap_source_token() == UINT64_C(0x019e6e2aee6952b9));
    expect_trap(invoke_empty_range);
    expect_trap(invoke_range_above_capacity);
    expect_trap(invoke_short_bitmap);
    expect_trap(invoke_short_triple);
    puts("runtime bounded bitmap tests passed");
    return 0;
}
