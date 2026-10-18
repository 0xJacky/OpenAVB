#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "avp/runtime/firebloom/runtime_firebloom.h"
#include "avp/runtime/memory/runtime_physical_memory.h"


#if __has_attribute(retain)
#define RUNTIME_MEMORY_POLICY_PUBLIC __attribute__((used, retain))
#else
#define RUNTIME_MEMORY_POLICY_PUBLIC __attribute__((used))
#endif

enum {
    RUNTIME_MEMORY_POLICY_BITMAP_SIZE = 80,
    RUNTIME_MEMORY_POLICY_BOOTSTRAP_RECORD_SIZE = 144,
    RUNTIME_MEMORY_POLICY_BOOTSTRAP_RECORD_COUNT = 4,
};

typedef enum {
    RUNTIME_MEMORY_RANGE_GENERAL = 1,
    RUNTIME_MEMORY_RANGE_PRIVATE = 2,
    RUNTIME_MEMORY_RANGE_PLATFORM = 3,
} RuntimeMemoryRangeClass;

typedef struct {
    uint32_t translation_regime;
    uint32_t memory_attribute;
    bool variant;
} RuntimeMemoryPolicyDecodedRegion;

typedef struct {
    uint64_t physical_base;
    uint64_t byte_size;
    uint32_t translation_regime;
    uint32_t memory_attribute;
    uint8_t mapping_flags;
    uint16_t reserved_zero;
} RuntimeMemoryPolicyMapRequest;

typedef struct {
    uintptr_t lower;
    uintptr_t upper;
} RuntimeMemoryPolicyRange;

typedef struct {
    uint32_t tag;
    uint8_t bytes[RUNTIME_MEMORY_POLICY_BOOTSTRAP_RECORD_SIZE - 4];
} RuntimeBootstrapPolicyRecord;

typedef struct {
    const void *candidate;
    const void *lower_bound;
} RuntimeBootstrapPolicyAddressPair;

typedef uint16_t (*RuntimeMemoryPolicyMap)(
    const RuntimeMemoryPolicyMapRequest *request,
    uint64_t *mapped_virtual_address,
    void *context
);
typedef void (*RuntimeMemoryPolicyUnmap)(
    uint64_t base,
    uint64_t size,
    void *context
);
typedef RuntimeFirebloomSpan (*RuntimeMemoryPolicyRegionLookup)(
    uint16_t region_id,
    void *context
);

typedef struct {
    RuntimeMemoryPolicyMap map;
    RuntimeMemoryPolicyUnmap unmap;
    RuntimeMemoryPolicyRegionLookup lookup_region;
    const uint8_t *region_policy_classes;
    size_t region_policy_class_count;
    const RuntimeBootstrapPolicyRecord *bootstrap_records;
    uint16_t bootstrap_record_count;
    uintptr_t physical_base;
    uintptr_t slide;
    RuntimeMemoryPolicyRange platform_range;
    RuntimeMemoryPolicyRange private_range;
    RuntimeMemoryPolicyRange dynamic_private_range;
    const RuntimeMemoryPolicyRange *protected_ranges;
    size_t protected_range_count;
    bool extended_attribute_encoding;
    bool alternate_region_encoding;
    void *context;
} RuntimeMemoryPolicyConfiguration;

extern const RuntimeFirebloomType runtime_memory_policy_byte_type;
extern const RuntimeFirebloomType runtime_memory_policy_qword_type;
extern const RuntimeFirebloomType runtime_memory_policy_record_type;

void runtime_memory_policy_recovered_configure(
    const RuntimeMemoryPolicyConfiguration *configuration
);
void runtime_memory_policy_recovered_reset(void);

RUNTIME_MEMORY_POLICY_PUBLIC bool
runtime_memory_policy_set_c_contains_region_id_thunk(uint32_t region_id);
RUNTIME_MEMORY_POLICY_PUBLIC RuntimeMemoryPolicyDecodedRegion
runtime_memory_policy_outlined_prepare_decode_region_outputs(void);
RUNTIME_MEMORY_POLICY_PUBLIC RuntimeFirebloomSpan
runtime_bootstrap_policy_records_span(void);
RUNTIME_MEMORY_POLICY_PUBLIC RuntimeFirebloomSpan
runtime_bootstrap_policy_record_count_span(void);
RUNTIME_MEMORY_POLICY_PUBLIC RuntimeFirebloomSpan
runtime_memory_policy_set_a_bitmap_span(void);
RUNTIME_MEMORY_POLICY_PUBLIC RuntimeFirebloomSpan
runtime_memory_policy_set_b_bitmap_span(void);
RUNTIME_MEMORY_POLICY_PUBLIC RuntimeFirebloomSpan
runtime_memory_policy_set_c_bitmap_span(void);
RUNTIME_MEMORY_POLICY_PUBLIC bool runtime_memory_bitmap_test_clone(
    const uint64_t *bitmap,
    uintptr_t lower_bound,
    uintptr_t upper_bound,
    uint32_t bit_index
);
RUNTIME_MEMORY_POLICY_PUBLIC _Noreturn void
runtime_memory_bitmap_test_bounds_panic(void);
RUNTIME_MEMORY_POLICY_PUBLIC void runtime_memory_policy_decode_region_id(
    uint32_t region_id,
    uint32_t *translation_regime_current,
    const uint32_t *translation_regime_lower_bound,
    const uint32_t *translation_regime_upper_bound,
    const RuntimeFirebloomType *translation_regime_type,
    uint32_t *memory_attribute_current,
    const uint32_t *memory_attribute_lower_bound,
    const uint32_t *memory_attribute_upper_bound,
    const RuntimeFirebloomType *memory_attribute_type,
    bool *variant_current,
    const bool *variant_lower_bound,
    const bool *variant_upper_bound,
    const RuntimeFirebloomType *variant_type
);
RUNTIME_MEMORY_POLICY_PUBLIC uint32_t
runtime_memory_policy_encode_region_id(
    uint32_t translation_regime,
    uint32_t memory_attribute,
    bool variant
);
RUNTIME_MEMORY_POLICY_PUBLIC bool
runtime_memory_policy_set_c_contains_region_id(uint32_t region_id);
RUNTIME_MEMORY_POLICY_PUBLIC bool
runtime_memory_policy_set_a_contains_region_id(uint32_t region_id);
RUNTIME_MEMORY_POLICY_PUBLIC bool
runtime_memory_policy_set_b_contains_region_id(uint32_t region_id);
RUNTIME_MEMORY_POLICY_PUBLIC uint16_t
runtime_memory_policy_map_regime1_attribute0_flags1_no_output(
    uint64_t physical_base,
    uint64_t byte_size
);
RUNTIME_MEMORY_POLICY_PUBLIC uint16_t
runtime_memory_policy_map_physical_range_svc(
    uint64_t physical_base,
    uint64_t byte_size,
    uint32_t translation_regime,
    uint32_t memory_attribute,
    uint8_t mapping_flags,
    uint16_t reserved_zero,
    uint64_t *mapped_virtual_address_current,
    const uint64_t *mapped_virtual_address_lower_bound,
    const uint64_t *mapped_virtual_address_upper_bound
);
RUNTIME_MEMORY_POLICY_PUBLIC uint16_t
runtime_memory_policy_map_regime0_attribute0_flags3_no_output(
    uint64_t physical_base,
    uint64_t byte_size
);
RUNTIME_MEMORY_POLICY_PUBLIC uint16_t
runtime_memory_policy_map_regime0_attribute6_flags3_no_output(
    uint64_t physical_base,
    uint64_t byte_size
);
RUNTIME_MEMORY_POLICY_PUBLIC uint16_t
runtime_memory_policy_map_regime1_attribute6_flags1_no_output(
    uint64_t physical_base,
    uint64_t byte_size
);
RUNTIME_MEMORY_POLICY_PUBLIC uint16_t
runtime_memory_policy_map_regime1_attribute2_flags1_no_output(
    uint64_t physical_base,
    uint64_t byte_size
);
RUNTIME_MEMORY_POLICY_PUBLIC void runtime_memory_policy_unmap_range_svc(
    uint64_t range_base,
    uint64_t range_size
);
RUNTIME_MEMORY_POLICY_PUBLIC void runtime_memory_policy_apply_region_id(
    uint64_t range_base,
    uint64_t range_size,
    uint32_t region_id
);
RUNTIME_MEMORY_POLICY_PUBLIC void
runtime_memory_policy_unmap_range_and_apply_region_id(
    uint64_t range_base,
    uint64_t range_size,
    uint32_t region_id
);
RUNTIME_MEMORY_POLICY_PUBLIC uint16_t
runtime_memory_policy_map_physical_range_flags3_no_output(
    uint64_t physical_base,
    uint64_t byte_size,
    uint32_t translation_regime,
    uint32_t memory_attribute
);
RUNTIME_MEMORY_POLICY_PUBLIC uintptr_t
runtime_address_profile_load_physical_base_to_x8(void);
RUNTIME_MEMORY_POLICY_PUBLIC bool
runtime_memory_policy_outlined_test_sp_bitmap_bit_x19(uint32_t bit_index);
RUNTIME_MEMORY_POLICY_PUBLIC RuntimeMemoryPolicyMapRequest
runtime_memory_policy_outlined_prepare_regime0_no_output(void);
RUNTIME_MEMORY_POLICY_PUBLIC RuntimeMemoryRangeClass
runtime_memory_range_classify_simple(uintptr_t lower_bound, size_t size);
RUNTIME_MEMORY_POLICY_PUBLIC uintptr_t runtime_address_profile_slide(void);
RUNTIME_MEMORY_POLICY_PUBLIC bool runtime_address_profile_is_alternate(void);
RUNTIME_MEMORY_POLICY_PUBLIC bool
runtime_memory_policy_uses_extended_attribute_encoding(void);
RUNTIME_MEMORY_POLICY_PUBLIC bool
runtime_memory_policy_uses_alternate_region_encoding(void);
RUNTIME_MEMORY_POLICY_PUBLIC uintptr_t
runtime_address_profile_translate(uintptr_t address);
RUNTIME_MEMORY_POLICY_PUBLIC bool
runtime_address_profile_contains_high_window(uintptr_t address);
RUNTIME_MEMORY_POLICY_PUBLIC RuntimeMemoryRangeClass
runtime_memory_range_classify(
    uintptr_t lower_bound,
    size_t size,
    uintptr_t *upper_bound,
    uintptr_t output_lower_bound,
    uintptr_t output_upper_bound
);
RUNTIME_MEMORY_POLICY_PUBLIC RuntimeFirebloomSpan
runtime_bootstrap_primary_upper_slot(void);
RUNTIME_MEMORY_POLICY_PUBLIC RuntimeFirebloomSpan
runtime_bootstrap_primary_lower_slot(void);
RUNTIME_MEMORY_POLICY_PUBLIC bool
runtime_physical_memory_range_is_reserved(
    uintptr_t physical_base,
    size_t size,
    uint16_t region_id
);
RUNTIME_MEMORY_POLICY_PUBLIC RuntimeFirebloomSpan
runtime_bootstrap_policy_record_lookup(uint32_t policy_tag);
RUNTIME_MEMORY_POLICY_PUBLIC void
runtime_memory_policy_require_physical_range(
    uintptr_t lower_bound,
    size_t size,
    uint8_t expected_region_class,
    bool strict_region_check
);
RUNTIME_MEMORY_POLICY_PUBLIC RuntimeMemoryRangeClass
runtime_memory_range_classify_simple_clone(uintptr_t lower_bound, size_t size);
RUNTIME_MEMORY_POLICY_PUBLIC const void *
runtime_bootstrap_policy_outlined_load_upper_compare_lower_to_candidate(
    const void *candidate,
    const void *lower_bound,
    const void *upper_bound
);
RUNTIME_MEMORY_POLICY_PUBLIC RuntimeBootstrapPolicyAddressPair
runtime_bootstrap_policy_outlined_add_record_offset(
    const void *lower_bound,
    size_t byte_offset
);
