#include "runtime_page_attributes_recovered_private.h"

#include "avp/runtime/memory/runtime_bounded_bitmap.h"

static bool runtime_page_attribute_modes_valid(uint32_t attribute_flags) {
    if ((attribute_flags & ~UINT32_C(0xf)) != 0) {
        return false;
    }
    const uint32_t secondary_mode = attribute_flags & UINT32_C(0x3);
    const uint32_t primary_mode = (attribute_flags >> 2) & UINT32_C(0x3);
    /* Each two-bit field encodes preserve, set, or clear for one bitmap. */
    return secondary_mode != 3 && primary_mode != 3 &&
           (secondary_mode != 0 || primary_mode != 0);
}

static uint32_t
runtime_page_attribute_class_mask(
    RuntimeBitmapRangeClass classification,
    uint32_t set_bit,
    uint32_t mixed_bit
) {
    /* AVP exposes set and mixed as distinct mask bits for each bitmap. */
    if (classification == RUNTIME_BITMAP_RANGE_SET) {
        return set_bit;
    }
    return classification == RUNTIME_BITMAP_RANGE_MIXED ? mixed_bit : 0;
}

static RuntimePageAttributePageSpan
runtime_page_attribute_record_indices(
    RuntimePageAttributeRecord *record,
    RuntimePageAttributePageSpan pages
) {
    if (record == 0 || pages.begin < record->first_page ||
        pages.end > record->page_limit || pages.begin >= pages.end) {
        runtime_page_attributes_fail();
    }
    return (RuntimePageAttributePageSpan){
        .begin = pages.begin - record->first_page,
        .end = pages.end - record->first_page,
    };
}

uint32_t
runtime_page_span_attribute_mask(
    RuntimePageAttributeRegistry *registry,
    RuntimePageAttributeLookupResult lookup,
    RuntimePageAttributePageSpan pages
) {
    if (!runtime_page_attribute_registry_test_initialized(registry)) {
        return RUNTIME_PAGE_ATTRIBUTE_UNINITIALIZED;
    }
    RuntimePageAttributeRecord *record =
        runtime_page_attribute_require_record(registry, lookup, pages);
    const RuntimePageAttributePageSpan indices =
        runtime_page_attribute_record_indices(record, pages);
    const uint32_t capacity = record->page_limit - record->first_page;
    uint32_t mask = runtime_page_attribute_class_mask(
        runtime_bitmap_classify_range(
            record->primary_bitmap,
            capacity,
            indices.begin,
            indices.end
        ),
        RUNTIME_PAGE_ATTRIBUTE_PRIMARY_SET,
        RUNTIME_PAGE_ATTRIBUTE_PRIMARY_CLEAR
    );
    mask |= runtime_page_attribute_class_mask(
        runtime_bitmap_classify_range(
            record->secondary_bitmap,
            capacity,
            indices.begin,
            indices.end
        ),
        RUNTIME_PAGE_ATTRIBUTE_SECONDARY_SET,
        RUNTIME_PAGE_ATTRIBUTE_SECONDARY_CLEAR
    );
    return mask;
}

uint32_t runtime_memory_range_page_attribute_mask(
    RuntimePageAttributeRegistry *registry,
    RuntimePageAttributeLookupResult lookup,
    RuntimePageAttributeMemoryRange range
) {
    return runtime_page_span_attribute_mask(
        registry,
        lookup,
        runtime_page_attribute_range_to_pages(range)
    );
}

bool runtime_page_attribute_flags_valid_for_span(
    RuntimePageAttributeRegistry *registry,
    uint32_t attribute_flags,
    RuntimePageAttributePageSpan pages
) {
    if (!runtime_page_attribute_registry_test_initialized(registry) ||
        !runtime_page_attribute_modes_valid(attribute_flags) ||
        pages.begin >= pages.end) {
        return false;
    }
    const uintptr_t first_address = (uintptr_t)pages.begin
                                    << RUNTIME_PAGE_ATTRIBUTE_PAGE_SHIFT;
    const uintptr_t final_address =
        ((uintptr_t)pages.end << RUNTIME_PAGE_ATTRIBUTE_PAGE_SHIFT) - 1;
    return runtime_page_attribute_address_allowed(registry, first_address) &&
           runtime_page_attribute_address_allowed(registry, final_address) &&
           runtime_page_attribute_registry_lookup(registry, pages).record != 0;
}

void runtime_page_attribute_record_update_range(
    RuntimePageAttributeRecord *record,
    RuntimePageAttributePageSpan pages,
    uint32_t attribute_flags,
    bool claim
) {
    if (!runtime_page_attribute_modes_valid(attribute_flags)) {
        runtime_page_attributes_fail();
    }
    const RuntimePageAttributePageSpan indices =
        runtime_page_attribute_record_indices(record, pages);
    const uint32_t capacity = record->page_limit - record->first_page;
    const uint32_t secondary_mode = attribute_flags & UINT32_C(0x3);
    const uint32_t primary_mode = (attribute_flags >> 2) & UINT32_C(0x3);

    if (secondary_mode != 0) {
        const bool set = claim && secondary_mode == 1;
        if (set) {
            runtime_bitmap_set_range(
                record->secondary_bitmap,
                capacity,
                indices.begin,
                indices.end
            );
        } else {
            runtime_bitmap_clear_range(
                record->secondary_bitmap,
                capacity,
                indices.begin,
                indices.end
            );
        }
    }
    if (primary_mode != 0) {
        const bool set = claim && primary_mode == 1;
        if (set) {
            runtime_bitmap_set_range(
                record->primary_bitmap,
                capacity,
                indices.begin,
                indices.end
            );
        } else {
            runtime_bitmap_clear_range(
                record->primary_bitmap,
                capacity,
                indices.begin,
                indices.end
            );
        }
    }
}

bool runtime_page_attribute_save_lookup_result_and_validate_flags(
    RuntimePageAttributeRegistry *registry,
    RuntimePageAttributeLookupResult lookup,
    RuntimePageAttributePageSpan pages,
    uint32_t attribute_flags,
    RuntimePageAttributeLookupResult *saved_lookup
) {
    if (saved_lookup == 0 || !runtime_page_attribute_flags_valid_for_span(
                                 registry, attribute_flags, pages)) {
        return false;
    }
    if (lookup.record == 0) {
        lookup = runtime_page_attribute_registry_lookup(registry, pages);
    }
    if (lookup.record == 0) {
        return false;
    }
    *saved_lookup = lookup;
    return true;
}

bool runtime_page_span_attribute_claim(
    RuntimePageAttributeRegistry *registry,
    RuntimePageAttributeLookupResult lookup,
    RuntimePageAttributePageSpan pages,
    uint32_t attribute_flags,
    uint32_t options
) {
    if ((options & ~RUNTIME_PAGE_ATTRIBUTE_STATIC_CLAIM) != 0) {
        runtime_page_attributes_fail();
    }
    RuntimePageAttributeLookupResult saved = {0};
    if (!runtime_page_attribute_save_lookup_result_and_validate_flags(
            registry, lookup, pages, attribute_flags, &saved)) {
        return false;
    }
    runtime_page_attribute_record_update_range(
        saved.record,
        pages,
        attribute_flags,
        true
    );
    return true;
}

bool runtime_page_span_attribute_clear(
    RuntimePageAttributeRegistry *registry,
    RuntimePageAttributeLookupResult lookup,
    RuntimePageAttributePageSpan pages,
    uint32_t attribute_flags,
    uint32_t options
) {
    if ((options & ~RUNTIME_PAGE_ATTRIBUTE_STATIC_CLAIM) != 0) {
        runtime_page_attributes_fail();
    }
    RuntimePageAttributeLookupResult saved = {0};
    if (!runtime_page_attribute_save_lookup_result_and_validate_flags(
            registry, lookup, pages, attribute_flags, &saved)) {
        return false;
    }
    runtime_page_attribute_record_update_range(
        saved.record,
        pages,
        attribute_flags,
        false
    );
    return true;
}

bool runtime_byte_bounds_attribute_claim(
    RuntimePageAttributeRegistry *registry,
    RuntimePageAttributeLookupResult lookup,
    uintptr_t lower,
    uintptr_t upper,
    uint32_t attribute_flags,
    uint32_t options
) {
    if (lower >= upper ||
        ((lower | upper) & (RUNTIME_PAGE_ATTRIBUTE_PAGE_SIZE - 1)) != 0) {
        runtime_page_span_conversion_failure();
    }
    return runtime_page_span_attribute_claim(
        registry,
        lookup,
        (RuntimePageAttributePageSpan){
            .begin = (uint32_t)(lower >> RUNTIME_PAGE_ATTRIBUTE_PAGE_SHIFT),
            .end = (uint32_t)(upper >> RUNTIME_PAGE_ATTRIBUTE_PAGE_SHIFT),
        },
        attribute_flags,
        options
    );
}

bool runtime_memory_range_attribute_claim(
    RuntimePageAttributeRegistry *registry,
    RuntimePageAttributeLookupResult lookup,
    RuntimePageAttributeMemoryRange range,
    uint32_t attribute_flags,
    uint32_t options
) {
    return runtime_page_span_attribute_claim(
        registry,
        lookup,
        runtime_page_attribute_range_to_pages(range),
        attribute_flags,
        options
    );
}

bool runtime_byte_bounds_attribute_clear(
    RuntimePageAttributeRegistry *registry,
    RuntimePageAttributeLookupResult lookup,
    uintptr_t lower,
    uintptr_t upper,
    uint32_t attribute_flags,
    uint32_t options
) {
    if (lower >= upper ||
        ((lower | upper) & (RUNTIME_PAGE_ATTRIBUTE_PAGE_SIZE - 1)) != 0) {
        runtime_page_span_conversion_failure();
    }
    return runtime_page_span_attribute_clear(
        registry,
        lookup,
        (RuntimePageAttributePageSpan){
            .begin = (uint32_t)(lower >> RUNTIME_PAGE_ATTRIBUTE_PAGE_SHIFT),
            .end = (uint32_t)(upper >> RUNTIME_PAGE_ATTRIBUTE_PAGE_SHIFT),
        },
        attribute_flags,
        options
    );
}

bool runtime_memory_range_attribute_clear(
    RuntimePageAttributeRegistry *registry,
    RuntimePageAttributeLookupResult lookup,
    RuntimePageAttributeMemoryRange range,
    uint32_t attribute_flags,
    uint32_t options
) {
    return runtime_page_span_attribute_clear(
        registry,
        lookup,
        runtime_page_attribute_range_to_pages(range),
        attribute_flags,
        options
    );
}
