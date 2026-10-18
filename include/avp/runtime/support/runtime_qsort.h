#pragma once

#include <stddef.h>
#include <stdint.h>

#define RUNTIME_QSORT_PUBLIC __attribute__((used, retain))

typedef int (*RuntimeQSortComparator)(const void *left, const void *right);

RUNTIME_QSORT_PUBLIC void runtime_qsort(
    void *base,
    size_t element_count,
    size_t element_size,
    RuntimeQSortComparator compare
);
RUNTIME_QSORT_PUBLIC void runtime_qsort_swap_elements(
    void *left,
    void *right,
    size_t element_size
);
RUNTIME_QSORT_PUBLIC void *runtime_qsort_median_of_three(
    void *left,
    void *middle,
    void *right,
    RuntimeQSortComparator compare
);
RUNTIME_QSORT_PUBLIC void runtime_qsort_outlined_swap_qword_x23_x24(
    uint64_t *left,
    uint64_t *right
);
RUNTIME_QSORT_PUBLIC void runtime_qsort_outlined_swap_x23_x24(
    void *left,
    void *right,
    size_t element_size
);
RUNTIME_QSORT_PUBLIC int runtime_compare_u64_ascending(
    const void *left,
    const void *right
);
