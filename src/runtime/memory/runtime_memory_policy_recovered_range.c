#include "avp/runtime/memory/runtime_memory_policy_recovered.h"

#include "avp/runtime/memory/runtime_memory.h"


extern RuntimeMemoryPolicyConfiguration
runtime_memory_policy_recovered_configuration_snapshot(void);

static _Noreturn void memory_range_panic(void) {
    runtime_firebloom_panic();
}

static bool range_is_valid(RuntimeMemoryPolicyRange range) {
    return range.lower < range.upper;
}

static bool range_contains(
    RuntimeMemoryPolicyRange range,
    uintptr_t lower,
    uintptr_t upper
) {
    return range_is_valid(range) && lower >= range.lower && upper <= range.upper;
}

static bool range_overlaps(
    RuntimeMemoryPolicyRange range,
    uintptr_t lower,
    uintptr_t upper
) {
    return range_is_valid(range) && lower < range.upper && range.lower < upper;
}

uintptr_t runtime_address_profile_load_physical_base_to_x8(void) {
    return runtime_memory_policy_recovered_configuration_snapshot().physical_base;
}

uintptr_t runtime_address_profile_slide(void) {
    return runtime_memory_policy_recovered_configuration_snapshot().slide;
}

bool runtime_address_profile_is_alternate(void) {
    return runtime_address_profile_slide() != 0;
}

uintptr_t runtime_address_profile_translate(uintptr_t address) {
    const uintptr_t base = runtime_address_profile_load_physical_base_to_x8();
    if (address < base) memory_range_panic();
    const uintptr_t offset = address - base;
    if (runtime_address_profile_is_alternate()) {
        if ((offset >> 37) >= 7) memory_range_panic();
        return offset + UINT64_C(0x31000000000);
    }
    if ((offset >> 40) >= 3) memory_range_panic();
    return offset - UINT64_C(0x30000000000);
}

bool runtime_address_profile_contains_high_window(uintptr_t address) {
    if (runtime_address_profile_is_alternate()) {
        const uintptr_t base = UINT64_C(0x31000000000);
        return address >= base && address - base < (UINT64_C(7) << 37);
    }
    return address >= UINT64_C(0xfffffd0000000000);
}

RuntimeMemoryRangeClass runtime_memory_range_classify(
    uintptr_t lower,
    size_t size,
    uintptr_t *upper_output,
    uintptr_t output_lower,
    uintptr_t output_upper
) {
    if (size > UINTPTR_MAX - lower) memory_range_panic();
    const uintptr_t upper = lower + size;
    if (upper_output != 0) {
        if ((uintptr_t)upper_output < output_lower ||
            (uintptr_t)upper_output > output_upper ||
            sizeof(*upper_output) > output_upper - (uintptr_t)upper_output) {
            memory_range_panic();
        }
        *upper_output = upper;
    }
    RuntimeMemoryPolicyConfiguration state =
        runtime_memory_policy_recovered_configuration_snapshot();
    RuntimeMemoryPolicyRange private_range = state.private_range;
    if (!range_is_valid(private_range)) {
        private_range = (RuntimeMemoryPolicyRange){
            .lower = UINT64_C(0x70034000),
            .upper = UINT64_C(0x7003dd70),
        };
    }
    if (range_contains(private_range, lower, upper) ||
        range_contains(state.dynamic_private_range, lower, upper)) {
        return RUNTIME_MEMORY_RANGE_PRIVATE;
    }
    if (range_overlaps(private_range, lower, upper) ||
        range_overlaps(state.dynamic_private_range, lower, upper)) {
        memory_range_panic();
    }
    if (range_contains(state.platform_range, lower, upper)) {
        return RUNTIME_MEMORY_RANGE_PLATFORM;
    }
    if (range_overlaps(state.platform_range, lower, upper)) {
        memory_range_panic();
    }
    for (size_t index = 0; index < state.protected_range_count; ++index) {
        if (range_overlaps(state.protected_ranges[index], lower, upper)) {
            memory_range_panic();
        }
    }
    return RUNTIME_MEMORY_RANGE_GENERAL;
}

RuntimeMemoryRangeClass runtime_memory_range_classify_simple(
    uintptr_t lower,
    size_t size
) {
    return runtime_memory_range_classify(lower, size, 0, 0, 0);
}

RuntimeMemoryRangeClass runtime_memory_range_classify_simple_clone(
    uintptr_t lower,
    size_t size
) {
    return runtime_memory_range_classify_simple(lower, size);
}

static bool physical_region_is_live(RuntimeFirebloomSpan span) {
    if (span.current == 0 || span.lower_bound == 0 || span.upper_bound == 0 ||
        (uintptr_t)span.current < (uintptr_t)span.lower_bound ||
        (uintptr_t)span.current >= (uintptr_t)span.upper_bound) {
        return false;
    }
    const RuntimePhysicalMemoryRegion *region = span.current;
    return region->physical_base != 0 && region->size != 0;
}

static RuntimeFirebloomSpan lookup_region(
    RuntimeMemoryPolicyConfiguration state,
    uint16_t region_id
) {
    if (state.lookup_region == 0) memory_range_panic();
    return state.lookup_region(region_id, state.context);
}

bool runtime_physical_memory_range_is_reserved(
    uintptr_t physical_base,
    size_t size,
    uint16_t region_id
) {
    if (region_id == 0 || size == 0 || size > UINTPTR_MAX - physical_base) {
        memory_range_panic();
    }
    RuntimeMemoryPolicyConfiguration state =
        runtime_memory_policy_recovered_configuration_snapshot();
    RuntimeFirebloomSpan span = lookup_region(state, region_id);
    if (!physical_region_is_live(span)) return false;
    const RuntimePhysicalMemoryRegion *region = span.current;
    const uintptr_t upper = physical_base + size;
    const uintptr_t region_upper = region->physical_base + region->size;
    if (physical_base < region->physical_base || upper > region_upper) {
        return true;
    }
    if (region_id >= state.region_policy_class_count ||
        state.region_policy_classes == 0) {
        return false;
    }
    return state.region_policy_classes[region_id] != 0 &&
        (runtime_memory_policy_set_a_contains_region_id(region_id) ||
         runtime_memory_policy_set_b_contains_region_id(region_id) ||
         runtime_memory_policy_set_c_contains_region_id(region_id));
}

void runtime_memory_policy_require_physical_range(
    uintptr_t lower,
    size_t size,
    uint8_t expected_region_class,
    bool strict_region_check
) {
    if (size == 0 || size > UINTPTR_MAX - lower) memory_range_panic();
    RuntimeMemoryPolicyConfiguration state =
        runtime_memory_policy_recovered_configuration_snapshot();
    uint16_t matching_id = 0;
    for (size_t region_id = 1;
         region_id < state.region_policy_class_count;
         ++region_id) {
        RuntimeFirebloomSpan span = lookup_region(state, (uint16_t)region_id);
        if (!physical_region_is_live(span)) continue;
        const RuntimePhysicalMemoryRegion *region = span.current;
        if (size <= region->size && lower >= region->physical_base &&
            lower - region->physical_base <= region->size - size) {
            matching_id = (uint16_t)region_id;
            break;
        }
    }
    if (matching_id == 0) memory_range_panic();
    const uint8_t actual_class = state.region_policy_classes[matching_id];
    if (actual_class != expected_region_class &&
        (strict_region_check || actual_class != 0)) {
        memory_range_panic();
    }
    if (runtime_memory_range_classify_simple_clone(lower, size) !=
        RUNTIME_MEMORY_RANGE_GENERAL) {
        memory_range_panic();
    }
}
