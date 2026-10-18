#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "avp/runtime/firebloom/runtime_firebloom.h"

enum {
    RUNTIME_PAGE_ATTRIBUTE_PAGE_SHIFT = 14,
    RUNTIME_PAGE_ATTRIBUTE_PAGE_SIZE = 1 << RUNTIME_PAGE_ATTRIBUTE_PAGE_SHIFT,
    RUNTIME_PAGE_ATTRIBUTE_MAX_RECORDS = 32,
    RUNTIME_PAGE_ATTRIBUTE_SECONDARY_SET = 0x1,
    RUNTIME_PAGE_ATTRIBUTE_SECONDARY_CLEAR = 0x2,
    RUNTIME_PAGE_ATTRIBUTE_PRIMARY_SET = 0x4,
    RUNTIME_PAGE_ATTRIBUTE_PRIMARY_CLEAR = 0x8,
    RUNTIME_PAGE_ATTRIBUTE_UNINITIALIZED = 0x10,
    RUNTIME_PAGE_ATTRIBUTE_STATIC_CLAIM = 0x10000,
};

typedef struct {
    uint32_t begin;
    uint32_t end;
} RuntimePageAttributePageSpan;

typedef struct {
    uintptr_t base;
    size_t size;
} RuntimePageAttributeMemoryRange;

typedef struct {
    uint32_t first_page;
    uint32_t page_limit;
    RuntimeFirebloomSpan primary_bitmap;
    RuntimeFirebloomSpan secondary_bitmap;
} RuntimePageAttributeRecord;

typedef struct {
    RuntimePageAttributeRecord *record;
    RuntimePageAttributeRecord *records_begin;
    RuntimePageAttributeRecord *records_end;
} RuntimePageAttributeLookupResult;

typedef struct {
    RuntimePageAttributeRecord *current;
    RuntimePageAttributeRecord *lower_bound;
    RuntimePageAttributeRecord *upper_bound;
} RuntimePageAttributeRecordSpan;

typedef struct {
    void *current;
    void *lower_bound;
    void *upper_bound;
} RuntimePageAttributeByteView;

typedef struct {
    RuntimeFirebloomSpan bitmap;
    uint32_t bit_capacity;
    uint32_t begin;
    uint32_t end;
} RuntimePageAttributeBitmapArguments;

typedef struct {
    uint32_t attribute_flags;
    uint32_t options;
} RuntimePageAttributeClaimArguments;

typedef struct {
    uintptr_t base;
    size_t size;
    uint32_t attribute_flags;
    uint32_t options;
} RuntimePageAttributeStaticRangeArguments;

typedef struct {
    uintptr_t values[4];
    uint32_t stack_value;
} RuntimePageAttributeOptionalArguments;

typedef struct {
    RuntimePageAttributeRecord *records;
    size_t record_count;
    size_t record_capacity;
    uintptr_t allowed_lower;
    uintptr_t allowed_upper;
    uintptr_t forbidden_lower;
    uintptr_t forbidden_upper;
    bool policy_active;
    bool initialized;
} RuntimePageAttributeRegistry;

#define RUNTIME_PAGE_ATTRIBUTES_PUBLIC __attribute__((used, retain))

RUNTIME_PAGE_ATTRIBUTES_PUBLIC RuntimePageAttributeRecordSpan
runtime_page_attribute_registry_lower_bound_slot_span(
    RuntimePageAttributeRegistry *registry);
RUNTIME_PAGE_ATTRIBUTES_PUBLIC RuntimePageAttributeClaimArguments
runtime_page_attribute_outlined_prepare_flags_6_10000(void);
RUNTIME_PAGE_ATTRIBUTES_PUBLIC uint32_t
runtime_page_span_attribute_mask(
    RuntimePageAttributeRegistry *registry,
    RuntimePageAttributeLookupResult lookup,
    RuntimePageAttributePageSpan pages
);
RUNTIME_PAGE_ATTRIBUTES_PUBLIC RuntimePageAttributeByteView
runtime_page_attribute_registry_state_span(
    RuntimePageAttributeRegistry *registry);
RUNTIME_PAGE_ATTRIBUTES_PUBLIC RuntimePageAttributeLookupResult
runtime_page_attribute_registry_lookup(
    RuntimePageAttributeRegistry *registry,
    RuntimePageAttributePageSpan pages
);
RUNTIME_PAGE_ATTRIBUTES_PUBLIC uint32_t
runtime_memory_range_page_attribute_mask(
    RuntimePageAttributeRegistry *registry,
    RuntimePageAttributeLookupResult lookup,
    RuntimePageAttributeMemoryRange range);
RUNTIME_PAGE_ATTRIBUTES_PUBLIC _Noreturn void
runtime_page_span_conversion_failure(void);
RUNTIME_PAGE_ATTRIBUTES_PUBLIC bool
runtime_page_span_attribute_claim(
    RuntimePageAttributeRegistry *registry,
    RuntimePageAttributeLookupResult lookup,
    RuntimePageAttributePageSpan pages,
    uint32_t attribute_flags,
    uint32_t options
);
RUNTIME_PAGE_ATTRIBUTES_PUBLIC bool runtime_page_attribute_flags_valid_for_span(
    RuntimePageAttributeRegistry *registry,
    uint32_t attribute_flags,
    RuntimePageAttributePageSpan pages
);
RUNTIME_PAGE_ATTRIBUTES_PUBLIC bool
runtime_page_attribute_address_allowed(
    RuntimePageAttributeRegistry *registry,
    uintptr_t address
);
RUNTIME_PAGE_ATTRIBUTES_PUBLIC bool
runtime_byte_bounds_attribute_claim(
    RuntimePageAttributeRegistry *registry,
    RuntimePageAttributeLookupResult lookup,
    uintptr_t lower,
    uintptr_t upper,
    uint32_t attribute_flags,
    uint32_t options
);
RUNTIME_PAGE_ATTRIBUTES_PUBLIC bool
runtime_memory_range_attribute_claim(
    RuntimePageAttributeRegistry *registry,
    RuntimePageAttributeLookupResult lookup,
    RuntimePageAttributeMemoryRange range,
    uint32_t attribute_flags,
    uint32_t options
);
RUNTIME_PAGE_ATTRIBUTES_PUBLIC bool
runtime_page_span_attribute_clear(
    RuntimePageAttributeRegistry *registry,
    RuntimePageAttributeLookupResult lookup,
    RuntimePageAttributePageSpan pages,
    uint32_t attribute_flags,
    uint32_t options
);
RUNTIME_PAGE_ATTRIBUTES_PUBLIC bool
runtime_byte_bounds_attribute_clear(
    RuntimePageAttributeRegistry *registry,
    RuntimePageAttributeLookupResult lookup,
    uintptr_t lower,
    uintptr_t upper,
    uint32_t attribute_flags,
    uint32_t options
);
RUNTIME_PAGE_ATTRIBUTES_PUBLIC bool
runtime_memory_range_attribute_clear(
    RuntimePageAttributeRegistry *registry,
    RuntimePageAttributeLookupResult lookup,
    RuntimePageAttributeMemoryRange range,
    uint32_t attribute_flags,
    uint32_t options
);
RUNTIME_PAGE_ATTRIBUTES_PUBLIC void runtime_page_attribute_registry_initialize(
    RuntimePageAttributeRegistry *registry,
    RuntimePageAttributeRecord *records,
    size_t record_capacity,
    const RuntimePageAttributeMemoryRange *regions,
    size_t region_count,
    void *bitmap_storage,
    size_t bitmap_storage_size
);
RUNTIME_PAGE_ATTRIBUTES_PUBLIC uint8_t *
runtime_page_attribute_record_initialize(
    RuntimePageAttributeRecord *record,
    RuntimePageAttributeMemoryRange region,
    uint8_t *bitmap_cursor,
    uint8_t *bitmap_limit
);
RUNTIME_PAGE_ATTRIBUTES_PUBLIC uint8_t *
runtime_page_attribute_outlined_initialize_registry_record(
    RuntimePageAttributeRecord *record,
    RuntimePageAttributeMemoryRange region,
    uint8_t *bitmap_cursor,
    uint8_t *bitmap_limit
);
RUNTIME_PAGE_ATTRIBUTES_PUBLIC RuntimePageAttributeByteView
runtime_page_attribute_policy_active_flag_bounded_view(
    RuntimePageAttributeRegistry *registry);
RUNTIME_PAGE_ATTRIBUTES_PUBLIC RuntimePageAttributeOptionalArguments
runtime_page_attribute_zero_optional_arguments(void);
RUNTIME_PAGE_ATTRIBUTES_PUBLIC RuntimePageAttributeOptionalArguments
runtime_page_attribute_zero_optional_arguments_clone(void);
RUNTIME_PAGE_ATTRIBUTES_PUBLIC bool
runtime_page_attribute_registry_test_initialized(
    const RuntimePageAttributeRegistry *registry);
RUNTIME_PAGE_ATTRIBUTES_PUBLIC uint8_t
runtime_page_attribute_policy_active_flag_to_w8(
    const RuntimePageAttributeRegistry *registry);
RUNTIME_PAGE_ATTRIBUTES_PUBLIC RuntimePageAttributeLookupResult
runtime_page_attribute_restore_lookup_result(
    RuntimePageAttributeLookupResult lookup);
RUNTIME_PAGE_ATTRIBUTES_PUBLIC void runtime_page_attribute_record_update_range(
    RuntimePageAttributeRecord *record,
    RuntimePageAttributePageSpan pages,
    uint32_t attribute_flags,
    bool claim
);
RUNTIME_PAGE_ATTRIBUTES_PUBLIC bool
runtime_page_attribute_save_lookup_result_and_validate_flags(
    RuntimePageAttributeRegistry *registry,
    RuntimePageAttributeLookupResult lookup,
    RuntimePageAttributePageSpan pages,
    uint32_t attribute_flags,
    RuntimePageAttributeLookupResult *saved_lookup
);
RUNTIME_PAGE_ATTRIBUTES_PUBLIC RuntimePageAttributeBitmapArguments
runtime_page_attribute_load_secondary_bitmap_arguments(
    RuntimePageAttributeRecord *record,
    RuntimePageAttributePageSpan pages
);
RUNTIME_PAGE_ATTRIBUTES_PUBLIC void runtime_page_attribute_policy_initialize(
    RuntimePageAttributeRegistry *registry,
    uintptr_t allowed_lower,
    uintptr_t allowed_upper,
    uintptr_t forbidden_lower,
    uintptr_t forbidden_upper
);
RUNTIME_PAGE_ATTRIBUTES_PUBLIC RuntimePageAttributeStaticRangeArguments
runtime_page_attribute_outlined_prepare_static_range_arguments(
    RuntimePageAttributeMemoryRange range,
    uint32_t attribute_flags,
    uint32_t options
);
RUNTIME_PAGE_ATTRIBUTES_PUBLIC size_t
runtime_page_attribute_rounded_range_size(uintptr_t base, size_t size);
RUNTIME_PAGE_ATTRIBUTES_PUBLIC bool
runtime_memory_range_attribute_claim_flags_6_10000(
    RuntimePageAttributeRegistry *registry,
    RuntimePageAttributeLookupResult lookup,
    RuntimePageAttributeMemoryRange range);
RUNTIME_PAGE_ATTRIBUTES_PUBLIC RuntimePageAttributeRecordSpan
runtime_page_attribute_registry_upper_bound_slot_span(
    RuntimePageAttributeRegistry *registry);
