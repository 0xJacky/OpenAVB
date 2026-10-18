/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Modified by owngoal-dev, 2026: the Bentley-McIlroy partitioning of
 * lib/libc/stdlib/qsort.c rewritten for the freestanding ROM, with the
 * swap and median helpers exposed as separate entry points.
 * Upstream: freebsd/freebsd-src, commit
 * 3aac283613bd3fd0228a06d6c854ca0bf190ecfb.
 */

#include "avp/runtime/support/runtime_qsort.h"

void runtime_qsort_swap_elements(
    void *left,
    void *right,
    size_t element_size
) {
    uint8_t *left_bytes = left;
    uint8_t *right_bytes = right;
    for (size_t index = 0; index < element_size; ++index) {
        uint8_t temporary = left_bytes[index];
        left_bytes[index] = right_bytes[index];
        right_bytes[index] = temporary;
    }
}

void runtime_qsort_outlined_swap_qword_x23_x24(
    uint64_t *left,
    uint64_t *right
) {
    uint64_t temporary = *left;
    *left = *right;
    *right = temporary;
}

void runtime_qsort_outlined_swap_x23_x24(
    void *left,
    void *right,
    size_t element_size
) {
    runtime_qsort_swap_elements(left, right, element_size);
}

void *runtime_qsort_median_of_three(
    void *left,
    void *middle,
    void *right,
    RuntimeQSortComparator compare
) {
    if (compare(left, middle) < 0) {
        if (compare(middle, right) < 0) {
            return middle;
        }
        return compare(left, right) < 0 ? right : left;
    }
    if (compare(middle, right) > 0) {
        return middle;
    }
    return compare(left, right) < 0 ? left : right;
}

static void runtime_qsort_partition(
    uint8_t *base,
    size_t element_count,
    size_t element_size,
    RuntimeQSortComparator compare
) {
    while (element_count >= 2) {
        if (element_count < 7) {
            for (size_t outer = 1; outer < element_count; ++outer) {
                for (size_t inner = outer; inner > 0; --inner) {
                    uint8_t *current = base + inner * element_size;
                    uint8_t *previous = current - element_size;
                    if (compare(previous, current) <= 0) {
                        break;
                    }
                    runtime_qsort_swap_elements(
                        previous,
                        current,
                        element_size
                    );
                }
            }
            return;
        }

        uint8_t *middle = base + (element_count / 2) * element_size;
        uint8_t *last = base + (element_count - 1) * element_size;
        if (element_count > 40) {
            size_t stride = (element_count / 8) * element_size;
            uint8_t *left_median = runtime_qsort_median_of_three(
                base,
                base + stride,
                base + 2 * stride,
                compare
            );
            uint8_t *middle_median = runtime_qsort_median_of_three(
                middle - stride,
                middle,
                middle + stride,
                compare
            );
            uint8_t *right_median = runtime_qsort_median_of_three(
                last - 2 * stride,
                last - stride,
                last,
                compare
            );
            middle = runtime_qsort_median_of_three(
                left_median,
                middle_median,
                right_median,
                compare
            );
        } else {
            middle = runtime_qsort_median_of_three(
                base,
                middle,
                last,
                compare
            );
        }

        runtime_qsort_swap_elements(base, middle, element_size);
        uint8_t *equal_left = base + element_size;
        uint8_t *scan_left = equal_left;
        uint8_t *scan_right = last;
        uint8_t *equal_right = last;

        for (;;) {
            while (scan_left <= scan_right) {
                int relation = compare(scan_left, base);
                if (relation > 0) {
                    break;
                }
                if (relation == 0) {
                    runtime_qsort_swap_elements(
                        equal_left,
                        scan_left,
                        element_size
                    );
                    equal_left += element_size;
                }
                scan_left += element_size;
            }
            while (scan_left <= scan_right) {
                int relation = compare(scan_right, base);
                if (relation < 0) {
                    break;
                }
                if (relation == 0) {
                    runtime_qsort_swap_elements(
                        scan_right,
                        equal_right,
                        element_size
                    );
                    equal_right -= element_size;
                }
                scan_right -= element_size;
            }
            if (scan_left > scan_right) {
                break;
            }
            runtime_qsort_swap_elements(
                scan_left,
                scan_right,
                element_size
            );
            scan_left += element_size;
            scan_right -= element_size;
        }

        uint8_t *end = base + element_count * element_size;
        size_t left_equal_bytes = (size_t)(equal_left - base);
        size_t left_partition_bytes = (size_t)(scan_left - equal_left);
        size_t left_swap_bytes = left_equal_bytes < left_partition_bytes ?
            left_equal_bytes : left_partition_bytes;
        for (size_t offset = 0; offset < left_swap_bytes;
             offset += element_size) {
            runtime_qsort_swap_elements(
                base + offset,
                scan_left - left_swap_bytes + offset,
                element_size
            );
        }

        size_t right_equal_bytes = (size_t)(end - equal_right - element_size);
        size_t right_partition_bytes = (size_t)(equal_right - scan_right);
        size_t right_swap_bytes = right_equal_bytes < right_partition_bytes ?
            right_equal_bytes : right_partition_bytes;
        for (size_t offset = 0; offset < right_swap_bytes;
             offset += element_size) {
            runtime_qsort_swap_elements(
                scan_left + offset,
                end - right_swap_bytes + offset,
                element_size
            );
        }

        size_t left_bytes = (size_t)(scan_left - equal_left);
        size_t right_bytes = (size_t)(equal_right - scan_right);
        size_t left_count = left_bytes / element_size;
        size_t right_count = right_bytes / element_size;
        if (left_count < right_count) {
            runtime_qsort_partition(
                base,
                left_count,
                element_size,
                compare
            );
            base = end - right_bytes;
            element_count = right_count;
        } else {
            runtime_qsort_partition(
                end - right_bytes,
                right_count,
                element_size,
                compare
            );
            element_count = left_count;
        }
    }
}

void runtime_qsort(
    void *base,
    size_t element_count,
    size_t element_size,
    RuntimeQSortComparator compare
) {
    runtime_qsort_partition(base, element_count, element_size, compare);
}

int runtime_compare_u64_ascending(const void *left, const void *right) {
    uint64_t left_value = *(const uint64_t *)left;
    uint64_t right_value = *(const uint64_t *)right;
    return (left_value > right_value) - (left_value < right_value);
}
