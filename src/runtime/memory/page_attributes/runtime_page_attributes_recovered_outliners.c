#include "runtime_page_attributes_recovered_private.h"

RuntimePageAttributeClaimArguments
runtime_page_attribute_outlined_prepare_flags_6_10000(void) {
    return (RuntimePageAttributeClaimArguments){
        .attribute_flags = 6,
        .options = RUNTIME_PAGE_ATTRIBUTE_STATIC_CLAIM,
    };
}

RuntimePageAttributeOptionalArguments
runtime_page_attribute_zero_optional_arguments(void) {
    return (RuntimePageAttributeOptionalArguments){0};
}

RuntimePageAttributeOptionalArguments
runtime_page_attribute_zero_optional_arguments_clone(void) {
    return (RuntimePageAttributeOptionalArguments){0};
}

RuntimePageAttributeLookupResult runtime_page_attribute_restore_lookup_result(
    RuntimePageAttributeLookupResult lookup) {
    return lookup;
}

RuntimePageAttributeBitmapArguments
runtime_page_attribute_load_secondary_bitmap_arguments(
    RuntimePageAttributeRecord *record,
    RuntimePageAttributePageSpan pages
) {
    if (record == 0 || pages.begin < record->first_page ||
        pages.end > record->page_limit || pages.begin >= pages.end) {
        runtime_page_attributes_fail();
    }
    return (RuntimePageAttributeBitmapArguments){
        .bitmap = record->secondary_bitmap,
        .bit_capacity = record->page_limit - record->first_page,
        .begin = pages.begin - record->first_page,
        .end = pages.end - record->first_page,
    };
}

RuntimePageAttributeStaticRangeArguments
runtime_page_attribute_outlined_prepare_static_range_arguments(
    RuntimePageAttributeMemoryRange range,
    uint32_t attribute_flags,
    uint32_t options
) {
    if (range.size == 0) {
        runtime_page_span_conversion_failure();
    }
    return (RuntimePageAttributeStaticRangeArguments){
        .base = range.base,
        .size = range.size,
        .attribute_flags = attribute_flags,
        .options = options,
    };
}

size_t runtime_page_attribute_rounded_range_size(uintptr_t base, size_t size) {
    const RuntimePageAttributePageSpan pages =
        runtime_page_attribute_range_to_pages((RuntimePageAttributeMemoryRange){
            .base = base,
            .size = size,
        });
    const uintptr_t rounded_begin = (uintptr_t)pages.begin
                                    << RUNTIME_PAGE_ATTRIBUTE_PAGE_SHIFT;
    const uintptr_t rounded_end = (uintptr_t)pages.end
                                  << RUNTIME_PAGE_ATTRIBUTE_PAGE_SHIFT;
    return rounded_end - rounded_begin;
}

bool runtime_memory_range_attribute_claim_flags_6_10000(
    RuntimePageAttributeRegistry *registry,
    RuntimePageAttributeLookupResult lookup,
    RuntimePageAttributeMemoryRange range
) {
    const RuntimePageAttributeClaimArguments arguments =
        runtime_page_attribute_outlined_prepare_flags_6_10000();
    return runtime_memory_range_attribute_claim(
        registry,
        lookup,
        range,
        arguments.attribute_flags,
        arguments.options
    );
}
