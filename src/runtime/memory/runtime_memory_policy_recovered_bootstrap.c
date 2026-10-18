#include "avp/runtime/memory/runtime_memory_policy_recovered.h"

#include "avp/runtime/memory/runtime_memory.h"


static RuntimeMemoryPolicyConfiguration bootstrap_configuration;

/* The core configuration symbol is intentionally mirrored through this hook. */
extern RuntimeMemoryPolicyConfiguration
runtime_memory_policy_recovered_configuration_snapshot(void);

static _Noreturn void bootstrap_policy_panic(void) {
    runtime_firebloom_panic();
}

static RuntimeMemoryPolicyConfiguration configuration(void) {
    bootstrap_configuration =
        runtime_memory_policy_recovered_configuration_snapshot();
    return bootstrap_configuration;
}

RuntimeFirebloomSpan runtime_bootstrap_policy_records_span(void) {
    RuntimeMemoryPolicyConfiguration state = configuration();
    const RuntimeBootstrapPolicyRecord *records = state.bootstrap_records;
    return (RuntimeFirebloomSpan){
        .current = (void *)records,
        .lower_bound = (void *)records,
        .upper_bound = (void *)(records + state.bootstrap_record_count),
        .type = &runtime_memory_policy_record_type,
    };
}

RuntimeFirebloomSpan runtime_bootstrap_policy_record_count_span(void) {
    static uint16_t count;
    count = configuration().bootstrap_record_count;
    return (RuntimeFirebloomSpan){
        .current = &count,
        .lower_bound = &count,
        .upper_bound = &count + 1,
        .type = &runtime_memory_policy_byte_type,
    };
}

RuntimeFirebloomSpan runtime_bootstrap_policy_record_lookup(
    uint32_t policy_tag
) {
    RuntimeFirebloomSpan records = runtime_bootstrap_policy_records_span();
    RuntimeMemoryPolicyConfiguration state = configuration();
    const RuntimeBootstrapPolicyRecord *record = records.current;
    for (uint16_t index = 0; index < state.bootstrap_record_count; ++index) {
        if (record[index].tag == policy_tag) {
            return (RuntimeFirebloomSpan){
                .current = (void *)&record[index],
                .lower_bound = records.lower_bound,
                .upper_bound = records.upper_bound,
                .type = records.type,
            };
        }
    }
    bootstrap_policy_panic();
}

const void *
runtime_bootstrap_policy_outlined_load_upper_compare_lower_to_candidate(
    const void *candidate,
    const void *lower_bound,
    const void *upper_bound
) {
    if (candidate == 0 || lower_bound == 0 || upper_bound == 0 ||
        (uintptr_t)candidate < (uintptr_t)lower_bound ||
        (uintptr_t)candidate >= (uintptr_t)upper_bound) {
        bootstrap_policy_panic();
    }
    return upper_bound;
}

RuntimeBootstrapPolicyAddressPair
runtime_bootstrap_policy_outlined_add_record_offset(
    const void *lower_bound,
    size_t byte_offset
) {
    if (lower_bound == 0 || byte_offset > UINTPTR_MAX - (uintptr_t)lower_bound) {
        bootstrap_policy_panic();
    }
    return (RuntimeBootstrapPolicyAddressPair){
        .candidate = (const uint8_t *)lower_bound + byte_offset,
        .lower_bound = lower_bound,
    };
}
