#pragma once

#include "avp/image/boot/boot_image_recovered_helpers.h"
#include "avp/image/der/apple_libder.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BOOT_IMAGE_OUTLINER_PUBLIC __attribute__((used, retain))

typedef struct {
    DERItem *item;
    void *decoded_current;
    void *decoded_lower_bound;
    void *decoded_upper_bound;
} BootIMG4PartialDecodeArguments;

typedef struct {
    uintptr_t lanes[10];
} BootImageRegisterFrame;

typedef struct {
    uintptr_t primary;
    uintptr_t secondary;
} BootImagePageBases;

typedef struct {
    void **current_slot;
    void **lower_slot;
    void **upper_slot;
    const RuntimeFirebloomType **type_slot;
} BootImageSpanSlots;

typedef struct {
    RuntimeFirebloomSpan span;
    uint8_t *offset_current;
} BootImageOffsetSpan;

BOOT_IMAGE_OUTLINER_PUBLIC BootIMG4PartialDecodeArguments
boot_img4_outlined_prepare_partial_decode_arguments(
    DERItem *item,
    void *decoded,
    size_t decoded_size
);
BOOT_IMAGE_OUTLINER_PUBLIC RuntimeFirebloomSpan
boot_img4_outlined_prepare_der_item_span(DERItem *item);
BOOT_IMAGE_OUTLINER_PUBLIC bool
boot_img4_stack_guard_compare_saved_x8(uint64_t saved_guard);
BOOT_IMAGE_OUTLINER_PUBLIC BootImageRegisterFrame
boot_image_register_provider_frame100_save_x19_x28(
    BootImageRegisterFrame frame
);
BOOT_IMAGE_OUTLINER_PUBLIC BootImageRegisterFrame
boot_image_register_provider_frame100_restore_x19_x28(
    BootImageRegisterFrame frame
);
BOOT_IMAGE_OUTLINER_PUBLIC BootImagePageBases
boot_image_outlined_load_global_page_bases_x20_x22(void);
BOOT_IMAGE_OUTLINER_PUBLIC BootImageSpanSlots
boot_image_source_lookup_prepare_registry_span_slots(void *record);
BOOT_IMAGE_OUTLINER_PUBLIC BootImageSpanSlots
boot_image_source_lookup_prepare_next_span_slots(void *record);
BOOT_IMAGE_OUTLINER_PUBLIC bool boot_image_source_lookup_validate_current_node(
    RuntimeFirebloomSpan node,
    size_t record_size
);
BOOT_IMAGE_OUTLINER_PUBLIC RuntimeFirebloomSpan
boot_image_outlined_restore_storage_span_to_x4_x7(
    RuntimeFirebloomSpan span
);
BOOT_IMAGE_OUTLINER_PUBLIC bool boot_image_validate_property_callback_context(
    RuntimeFirebloomSpan context
);
BOOT_IMAGE_OUTLINER_PUBLIC BootImageOffsetSpan
boot_image_outlined_validate_output_span_and_add_x22(
    RuntimeFirebloomSpan output,
    size_t offset
);
