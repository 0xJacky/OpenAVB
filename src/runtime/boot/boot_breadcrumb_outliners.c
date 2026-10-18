#include "avp/runtime/boot/boot_breadcrumb_outliners.h"

static const RuntimeFirebloomType breadcrumb_byte_type = {
    .identity = UINT64_C(0x00144f18),
    .element_size = 1,
    .bytewise_copyable = true,
};

BootBreadcrumbOffsetComparison
boot_breadcrumb_outlined_compare_base_plus_offset_to_upper(
    void *base,
    size_t offset,
    void *upper_bound
) {
    uint8_t *address = (uint8_t *)base + offset;
    return (BootBreadcrumbOffsetComparison){
        .address = address,
        .below_upper = address < (uint8_t *)upper_bound,
    };
}

RuntimeFirebloomSpan
boot_breadcrumb_outlined_restore_span_bounds_x21_x20_x19(
    void *current,
    void *lower_bound,
    void *upper_bound,
    const RuntimeFirebloomType *type
) {
    return (RuntimeFirebloomSpan){
        .current = current,
        .lower_bound = lower_bound,
        .upper_bound = upper_bound,
        .type = type,
    };
}

RuntimeFirebloomSpan boot_breadcrumb_outlined_save_span_bounds_x21_x20_x19(
    RuntimeFirebloomSpan span
) {
    return span;
}

const RuntimeFirebloomType *boot_breadcrumb_outlined_load_byte_type_x3(void) {
    return &breadcrumb_byte_type;
}

RuntimeFirebloomSpan boot_breadcrumb_outlined_restore_span_x23_x26(
    RuntimeFirebloomSpan span
) {
    return span;
}

RuntimeFirebloomSpan boot_breadcrumb_outlined_save_span_x19_x22(
    RuntimeFirebloomSpan span
) {
    return span;
}
