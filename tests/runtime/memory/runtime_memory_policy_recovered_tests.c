#include "avp/runtime/memory/runtime_memory_policy_recovered.h"

#include <assert.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>


static jmp_buf panic_environment;
static bool panic_expected;
static RuntimeMemoryPolicyMapRequest last_map;
static uint64_t last_unmap_base;
static uint64_t last_unmap_size;
static unsigned map_count;
static unsigned unmap_count;
static RuntimePhysicalMemoryRegion regions[32];
static uint8_t policy_classes[32];
static RuntimeBootstrapPolicyRecord records[4];
static RuntimeMemoryPolicyRange protected_ranges[2];

_Noreturn void runtime_firebloom_panic(void) {
    assert(panic_expected);
    longjmp(panic_environment, 1);
}

static uint16_t map_range(
    const RuntimeMemoryPolicyMapRequest *request,
    uint64_t *mapped,
    void *context
) {
    assert(context == regions);
    last_map = *request;
    *mapped = request->physical_base + UINT64_C(0x100000000);
    ++map_count;
    return 7;
}

static void unmap_range(uint64_t base, uint64_t size, void *context) {
    assert(context == regions);
    last_unmap_base = base;
    last_unmap_size = size;
    ++unmap_count;
}

static RuntimeFirebloomSpan lookup_region(uint16_t region_id, void *context) {
    assert(context == regions);
    assert(region_id < 32);
    return (RuntimeFirebloomSpan){
        .current = &regions[region_id],
        .lower_bound = regions,
        .upper_bound = regions + 32,
        .type = &runtime_memory_policy_record_type,
    };
}

static void configure(bool alternate, bool extended) {
    memset(regions, 0, sizeof(regions));
    memset(policy_classes, 0, sizeof(policy_classes));
    memset(records, 0, sizeof(records));
    records[0].tag = UINT32_C(0x746f6f72);
    records[1].tag = UINT32_C(0x00007366);
    records[2].tag = UINT32_C(0x726f7473);
    records[3].tag = UINT32_C(0x00627375);
    protected_ranges[0] = (RuntimeMemoryPolicyRange){0x5000, 0x6000};
    protected_ranges[1] = (RuntimeMemoryPolicyRange){0x9000, 0xa000};
    regions[16] = (RuntimePhysicalMemoryRegion){
        .physical_base = 0x100000,
        .active_alias = 0x100000,
        .size = 0x10000,
    };
    policy_classes[16] = 7;
    map_count = 0;
    unmap_count = 0;
    last_map = (RuntimeMemoryPolicyMapRequest){0};
    runtime_memory_policy_recovered_configure(
        &(RuntimeMemoryPolicyConfiguration){
            .map = map_range,
            .unmap = unmap_range,
            .lookup_region = lookup_region,
            .region_policy_classes = policy_classes,
            .region_policy_class_count = 32,
            .bootstrap_records = records,
            .bootstrap_record_count = 4,
            .physical_base = 0x100000,
            .slide = alternate ? 0x4000 : 0,
            .platform_range = {0x80000000, 0x80010000},
            .private_range = {0x70034000, 0x7003dd70},
            .dynamic_private_range = {0x71000000, 0x71010000},
            .protected_ranges = protected_ranges,
            .protected_range_count = 2,
            .extended_attribute_encoding = extended,
            .alternate_region_encoding = alternate,
            .context = regions,
        }
    );
}

static void expect_panic(void (*operation)(void)) {
    panic_expected = true;
    if (setjmp(panic_environment) == 0) {
        operation();
        assert(false);
    }
    panic_expected = false;
}

static void invalid_bitmap(void) {
    uint64_t word = 0;
    (void)runtime_memory_bitmap_test_clone(
        &word, (uintptr_t)&word, (uintptr_t)&word + sizeof(word), 64);
}

static void invalid_decode(void) {
    uint32_t regime = 0;
    uint32_t attribute = 0;
    runtime_memory_policy_decode_region_id(
        1, &regime, &regime, &regime + 1,
        &runtime_memory_policy_qword_type,
        &attribute, &attribute, &attribute + 1,
        &runtime_memory_policy_qword_type,
        0, 0, 0, 0);
}

static void protected_overlap(void) {
    (void)runtime_memory_range_classify_simple(0x5800, 0x100);
}

static void test_bitmap_and_spans(void) {
    configure(false, false);
    RuntimeFirebloomSpan a = runtime_memory_policy_set_a_bitmap_span();
    RuntimeFirebloomSpan b = runtime_memory_policy_set_b_bitmap_span();
    RuntimeFirebloomSpan c = runtime_memory_policy_set_c_bitmap_span();
    assert((uintptr_t)a.upper_bound - (uintptr_t)a.current == 80);
    assert((uintptr_t)b.upper_bound - (uintptr_t)b.current == 80);
    assert((uintptr_t)c.upper_bound - (uintptr_t)c.current == 80);
    assert(runtime_memory_policy_set_a_contains_region_id(0));
    assert(runtime_memory_policy_set_b_contains_region_id(16));
    assert(runtime_memory_policy_set_c_contains_region_id(23));
    assert(runtime_memory_policy_set_c_contains_region_id_thunk(23));
    assert(runtime_memory_policy_outlined_test_sp_bitmap_bit_x19(23));
    expect_panic(invalid_bitmap);
}

static void test_region_codec(void) {
    configure(false, false);
    const uint32_t valid[] = {
        0, 2, 3, 5, 6, 7, 8, 9, 10, 11, 12,
        14, 15, 16, 17,
    };
    for (size_t index = 1; index < sizeof(valid) / sizeof(valid[0]); ++index) {
        uint32_t regime = 0;
        uint32_t attribute = 0;
        bool variant = false;
        runtime_memory_policy_decode_region_id(
            valid[index], &regime, &regime, &regime + 1,
            &runtime_memory_policy_qword_type,
            &attribute, &attribute, &attribute + 1,
            &runtime_memory_policy_qword_type,
            &variant, &variant, &variant + 1,
            &runtime_memory_policy_byte_type);
        assert(runtime_memory_policy_encode_region_id(
            regime, attribute, variant) == valid[index]);
    }
    RuntimeMemoryPolicyDecodedRegion empty =
        runtime_memory_policy_outlined_prepare_decode_region_outputs();
    assert(empty.translation_regime == 0 && empty.memory_attribute == 0 &&
        !empty.variant);
    expect_panic(invalid_decode);

    configure(true, true);
    assert(runtime_memory_policy_uses_extended_attribute_encoding());
    assert(runtime_memory_policy_uses_alternate_region_encoding());
    assert(runtime_memory_policy_encode_region_id(0, 1, false) == 18);
    assert(runtime_memory_policy_encode_region_id(0, 1, true) == 19);
    assert(runtime_memory_policy_encode_region_id(1, 5, false) == 20);
    assert(runtime_memory_policy_encode_region_id(0, 5, false) == 21);
    assert(runtime_memory_policy_encode_region_id(1, 3, false) == 22);
    assert(runtime_memory_policy_encode_region_id(0, 3, false) == 23);
}

static void test_mapping_transcript(void) {
    configure(false, false);
    uint64_t mapped = 0;
    assert(runtime_memory_policy_map_physical_range_svc(
        0x200000, 0x4000, 1, 6, 1, 0,
        &mapped, &mapped, &mapped + 1) == 7);
    assert(mapped == UINT64_C(0x100200000));
    assert(last_map.translation_regime == 1 &&
        last_map.memory_attribute == 6 && last_map.mapping_flags == 1);
    (void)runtime_memory_policy_map_regime1_attribute0_flags1_no_output(1, 2);
    (void)runtime_memory_policy_map_regime0_attribute0_flags3_no_output(3, 4);
    (void)runtime_memory_policy_map_regime0_attribute6_flags3_no_output(5, 6);
    (void)runtime_memory_policy_map_regime1_attribute6_flags1_no_output(7, 8);
    (void)runtime_memory_policy_map_regime1_attribute2_flags1_no_output(9, 10);
    (void)runtime_memory_policy_map_physical_range_flags3_no_output(11, 12, 0, 1);
    assert(map_count == 7);
    RuntimeMemoryPolicyMapRequest prepared =
        runtime_memory_policy_outlined_prepare_regime0_no_output();
    assert(prepared.translation_regime == 0 && prepared.mapping_flags == 3);
    runtime_memory_policy_unmap_range_svc(0x3000, 0x4000);
    assert(last_unmap_base == 0x3000 && last_unmap_size == 0x4000);
    runtime_memory_policy_apply_region_id(0x5000, 0x4000, 14);
    runtime_memory_policy_unmap_range_and_apply_region_id(0x6000, 0x4000, 9);
    assert(unmap_count == 2 && map_count == 9);
}

static void test_address_and_range_policy(void) {
    configure(false, false);
    assert(runtime_address_profile_load_physical_base_to_x8() == 0x100000);
    assert(runtime_address_profile_slide() == 0);
    assert(!runtime_address_profile_is_alternate());
    assert(runtime_address_profile_translate(0x100000) ==
        UINT64_C(0xfffffd0000000000));
    assert(runtime_address_profile_contains_high_window(
        UINT64_C(0xfffffd0000000000)));
    uintptr_t upper = 0;
    assert(runtime_memory_range_classify(
        0x10000, 0x100, &upper, (uintptr_t)&upper,
        (uintptr_t)(&upper + 1)) == RUNTIME_MEMORY_RANGE_GENERAL);
    assert(upper == 0x10100);
    assert(runtime_memory_range_classify_simple(0x70035000, 0x100) ==
        RUNTIME_MEMORY_RANGE_PRIVATE);
    assert(runtime_memory_range_classify_simple_clone(0x80001000, 0x100) ==
        RUNTIME_MEMORY_RANGE_PLATFORM);
    expect_panic(protected_overlap);

    configure(true, false);
    assert(runtime_address_profile_is_alternate());
    assert(runtime_address_profile_translate(0x100000) ==
        UINT64_C(0x31000000000));
    assert(runtime_address_profile_contains_high_window(
        UINT64_C(0x31000000000)));
}

static void test_bootstrap_and_physical_policy(void) {
    configure(false, false);
    RuntimeFirebloomSpan span = runtime_bootstrap_policy_records_span();
    assert((uintptr_t)span.upper_bound - (uintptr_t)span.current ==
        4 * sizeof(RuntimeBootstrapPolicyRecord));
    RuntimeFirebloomSpan count = runtime_bootstrap_policy_record_count_span();
    assert(*(uint16_t *)count.current == 4);
    RuntimeFirebloomSpan record =
        runtime_bootstrap_policy_record_lookup(UINT32_C(0x726f7473));
    assert(((RuntimeBootstrapPolicyRecord *)record.current)->tag ==
        UINT32_C(0x726f7473));
    RuntimeBootstrapPolicyAddressPair pair =
        runtime_bootstrap_policy_outlined_add_record_offset(
            span.lower_bound, sizeof(RuntimeBootstrapPolicyRecord));
    assert(pair.candidate == &records[1]);
    assert(runtime_bootstrap_policy_outlined_load_upper_compare_lower_to_candidate(
        pair.candidate, span.lower_bound, span.upper_bound) == span.upper_bound);
    RuntimeFirebloomSpan lower = runtime_bootstrap_primary_lower_slot();
    RuntimeFirebloomSpan upper = runtime_bootstrap_primary_upper_slot();
    assert(lower.current != upper.current);
    assert(runtime_physical_memory_range_is_reserved(0x101000, 0x1000, 16));
    runtime_memory_policy_require_physical_range(0x101000, 0x1000, 7, true);
}

int main(void) {
    test_bitmap_and_spans();
    test_region_codec();
    test_mapping_transcript();
    test_address_and_range_policy();
    test_bootstrap_and_physical_policy();
    runtime_memory_policy_recovered_reset();
    puts("runtime memory policy recovered tests passed");
}
