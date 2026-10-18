#include "avp/runtime/support/runtime_qsort.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint64_t key;
    uint8_t payload[5];
} OddRecord;

static int compare_odd_record(const void *left, const void *right) {
    const OddRecord *left_record = left;
    const OddRecord *right_record = right;
    return runtime_compare_u64_ascending(
        &left_record->key,
        &right_record->key
    );
}

static void test_helpers(void) {
    uint64_t left = 9;
    uint64_t middle = 2;
    uint64_t right = 7;
    assert(runtime_qsort_median_of_three(
        &left,
        &middle,
        &right,
        runtime_compare_u64_ascending
    ) == &right);
    runtime_qsort_outlined_swap_qword_x23_x24(&left, &right);
    assert(left == 7 && right == 9);

    uint8_t first[] = {1, 2, 3};
    uint8_t second[] = {4, 5, 6};
    runtime_qsort_outlined_swap_x23_x24(first, second, sizeof(first));
    assert(memcmp(first, (uint8_t[]){4, 5, 6}, sizeof(first)) == 0);
    assert(memcmp(second, (uint8_t[]){1, 2, 3}, sizeof(second)) == 0);
}

static void test_sort_geometries(void) {
    uint64_t values[] = {5, 1, 9, 1, 0, UINT64_MAX, 5, 3};
    runtime_qsort(
        values,
        sizeof(values) / sizeof(values[0]),
        sizeof(values[0]),
        runtime_compare_u64_ascending
    );
    const uint64_t expected[] = {0, 1, 1, 3, 5, 5, 9, UINT64_MAX};
    assert(memcmp(values, expected, sizeof(values)) == 0);

    OddRecord records[65];
    for (size_t index = 0; index < 65; ++index) {
        records[index].key = (index * 37) % 11;
        memset(records[index].payload, (int)index, sizeof(records[index].payload));
    }
    runtime_qsort(records, 65, sizeof(records[0]), compare_odd_record);
    for (size_t index = 1; index < 65; ++index) {
        assert(records[index - 1].key <= records[index].key);
    }

    runtime_qsort(records, 0, sizeof(records[0]), compare_odd_record);
    runtime_qsort(records, 1, sizeof(records[0]), compare_odd_record);
}

int main(void) {
    test_helpers();
    test_sort_geometries();
    puts("runtime_qsort_tests: ok");
}
