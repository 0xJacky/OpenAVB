#include "avp/image/boot/boot_image_outliners.h"

#include <assert.h>
#include <stdio.h>

uintptr_t __stack_chk_guard = UINT64_C(0x123456789abcdef0);

int main(void) {
    DERItem item = {0};
    uint8_t decoded[0x30] = {0};
    const BootIMG4PartialDecodeArguments arguments =
        boot_img4_outlined_prepare_partial_decode_arguments(
            &item,
            decoded,
            sizeof(decoded)
        );
    assert(arguments.item == &item);
    assert(arguments.decoded_current == decoded);
    assert(arguments.decoded_lower_bound == decoded);
    assert(arguments.decoded_upper_bound == decoded + sizeof(decoded));

    const RuntimeFirebloomSpan item_span =
        boot_img4_outlined_prepare_der_item_span(&item);
    assert(item_span.current == &item && item_span.upper_bound == &item + 1);
    assert(boot_img4_stack_guard_compare_saved_x8(__stack_chk_guard));
    assert(!boot_img4_stack_guard_compare_saved_x8(0));

    BootImageRegisterFrame frame = {0};
    frame.lanes[9] = 42;
    frame = boot_image_register_provider_frame100_save_x19_x28(frame);
    frame = boot_image_register_provider_frame100_restore_x19_x28(frame);
    assert(frame.lanes[9] == 42);

    const BootImagePageBases bases =
        boot_image_outlined_load_global_page_bases_x20_x22();
    assert(bases.primary == UINT64_C(0x7002a000));
    assert(bases.secondary == bases.primary);

    uint8_t record[0x40] = {0};
    BootImageSpanSlots slots =
        boot_image_source_lookup_prepare_registry_span_slots(record);
    assert((uint8_t *)slots.current_slot == record + 0x18);
    assert((uint8_t *)slots.type_slot == record);
    slots = boot_image_source_lookup_prepare_next_span_slots(record);
    assert((uint8_t *)slots.current_slot == record + 0x20);
    assert((uint8_t *)slots.type_slot == record + 0x38);

    RuntimeFirebloomSpan node = {
        .current = record,
        .lower_bound = record,
        .upper_bound = record + sizeof(record),
        .type = boot_image_outlined_load_byte_type_to_x7(),
    };
    assert(boot_image_source_lookup_validate_current_node(node, sizeof(record)));
    assert(boot_image_outlined_restore_storage_span_to_x4_x7(node).current ==
           record);

    uint8_t context_bytes[0x80] = {0};
    RuntimeFirebloomSpan context = {
        .current = context_bytes,
        .lower_bound = context_bytes,
        .upper_bound = context_bytes + sizeof(context_bytes),
        .type = boot_image_outlined_load_byte_type_to_x7(),
    };
    assert(boot_image_validate_property_callback_context(context));

    uint8_t output_bytes[8] = {0};
    RuntimeFirebloomSpan output = {
        .current = output_bytes,
        .lower_bound = output_bytes,
        .upper_bound = output_bytes + sizeof(output_bytes),
        .type = boot_image_outlined_load_byte_type_to_x7(),
    };
    const BootImageOffsetSpan offset =
        boot_image_outlined_validate_output_span_and_add_x22(output, 3);
    assert(offset.offset_current == output_bytes + 3);
    puts("boot image outliner tests passed");
    return 0;
}
