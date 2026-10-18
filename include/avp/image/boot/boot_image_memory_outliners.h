#pragma once

#include "avp/runtime/firebloom/runtime_firebloom.h"

#include <stdbool.h>
#include <stdint.h>

#define BOOT_IMAGE_MEMORY_OUTLINER_PUBLIC __attribute__((used, retain))

BOOT_IMAGE_MEMORY_OUTLINER_PUBLIC RuntimeFirebloomSpan
boot_image_validate_source_allocation_span(RuntimeFirebloomSpan allocation);
BOOT_IMAGE_MEMORY_OUTLINER_PUBLIC RuntimeFirebloomSpan
boot_image_outlined_allocation_span_x19_x22(RuntimeFirebloomSpan allocation);
BOOT_IMAGE_MEMORY_OUTLINER_PUBLIC RuntimeFirebloomSpan
boot_image_outlined_restore_source_record_bounds_x26_x24(
    void *current,
    void *lower_bound,
    void *upper_bound,
    const RuntimeFirebloomType *type
);
BOOT_IMAGE_MEMORY_OUTLINER_PUBLIC bool
boot_image_stack_guard_compare_saved_x8_clone(uint64_t saved_guard);
BOOT_IMAGE_MEMORY_OUTLINER_PUBLIC RuntimeFirebloomSpan
boot_image_outlined_save_source_record_bounds_x26_x24(
    RuntimeFirebloomSpan record
);
BOOT_IMAGE_MEMORY_OUTLINER_PUBLIC uint64_t
boot_image_stack_guard_load_to_x8_clone(void);
BOOT_IMAGE_MEMORY_OUTLINER_PUBLIC uint32_t
boot_image_load_error_code_base_to_w20(void);
BOOT_IMAGE_MEMORY_OUTLINER_PUBLIC RuntimeFirebloomSpan
boot_image_outlined_save_allocation_span_x19_x22(
    RuntimeFirebloomSpan allocation
);
