#include "avp/runtime/boot/boot_breadcrumb_outliners.h"

#include <assert.h>
#include <stdio.h>

static void assert_span_equal(
    RuntimeFirebloomSpan left,
    RuntimeFirebloomSpan right
) {
    assert(left.current == right.current);
    assert(left.lower_bound == right.lower_bound);
    assert(left.upper_bound == right.upper_bound);
    assert(left.type == right.type);
}

int main(void) {
    uint8_t storage[32] = {0};
    BootBreadcrumbOffsetComparison comparison =
        boot_breadcrumb_outlined_compare_base_plus_offset_to_upper(
            storage, 12, storage + sizeof(storage));
    assert(comparison.address == storage + 12);
    assert(comparison.below_upper);
    comparison = boot_breadcrumb_outlined_compare_base_plus_offset_to_upper(
        storage, sizeof(storage), storage + sizeof(storage));
    assert(!comparison.below_upper);

    const RuntimeFirebloomType *byte_type =
        boot_breadcrumb_outlined_load_byte_type_x3();
    assert(byte_type->identity == UINT64_C(0x00144f18));
    assert(byte_type->element_size == 1);
    assert(byte_type->bytewise_copyable);

    const RuntimeFirebloomSpan span = {
        .current = storage + 4,
        .lower_bound = storage,
        .upper_bound = storage + sizeof(storage),
        .type = byte_type,
    };
    assert_span_equal(
        boot_breadcrumb_outlined_restore_span_bounds_x21_x20_x19(
            span.current, span.lower_bound, span.upper_bound, span.type),
        span);
    assert_span_equal(
        boot_breadcrumb_outlined_save_span_bounds_x21_x20_x19(span), span);
    assert_span_equal(
        boot_breadcrumb_outlined_restore_span_x23_x26(span), span);
    assert_span_equal(boot_breadcrumb_outlined_save_span_x19_x22(span), span);

    puts("boot breadcrumb outliner tests passed");
    return 0;
}
