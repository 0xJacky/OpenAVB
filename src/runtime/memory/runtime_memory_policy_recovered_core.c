#include "avp/runtime/memory/runtime_memory_policy_recovered.h"

#include "avp/runtime/memory/runtime_memory.h"


const RuntimeFirebloomType runtime_memory_policy_byte_type = {
    .identity = UINT64_C(0x00144f18),
    .element_size = 1,
    .bytewise_copyable = true,
};

const RuntimeFirebloomType runtime_memory_policy_qword_type = {
    .identity = UINT64_C(0x00144f00),
    .element_size = sizeof(uint64_t),
    .bytewise_copyable = true,
};

const RuntimeFirebloomType runtime_memory_policy_record_type = {
    .identity = UINT64_C(0x00147278),
    .element_size = sizeof(RuntimeBootstrapPolicyRecord),
    .bytewise_copyable = false,
};

static const uint8_t set_a_bitmap[RUNTIME_MEMORY_POLICY_BITMAP_SIZE] = {
    0x01, 0x02, 0x60, 0x00, 0x00, 0x1e, 0x40, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x40,
};
static const uint8_t set_b_bitmap[RUNTIME_MEMORY_POLICY_BITMAP_SIZE] = {
    0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x08, 0x00,
    0x70, 0x00, 0x38, 0x00, 0x00, 0x1c, 0x1c, 0x1c,
    0x1c, 0x1c, 0x1c, 0x00, 0x00, 0x02, 0x02, 0x00,
    0x10, 0xc1, 0xff, 0x01, 0xf8, 0x01, 0x18, 0x10,
    0x00, 0x40, 0x00, 0x00, 0x20, 0x00, 0xc0, 0x5b,
    0x04, 0x00, 0x00, 0x80, 0x01, 0xc1, 0x01, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x03, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7e, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x70, 0x10,
    0xf8, 0x01,
};
static const uint8_t set_c_bitmap[RUNTIME_MEMORY_POLICY_BITMAP_SIZE] = {
    0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x08, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x0e, 0x38, 0x09, 0x60, 0x40,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
};

static const RuntimeBootstrapPolicyRecord default_bootstrap_records[] = {
    {.tag = UINT32_C(0x746f6f72)},
    {.tag = UINT32_C(0x00007366)},
    {.tag = UINT32_C(0x726f7473)},
    {.tag = UINT32_C(0x00627375)},
};

static RuntimeMemoryPolicyConfiguration policy_configuration;
static uint16_t bootstrap_record_count =
    RUNTIME_MEMORY_POLICY_BOOTSTRAP_RECORD_COUNT;
static uintptr_t bootstrap_primary_lower;
static uintptr_t bootstrap_primary_upper;

static _Noreturn void memory_policy_panic(void) {
    runtime_firebloom_panic();
}

static RuntimeMemoryPolicyConfiguration memory_policy_configuration(void) {
    RuntimeMemoryPolicyConfiguration configuration = policy_configuration;
    if (configuration.bootstrap_records == 0) {
        configuration.bootstrap_records = default_bootstrap_records;
        configuration.bootstrap_record_count =
            RUNTIME_MEMORY_POLICY_BOOTSTRAP_RECORD_COUNT;
    }
    return configuration;
}

RuntimeMemoryPolicyConfiguration
runtime_memory_policy_recovered_configuration_snapshot(void) {
    return memory_policy_configuration();
}

void runtime_memory_policy_recovered_configure(
    const RuntimeMemoryPolicyConfiguration *configuration
) {
    if (configuration == 0 ||
        (configuration->bootstrap_record_count != 0 &&
         configuration->bootstrap_records == 0) ||
        (configuration->protected_range_count != 0 &&
         configuration->protected_ranges == 0)) {
        memory_policy_panic();
    }
    policy_configuration = *configuration;
    bootstrap_record_count = configuration->bootstrap_record_count;
}

void runtime_memory_policy_recovered_reset(void) {
    policy_configuration = (RuntimeMemoryPolicyConfiguration){0};
    bootstrap_record_count = RUNTIME_MEMORY_POLICY_BOOTSTRAP_RECORD_COUNT;
    bootstrap_primary_lower = 0;
    bootstrap_primary_upper = 0;
}

static RuntimeFirebloomSpan bitmap_span(const uint8_t *bitmap) {
    return (RuntimeFirebloomSpan){
        .current = (void *)bitmap,
        .lower_bound = (void *)bitmap,
        .upper_bound = (void *)(bitmap + RUNTIME_MEMORY_POLICY_BITMAP_SIZE),
        .type = &runtime_memory_policy_qword_type,
    };
}

RuntimeFirebloomSpan runtime_memory_policy_set_a_bitmap_span(void) {
    return bitmap_span(set_a_bitmap);
}

RuntimeFirebloomSpan runtime_memory_policy_set_b_bitmap_span(void) {
    return bitmap_span(set_b_bitmap);
}

RuntimeFirebloomSpan runtime_memory_policy_set_c_bitmap_span(void) {
    return bitmap_span(set_c_bitmap);
}

bool runtime_memory_bitmap_test_clone(
    const uint64_t *bitmap,
    uintptr_t lower_bound,
    uintptr_t upper_bound,
    uint32_t bit_index
) {
    const uintptr_t word = (uintptr_t)bitmap +
        ((uintptr_t)(bit_index >> 6) * sizeof(uint64_t));
    if (word < lower_bound || word > upper_bound ||
        sizeof(uint64_t) > upper_bound - word) {
        runtime_memory_bitmap_test_bounds_panic();
    }
    return ((*(const uint64_t *)word >> (bit_index & 63u)) & 1u) != 0;
}

_Noreturn void runtime_memory_bitmap_test_bounds_panic(void) {
    memory_policy_panic();
}

static bool bitmap_contains(RuntimeFirebloomSpan span, uint32_t region_id) {
    return runtime_memory_bitmap_test_clone(
        span.current,
        (uintptr_t)span.lower_bound,
        (uintptr_t)span.upper_bound,
        region_id
    );
}

bool runtime_memory_policy_set_a_contains_region_id(uint32_t region_id) {
    return bitmap_contains(runtime_memory_policy_set_a_bitmap_span(), region_id);
}

bool runtime_memory_policy_set_b_contains_region_id(uint32_t region_id) {
    return bitmap_contains(runtime_memory_policy_set_b_bitmap_span(), region_id);
}

bool runtime_memory_policy_set_c_contains_region_id(uint32_t region_id) {
    return bitmap_contains(runtime_memory_policy_set_c_bitmap_span(), region_id);
}

bool runtime_memory_policy_set_c_contains_region_id_thunk(uint32_t region_id) {
    return runtime_memory_policy_set_c_contains_region_id(region_id);
}

bool runtime_memory_policy_outlined_test_sp_bitmap_bit_x19(uint32_t bit_index) {
    return runtime_memory_policy_set_c_contains_region_id(bit_index);
}

bool runtime_memory_policy_uses_extended_attribute_encoding(void) {
    return memory_policy_configuration().extended_attribute_encoding;
}

bool runtime_memory_policy_uses_alternate_region_encoding(void) {
    return memory_policy_configuration().alternate_region_encoding;
}

RuntimeMemoryPolicyDecodedRegion
runtime_memory_policy_outlined_prepare_decode_region_outputs(void) {
    return (RuntimeMemoryPolicyDecodedRegion){0};
}

static void require_output(
    const void *current,
    const void *lower,
    const void *upper,
    size_t size,
    const RuntimeFirebloomType *type
) {
    if (current == 0 || lower == 0 || upper == 0 || type == 0 ||
        (uintptr_t)current < (uintptr_t)lower ||
        (uintptr_t)current > (uintptr_t)upper ||
        size > (uintptr_t)upper - (uintptr_t)current) {
        memory_policy_panic();
    }
}

static RuntimeMemoryPolicyDecodedRegion decode_region(uint32_t region_id) {
    RuntimeMemoryPolicyDecodedRegion decoded = {0};
    if (region_id > 23 || region_id == 1 || region_id == 4 || region_id == 13) {
        memory_policy_panic();
    }
    if (region_id == 0) {
        decoded.translation_regime = 3;
    } else if (region_id == 7 || region_id == 10) {
        decoded.translation_regime = 2;
    } else if (region_id == 2 || region_id == 5 || region_id == 8 ||
               region_id == 11 || region_id == 14 || region_id == 16 ||
               region_id == 20 || region_id == 22) {
        decoded.translation_regime = 1;
    }
    if (region_id == 2 || region_id == 3) {
        decoded.memory_attribute = 4;
    } else if (region_id == 5 || region_id == 6) {
        decoded.memory_attribute = 2;
    } else if (region_id >= 14 && region_id <= 17) {
        decoded.memory_attribute = 6;
    } else if (region_id == 18 || region_id == 19) {
        decoded.memory_attribute =
            runtime_memory_policy_uses_extended_attribute_encoding() ? 1 : 0;
    } else if (region_id == 20 || region_id == 21) {
        decoded.memory_attribute =
            runtime_memory_policy_uses_alternate_region_encoding() ? 5 : 4;
    } else if (region_id == 22 || region_id == 23) {
        decoded.memory_attribute =
            runtime_memory_policy_uses_alternate_region_encoding() ? 3 : 2;
    }
    decoded.variant = region_id <= 19 &&
        ((UINT32_C(0x000b1c00) >> region_id) & 1u) != 0;
    return decoded;
}

void runtime_memory_policy_decode_region_id(
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
) {
    RuntimeMemoryPolicyDecodedRegion decoded = decode_region(region_id);
    require_output(
        translation_regime_current,
        translation_regime_lower_bound,
        translation_regime_upper_bound,
        sizeof(*translation_regime_current),
        translation_regime_type
    );
    require_output(
        memory_attribute_current,
        memory_attribute_lower_bound,
        memory_attribute_upper_bound,
        sizeof(*memory_attribute_current),
        memory_attribute_type
    );
    *translation_regime_current = decoded.translation_regime;
    *memory_attribute_current = decoded.memory_attribute;
    if (decoded.variant && variant_current == 0) {
        memory_policy_panic();
    }
    if (variant_current != 0) {
        require_output(
            variant_current,
            variant_lower_bound,
            variant_upper_bound,
            sizeof(*variant_current),
            variant_type
        );
        *variant_current = decoded.variant;
    }
}

uint32_t runtime_memory_policy_encode_region_id(
    uint32_t regime,
    uint32_t attribute,
    bool variant
) {
    if (attribute > 6) {
        memory_policy_panic();
    }
    if (attribute == 0) {
        if (regime > 2) memory_policy_panic();
        const uint32_t base[] = {9, 8, 7};
        return base[regime] + (variant ? 3u : 0u);
    }
    if (attribute == 1) {
        if (regime != 0) memory_policy_panic();
        if (!runtime_memory_policy_uses_extended_attribute_encoding()) {
            return 9 + (variant ? 3u : 0u);
        }
        return variant ? 19 : 18;
    }
    if (attribute == 2 || attribute == 3) {
        if (variant || regime > 1) memory_policy_panic();
        if (attribute == 3 &&
            runtime_memory_policy_uses_alternate_region_encoding()) {
            return regime == 0 ? 23 : 22;
        }
        return regime == 0 ? 6 : 5;
    }
    if (attribute == 4 || attribute == 5) {
        if (variant || regime > 1) memory_policy_panic();
        if (attribute == 5 &&
            runtime_memory_policy_uses_alternate_region_encoding()) {
            return regime == 0 ? 21 : 20;
        }
        return regime == 0 ? 3 : 2;
    }
    if (regime > 1) memory_policy_panic();
    return regime == 0 ? (variant ? 17 : 15) : (variant ? 16 : 14);
}

static uint16_t invoke_map(
    RuntimeMemoryPolicyMapRequest request,
    uint64_t *mapped
) {
    RuntimeMemoryPolicyConfiguration configuration =
        memory_policy_configuration();
    if (configuration.map != 0) {
        return configuration.map(&request, mapped, configuration.context);
    }
#if defined(__aarch64__) && defined(__ELF__)
    register uint64_t x0 __asm("x0") = request.physical_base;
    register uint64_t x1 __asm("x1") = request.byte_size;
    register uint64_t x2 __asm("x2") = request.translation_regime;
    register uint64_t x3 __asm("x3") = request.memory_attribute;
    register uint64_t x4 __asm("x4") = request.mapping_flags;
    register uint64_t x5 __asm("x5") = request.reserved_zero;
    __asm__ volatile(
        "svc #0x30"
        : "+r"(x0),
        "+r"(x1)
        : "r"(x2),
        "r"(x3),
        "r"(x4),
        "r"(x5)
        : "memory",
        "cc"
    );
    *mapped = x1;
    return (uint16_t)x0;
#else
    memory_policy_panic();
#endif
}

uint16_t runtime_memory_policy_map_physical_range_svc(
    uint64_t physical_base,
    uint64_t byte_size,
    uint32_t translation_regime,
    uint32_t memory_attribute,
    uint8_t mapping_flags,
    uint16_t reserved_zero,
    uint64_t *mapped_current,
    const uint64_t *mapped_lower,
    const uint64_t *mapped_upper
) {
    uint64_t mapped = 0;
    uint16_t capability = invoke_map((RuntimeMemoryPolicyMapRequest){
        physical_base, byte_size, translation_regime, memory_attribute,
        mapping_flags, reserved_zero,
    }, &mapped);
    if (mapped_current != 0) {
        require_output(
            mapped_current,
            mapped_lower,
            mapped_upper,
            sizeof(*mapped_current),
            &runtime_memory_policy_qword_type
        );
        *mapped_current = mapped;
    }
    return capability;
}

#define MAP_WRAPPER(name, regime, attribute, flags) \
uint16_t name(uint64_t base, uint64_t size) { \
    return runtime_memory_policy_map_physical_range_svc( \
        base, size, regime, attribute, flags, 0, 0, 0, 0); \
}

MAP_WRAPPER(
    runtime_memory_policy_map_regime1_attribute0_flags1_no_output,
    1,
    0,
    1
)
MAP_WRAPPER(
    runtime_memory_policy_map_regime0_attribute0_flags3_no_output,
    0,
    0,
    3
)
MAP_WRAPPER(
    runtime_memory_policy_map_regime0_attribute6_flags3_no_output,
    0,
    6,
    3
)
MAP_WRAPPER(
    runtime_memory_policy_map_regime1_attribute6_flags1_no_output,
    1,
    6,
    1
)
MAP_WRAPPER(
    runtime_memory_policy_map_regime1_attribute2_flags1_no_output,
    1,
    2,
    1
)

uint16_t runtime_memory_policy_map_physical_range_flags3_no_output(
    uint64_t base,
    uint64_t size,
    uint32_t regime,
    uint32_t attribute
) {
    return runtime_memory_policy_map_physical_range_svc(
        base,
        size,
        regime,
        attribute,
        3,
        0,
        0,
        0,
        0
    );
}

RuntimeMemoryPolicyMapRequest
runtime_memory_policy_outlined_prepare_regime0_no_output(void) {
    return (RuntimeMemoryPolicyMapRequest){
        .translation_regime = 0,
        .mapping_flags = 3,
    };
}

void runtime_memory_policy_unmap_range_svc(uint64_t base, uint64_t size) {
    RuntimeMemoryPolicyConfiguration configuration =
        memory_policy_configuration();
    if (configuration.unmap != 0) {
        configuration.unmap(base, size, configuration.context);
        return;
    }
#if defined(__aarch64__) && defined(__ELF__)
    register uint64_t x0 __asm("x0") = base;
    register uint64_t x1 __asm("x1") = size;
    __asm__ volatile("svc #0x31" : "+r"(x0), "+r"(x1) : : "memory", "cc");
#else
    memory_policy_panic();
#endif
}

void runtime_memory_policy_apply_region_id(
    uint64_t base,
    uint64_t size,
    uint32_t region_id
) {
    switch (region_id) {
        case 5: (void)runtime_memory_policy_map_regime1_attribute2_flags1_no_output(base, size); return;
        case 8: (void)runtime_memory_policy_map_regime1_attribute0_flags1_no_output(base, size); return;
        case 9: (void)runtime_memory_policy_map_regime0_attribute0_flags3_no_output(base, size); return;
        case 14: (void)runtime_memory_policy_map_regime1_attribute6_flags1_no_output(base, size); return;
        case 15: (void)runtime_memory_policy_map_regime0_attribute6_flags3_no_output(base, size); return;
        case 18: (void)runtime_memory_policy_map_physical_range_flags3_no_output(base, size, 0, 1); return;
        default: memory_policy_panic();
    }
}

void runtime_memory_policy_unmap_range_and_apply_region_id(
    uint64_t base,
    uint64_t size,
    uint32_t region_id
) {
    runtime_memory_policy_unmap_range_svc(base, size);
    if (region_id != 0) runtime_memory_policy_apply_region_id(base, size, region_id);
}

RuntimeFirebloomSpan runtime_bootstrap_primary_lower_slot(void) {
    return (RuntimeFirebloomSpan){
        &bootstrap_primary_lower, &bootstrap_primary_lower,
        &bootstrap_primary_lower + 1, &runtime_memory_policy_qword_type,
    };
}

RuntimeFirebloomSpan runtime_bootstrap_primary_upper_slot(void) {
    return (RuntimeFirebloomSpan){
        &bootstrap_primary_upper, &bootstrap_primary_upper,
        &bootstrap_primary_upper + 1, &runtime_memory_policy_qword_type,
    };
}
