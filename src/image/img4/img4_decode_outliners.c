#include "avp/image/img4/img4_decode_evaluation.h"
#include "avp/runtime/support/stack_guard.h"

#include <stddef.h>
#include <stdint.h>


bool IMG4_DECODE_EVALUATION_PUBLIC
img4_decode_stack_guard_compare_saved_x8(uint64_t saved_guard) {
    return runtime_stack_guard() == saved_guard;
}

uint64_t IMG4_DECODE_EVALUATION_PUBLIC img4_decode_stack_guard_load_to_x8(void) {
    return runtime_stack_guard();
}

Img4StackGuardPrologueState IMG4_DECODE_EVALUATION_PUBLIC
img4_decode_stack_guard_preserve_x0_load_to_x9(const DERItem *item) {
    return (Img4StackGuardPrologueState){item, runtime_stack_guard()};
}

bool IMG4_DECODE_EVALUATION_PUBLIC
img4_decode_stack_guard_compare_saved_x8_clone(uint64_t saved_guard) {
    return img4_decode_stack_guard_compare_saved_x8(saved_guard);
}

void *IMG4_DECODE_EVALUATION_PUBLIC
img4_decode_property_outlined_bind_output_and_save_stack_guard(void *output) {
    return output;
}

uint64_t IMG4_DECODE_EVALUATION_PUBLIC
img4_decode_stack_guard_load_to_x8_clone(void) {
    return runtime_stack_guard();
}

void IMG4_DECODE_EVALUATION_PUBLIC
img4_decode_property_outlined_zero_decode_scratch(void *scratch) {
    uint8_t *bytes = scratch;
    for (size_t index = 0; index < 48; ++index) {
        bytes[index] = 0;
    }
}

DERReturn IMG4_DECODE_EVALUATION_PUBLIC
img4_decode_property_outlined_decode_private_tag(
    const DERItem *property,
    uint32_t fourcc,
    Img4DecodedProperty *decoded
) {
    return DERImg4DecodeProperty(
        property,
        Img4DecodePrivateTag(fourcc),
        decoded
    );
}

const DERItem *IMG4_DECODE_EVALUATION_PUBLIC
img4_decode_property_outlined_prepare_value_parse(
    const Img4DecodedProperty *decoded) {
    return decoded == NULL ? NULL : &decoded->value.item;
}
