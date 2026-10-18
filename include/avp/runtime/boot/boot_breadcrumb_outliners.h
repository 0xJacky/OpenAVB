#pragma once

#include "avp/runtime/firebloom/runtime_firebloom.h"

#include <stdbool.h>
#include <stddef.h>

#define BOOT_BREADCRUMB_OUTLINER_PUBLIC __attribute__((used, retain))

typedef struct {
    uint8_t *address;
    bool below_upper;
} BootBreadcrumbOffsetComparison;

BOOT_BREADCRUMB_OUTLINER_PUBLIC BootBreadcrumbOffsetComparison
boot_breadcrumb_outlined_compare_base_plus_offset_to_upper(
    void *base,
    size_t offset,
    void *upper_bound
);
BOOT_BREADCRUMB_OUTLINER_PUBLIC RuntimeFirebloomSpan
boot_breadcrumb_outlined_restore_span_bounds_x21_x20_x19(
    void *current,
    void *lower_bound,
    void *upper_bound,
    const RuntimeFirebloomType *type
);
BOOT_BREADCRUMB_OUTLINER_PUBLIC RuntimeFirebloomSpan
boot_breadcrumb_outlined_save_span_bounds_x21_x20_x19(
    RuntimeFirebloomSpan span
);
BOOT_BREADCRUMB_OUTLINER_PUBLIC const RuntimeFirebloomType *
boot_breadcrumb_outlined_load_byte_type_x3(void);
BOOT_BREADCRUMB_OUTLINER_PUBLIC RuntimeFirebloomSpan
boot_breadcrumb_outlined_restore_span_x23_x26(RuntimeFirebloomSpan span);
BOOT_BREADCRUMB_OUTLINER_PUBLIC RuntimeFirebloomSpan
boot_breadcrumb_outlined_save_span_x19_x22(RuntimeFirebloomSpan span);
