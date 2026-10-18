#include "runtime_page_attributes_recovered_private.h"

#include <limits.h>

_Noreturn void runtime_page_attributes_fail(void) { __builtin_trap(); }

size_t runtime_page_attribute_bitmap_bytes(uint32_t page_count) {
    if (page_count == 0) {
        runtime_page_attributes_fail();
    }
    /* The shared bounded engine reads complete words at both range edges. */
    return ((size_t)page_count + RUNTIME_PAGE_ATTRIBUTE_BITMAP_WORD_BITS - 1) /
           RUNTIME_PAGE_ATTRIBUTE_BITMAP_WORD_BITS * sizeof(uint64_t);
}

RuntimeFirebloomSpan runtime_page_attribute_make_bitmap_span(
    uint8_t *storage,
    size_t size
) {
    if (storage == 0 || size == 0 ||
        (uintptr_t)storage + size < (uintptr_t)storage) {
        runtime_page_attributes_fail();
    }
    return (RuntimeFirebloomSpan){
        .current = storage,
        .lower_bound = storage,
        .upper_bound = storage + size,
        .type = 0,
    };
}

RuntimePageAttributePageSpan
runtime_page_attribute_range_to_pages(RuntimePageAttributeMemoryRange range) {
    uintptr_t end = 0;
    if (range.size == 0 ||
        __builtin_add_overflow(range.base, range.size, &end)) {
        runtime_page_span_conversion_failure();
    }
    const uintptr_t page_mask = RUNTIME_PAGE_ATTRIBUTE_PAGE_SIZE - 1;
    const uintptr_t page_begin =
        range.base >> RUNTIME_PAGE_ATTRIBUTE_PAGE_SHIFT;
    const uintptr_t page_end =
        (end + page_mask) >> RUNTIME_PAGE_ATTRIBUTE_PAGE_SHIFT;
    if (end + page_mask < end || page_begin >= page_end ||
        page_end > UINT32_MAX) {
        runtime_page_span_conversion_failure();
    }
    return (RuntimePageAttributePageSpan){
        .begin = (uint32_t)page_begin,
        .end = (uint32_t)page_end,
    };
}

_Noreturn void runtime_page_span_conversion_failure(void) {
    runtime_page_attributes_fail();
}

uint8_t *runtime_page_attribute_record_initialize(
    RuntimePageAttributeRecord *record,
    RuntimePageAttributeMemoryRange region,
    uint8_t *bitmap_cursor,
    uint8_t *bitmap_limit
) {
    if (record == 0 || bitmap_cursor == 0 || bitmap_limit == 0 ||
        region.size == 0 ||
        (region.base & (RUNTIME_PAGE_ATTRIBUTE_PAGE_SIZE - 1)) != 0 ||
        (region.size & (RUNTIME_PAGE_ATTRIBUTE_PAGE_SIZE - 1)) != 0) {
        runtime_page_attributes_fail();
    }
    const RuntimePageAttributePageSpan pages =
        runtime_page_attribute_range_to_pages(region);
    const uint32_t page_count = pages.end - pages.begin;
    const size_t bitmap_bytes = runtime_page_attribute_bitmap_bytes(page_count);
    if (bitmap_limit < bitmap_cursor ||
        (size_t)(bitmap_limit - bitmap_cursor) < bitmap_bytes * 2) {
        runtime_page_attributes_fail();
    }

    record->first_page = pages.begin;
    record->page_limit = pages.end;
    record->primary_bitmap =
        runtime_page_attribute_make_bitmap_span(bitmap_cursor, bitmap_bytes);
    bitmap_cursor += bitmap_bytes;
    record->secondary_bitmap =
        runtime_page_attribute_make_bitmap_span(bitmap_cursor, bitmap_bytes);
    bitmap_cursor += bitmap_bytes;
    for (uint8_t *cursor = (uint8_t *)record->primary_bitmap.current;
         cursor < bitmap_cursor; ++cursor) {
        *cursor = 0;
    }
    return bitmap_cursor;
}

uint8_t *runtime_page_attribute_outlined_initialize_registry_record(
    RuntimePageAttributeRecord *record,
    RuntimePageAttributeMemoryRange region,
    uint8_t *bitmap_cursor,
    uint8_t *bitmap_limit
) {
    return runtime_page_attribute_record_initialize(
        record,
        region,
        bitmap_cursor,
        bitmap_limit
    );
}

void runtime_page_attribute_registry_initialize(
    RuntimePageAttributeRegistry *registry,
    RuntimePageAttributeRecord *records,
    size_t record_capacity,
    const RuntimePageAttributeMemoryRange *regions,
    size_t region_count,
    void *bitmap_storage,
    size_t bitmap_storage_size
) {
    if (registry == 0 || records == 0 || regions == 0 || bitmap_storage == 0 ||
        registry->initialized || region_count == 0 ||
        region_count > record_capacity ||
        record_capacity > RUNTIME_PAGE_ATTRIBUTE_MAX_RECORDS ||
        bitmap_storage_size == 0) {
        runtime_page_attributes_fail();
    }

    uint8_t *cursor = bitmap_storage;
    uint8_t *const limit = cursor + bitmap_storage_size;
    if (limit < cursor) {
        runtime_page_attributes_fail();
    }
    uintptr_t previous_end = 0;
    for (size_t index = 0; index < region_count; ++index) {
        /* Sorted disjoint records make every later lookup deterministic. */
        if (index != 0 && regions[index].base < previous_end) {
            runtime_page_attributes_fail();
        }
        uintptr_t region_end = 0;
        if (__builtin_add_overflow(regions[index].base, regions[index].size,
                                   &region_end)) {
            runtime_page_attributes_fail();
        }
        cursor = runtime_page_attribute_record_initialize(
            &records[index],
            regions[index],
            cursor,
            limit
        );
        previous_end = region_end;
    }

    registry->records = records;
    registry->record_count = region_count;
    registry->record_capacity = record_capacity;
    registry->allowed_lower = 0;
    registry->allowed_upper = 0;
    registry->forbidden_lower = 0;
    registry->forbidden_upper = 0;
    registry->policy_active = false;
    registry->initialized = true;
}

bool runtime_page_attribute_registry_test_initialized(
    const RuntimePageAttributeRegistry *registry) {
    return registry != 0 && registry->initialized;
}

RuntimePageAttributeLookupResult
runtime_page_attribute_registry_lookup(
    RuntimePageAttributeRegistry *registry,
    RuntimePageAttributePageSpan pages
) {
    RuntimePageAttributeLookupResult result = {0};
    if (registry == 0 || !registry->initialized || pages.begin >= pages.end) {
        return result;
    }
    result.records_begin = registry->records;
    result.records_end = registry->records + registry->record_count;
    size_t lower = 0;
    size_t upper = registry->record_count;
    /* Boot-time ordering lets the hot query path avoid a linear scan. */
    while (lower < upper) {
        const size_t middle = lower + (upper - lower) / 2;
        RuntimePageAttributeRecord *record = &registry->records[middle];
        if (pages.begin < record->first_page) {
            upper = middle;
        } else if (pages.begin >= record->page_limit) {
            lower = middle + 1;
        } else {
            if (pages.end <= record->page_limit) {
                result.record = record;
            }
            return result;
        }
    }
    return result;
}

RuntimePageAttributeRecord *
runtime_page_attribute_require_record(
    RuntimePageAttributeRegistry *registry,
    RuntimePageAttributeLookupResult lookup,
    RuntimePageAttributePageSpan pages
) {
    RuntimePageAttributeRecord *record = lookup.record;
    if (record == 0) {
        lookup = runtime_page_attribute_registry_lookup(registry, pages);
        record = lookup.record;
    }
    if (record == 0 || pages.begin < record->first_page ||
        pages.end > record->page_limit || pages.begin >= pages.end) {
        runtime_page_attributes_fail();
    }
    return record;
}

RuntimePageAttributeRecordSpan
runtime_page_attribute_registry_lower_bound_slot_span(
    RuntimePageAttributeRegistry *registry) {
    if (registry == 0 || !registry->initialized) {
        runtime_page_attributes_fail();
    }
    return (RuntimePageAttributeRecordSpan){
        .current = registry->records,
        .lower_bound = registry->records,
        .upper_bound = registry->records + registry->record_count,
    };
}

RuntimePageAttributeRecordSpan
runtime_page_attribute_registry_upper_bound_slot_span(
    RuntimePageAttributeRegistry *registry) {
    RuntimePageAttributeRecordSpan span =
        runtime_page_attribute_registry_lower_bound_slot_span(registry);
    span.current = span.upper_bound;
    return span;
}

RuntimePageAttributeByteView runtime_page_attribute_registry_state_span(
    RuntimePageAttributeRegistry *registry) {
    if (registry == 0) {
        runtime_page_attributes_fail();
    }
    return (RuntimePageAttributeByteView){
        .current = &registry->initialized,
        .lower_bound = &registry->initialized,
        .upper_bound =
            (uint8_t *)&registry->initialized + sizeof(registry->initialized),
    };
}
