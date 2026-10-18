#include "avp/image/boot/boot_image_outliners.h"

extern uintptr_t __stack_chk_guard;

BootIMG4PartialDecodeArguments
boot_img4_outlined_prepare_partial_decode_arguments(
    DERItem *item,
    void *decoded,
    size_t decoded_size
) {
    return (BootIMG4PartialDecodeArguments){
        .item = item,
        .decoded_current = decoded,
        .decoded_lower_bound = decoded,
        .decoded_upper_bound = (uint8_t *)decoded + decoded_size,
    };
}

RuntimeFirebloomSpan boot_img4_outlined_prepare_der_item_span(DERItem *item) {
    return (RuntimeFirebloomSpan){
        .current = item,
        .lower_bound = item,
        .upper_bound = item + 1,
        .type = NULL,
    };
}

bool boot_img4_stack_guard_compare_saved_x8(uint64_t saved_guard) {
    return saved_guard == __stack_chk_guard;
}

BootImageRegisterFrame boot_image_register_provider_frame100_save_x19_x28(
    BootImageRegisterFrame frame
) {
    return frame;
}

BootImageRegisterFrame boot_image_register_provider_frame100_restore_x19_x28(
    BootImageRegisterFrame frame
) {
    return frame;
}

BootImagePageBases boot_image_outlined_load_global_page_bases_x20_x22(void) {
    return (BootImagePageBases){
        .primary = UINT64_C(0x7002a000),
        .secondary = UINT64_C(0x7002a000),
    };
}

BootImageSpanSlots boot_image_source_lookup_prepare_registry_span_slots(
    void *record
) {
    uint8_t *base = record;
    return (BootImageSpanSlots){
        .current_slot = (void **)(base + 0x18),
        .lower_slot = (void **)(base + 0x10),
        .upper_slot = (void **)(base + 0x08),
        .type_slot = (const RuntimeFirebloomType **)base,
    };
}

BootImageSpanSlots boot_image_source_lookup_prepare_next_span_slots(
    void *record
) {
    uint8_t *base = record;
    return (BootImageSpanSlots){
        .current_slot = (void **)(base + 0x20),
        .lower_slot = (void **)(base + 0x28),
        .upper_slot = (void **)(base + 0x30),
        .type_slot = (const RuntimeFirebloomType **)(base + 0x38),
    };
}

bool boot_image_source_lookup_validate_current_node(
    RuntimeFirebloomSpan node,
    size_t record_size
) {
    const RuntimeFirebloomSpan validated =
        boot_image_validate_source_registry_node_span(node, record_size);
    return validated.current == validated.lower_bound;
}

RuntimeFirebloomSpan boot_image_outlined_restore_storage_span_to_x4_x7(
    RuntimeFirebloomSpan span
) {
    return span;
}

bool boot_image_validate_property_callback_context(RuntimeFirebloomSpan context) {
    const RuntimeFirebloomSpan validated =
        boot_image_validate_property_context_span(context, 0x80);
    return validated.current == validated.lower_bound;
}

BootImageOffsetSpan boot_image_outlined_validate_output_span_and_add_x22(
    RuntimeFirebloomSpan output,
    size_t offset
) {
    output = boot_img4_validate_byte_span(output);
    return (BootImageOffsetSpan){
        .span = output,
        .offset_current = (uint8_t *)output.current + offset,
    };
}
