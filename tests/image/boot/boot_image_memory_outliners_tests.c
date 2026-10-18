#include "avp/image/boot/boot_image_memory_outliners.h"

#include <assert.h>
#include <stdio.h>

uintptr_t __stack_chk_guard = UINT64_C(0x123456789abcdef0);

static RuntimeFirebloomType allocation_type = {
    .identity = UINT64_C(0x00145258),
    .element_size = 0x30,
    .bytewise_copyable = false,
};

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
    uint8_t allocation_bytes[0x60] = {0};
    const RuntimeFirebloomSpan allocation = {
        .current = allocation_bytes,
        .lower_bound = allocation_bytes,
        .upper_bound = allocation_bytes + sizeof(allocation_bytes),
        .type = &allocation_type,
    };
    assert_span_equal(
        boot_image_validate_source_allocation_span(allocation),
        allocation
    );
    assert_span_equal(
        boot_image_outlined_allocation_span_x19_x22(allocation),
        allocation
    );
    assert_span_equal(
        boot_image_outlined_save_allocation_span_x19_x22(allocation),
        allocation
    );
    assert_span_equal(
        boot_image_outlined_restore_source_record_bounds_x26_x24(
            allocation.current,
            allocation.lower_bound,
            allocation.upper_bound,
            allocation.type
        ),
        allocation
    );
    assert_span_equal(
        boot_image_outlined_save_source_record_bounds_x26_x24(allocation),
        allocation
    );

    assert(boot_image_stack_guard_load_to_x8_clone() == __stack_chk_guard);
    assert(boot_image_stack_guard_compare_saved_x8_clone(__stack_chk_guard));
    assert(!boot_image_stack_guard_compare_saved_x8_clone(0));
    assert(boot_image_load_error_code_base_to_w20() ==
           UINT32_C(0x40030001));

    puts("boot image memory outliner tests passed");
    return 0;
}
