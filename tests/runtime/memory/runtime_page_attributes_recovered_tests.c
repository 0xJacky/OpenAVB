#include "avp/runtime/memory/runtime_bounded_bitmap.h"
#include "avp/runtime/memory/runtime_page_attributes_recovered.h"

#include <assert.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

enum {
    TEST_PAGE = RUNTIME_PAGE_ATTRIBUTE_PAGE_SIZE,
};

static void expect_trap(void (*operation)(void)) {
    const pid_t child = fork();
    assert(child >= 0);
    if (child == 0) {
        operation();
        _exit(0);
    }
    int status = 0;
    assert(waitpid(child, &status, 0) == child);
    assert(WIFSIGNALED(status));
    assert(WTERMSIG(status) == SIGILL || WTERMSIG(status) == SIGTRAP);
}

static void trigger_range_conversion_failure(void) {
    (void)runtime_memory_range_page_attribute_mask(
        0, (RuntimePageAttributeLookupResult){0},
        (RuntimePageAttributeMemoryRange){
            .base = UINTPTR_MAX - 4,
            .size = 8,
        });
}

static void test_registry_geometry_and_lookup(void) {
    RuntimePageAttributeRegistry registry = {0};
    RuntimePageAttributeRecord records[2] = {0};
    const RuntimePageAttributeMemoryRange regions[2] = {
        {.base = 0x100000, .size = 8 * TEST_PAGE},
        {.base = 0x140000, .size = 4 * TEST_PAGE},
    };
    uint8_t bitmap_storage[32] = {0xff};

    assert(runtime_page_span_attribute_mask(
               &registry, (RuntimePageAttributeLookupResult){0},
               (RuntimePageAttributePageSpan){.begin = 64, .end = 65}) ==
           RUNTIME_PAGE_ATTRIBUTE_UNINITIALIZED);
    runtime_page_attribute_registry_initialize(&registry, records, 2, regions,
                                               2, bitmap_storage,
                                               sizeof(bitmap_storage));
    assert(runtime_page_attribute_registry_test_initialized(&registry));
    assert(registry.record_count == 2);
    assert(records[0].first_page == 64);
    assert(records[0].page_limit == 72);
    assert(records[1].first_page == 80);
    assert(records[1].page_limit == 84);

    const RuntimePageAttributeRecordSpan lower =
        runtime_page_attribute_registry_lower_bound_slot_span(&registry);
    const RuntimePageAttributeRecordSpan upper =
        runtime_page_attribute_registry_upper_bound_slot_span(&registry);
    assert(lower.current == &records[0]);
    assert(lower.lower_bound == &records[0]);
    assert(lower.upper_bound == &records[2]);
    assert(upper.current == &records[2]);

    const RuntimePageAttributeByteView state =
        runtime_page_attribute_registry_state_span(&registry);
    assert(state.current == &registry.initialized);
    assert((uint8_t *)state.upper_bound - (uint8_t *)state.lower_bound == 1);

    RuntimePageAttributeLookupResult lookup =
        runtime_page_attribute_registry_lookup(
            &registry, (RuntimePageAttributePageSpan){.begin = 65, .end = 68});
    assert(lookup.record == &records[0]);
    assert(lookup.records_begin == &records[0]);
    assert(lookup.records_end == &records[2]);
    assert(runtime_page_attribute_restore_lookup_result(lookup).record ==
           &records[0]);
    assert(
        runtime_page_attribute_registry_lookup(
            &registry, (RuntimePageAttributePageSpan){.begin = 71, .end = 81})
            .record == 0);

    RuntimePageAttributeRecord direct_record = {0};
    uint8_t direct_storage[16] = {0xff};
    uint8_t *cursor = runtime_page_attribute_record_initialize(
        &direct_record,
        (RuntimePageAttributeMemoryRange){
            .base = 0x200000,
            .size = 2 * TEST_PAGE,
        },
        direct_storage, direct_storage + sizeof(direct_storage));
    assert(cursor == direct_storage + sizeof(direct_storage));
    cursor = runtime_page_attribute_outlined_initialize_registry_record(
        &direct_record,
        (RuntimePageAttributeMemoryRange){
            .base = 0x200000,
            .size = 2 * TEST_PAGE,
        },
        direct_storage, direct_storage + sizeof(direct_storage));
    assert(cursor == direct_storage + sizeof(direct_storage));
}

static void test_policy_and_mutation(void) {
    RuntimePageAttributeRegistry registry = {0};
    RuntimePageAttributeRecord records[2] = {0};
    const RuntimePageAttributeMemoryRange regions[2] = {
        {.base = 0x100000, .size = 8 * TEST_PAGE},
        {.base = 0x140000, .size = 4 * TEST_PAGE},
    };
    uint8_t bitmap_storage[32] = {0};
    runtime_page_attribute_registry_initialize(&registry, records, 2, regions,
                                               2, bitmap_storage,
                                               sizeof(bitmap_storage));
    runtime_page_attribute_policy_initialize(&registry, 0x100000, 0x180000,
                                             0x120000, 0x140000);
    assert(runtime_page_attribute_policy_active_flag_to_w8(&registry) == 1);
    const RuntimePageAttributeByteView active =
        runtime_page_attribute_policy_active_flag_bounded_view(&registry);
    assert(active.current == &registry.policy_active);
    assert(runtime_page_attribute_address_allowed(&registry, 0x100000));
    assert(!runtime_page_attribute_address_allowed(&registry, 0x120000));
    assert(!runtime_page_attribute_address_allowed(&registry, 0x180000));

    const RuntimePageAttributePageSpan pages = {.begin = 65, .end = 68};
    RuntimePageAttributeLookupResult lookup =
        runtime_page_attribute_registry_lookup(&registry, pages);
    assert(runtime_page_attribute_flags_valid_for_span(
        &registry,
        RUNTIME_PAGE_ATTRIBUTE_PRIMARY_SET |
            RUNTIME_PAGE_ATTRIBUTE_SECONDARY_SET,
        pages));
    assert(!runtime_page_attribute_flags_valid_for_span(&registry,
                                                        UINT32_C(0x3), pages));
    RuntimePageAttributeLookupResult saved = {0};
    assert(runtime_page_attribute_save_lookup_result_and_validate_flags(
        &registry, lookup, pages,
        RUNTIME_PAGE_ATTRIBUTE_PRIMARY_SET |
            RUNTIME_PAGE_ATTRIBUTE_SECONDARY_SET,
        &saved));
    assert(saved.record == &records[0]);

    assert(runtime_page_span_attribute_claim(
        &registry, lookup, pages,
        RUNTIME_PAGE_ATTRIBUTE_PRIMARY_SET |
            RUNTIME_PAGE_ATTRIBUTE_SECONDARY_SET,
        0));
    assert(runtime_page_span_attribute_mask(&registry, lookup, pages) == 5);
    assert(runtime_page_span_attribute_mask(
               &registry, lookup,
               (RuntimePageAttributePageSpan){.begin = 64, .end = 72}) == 10);
    RuntimePageAttributeBitmapArguments secondary =
        runtime_page_attribute_load_secondary_bitmap_arguments(&records[0],
                                                               pages);
    assert(secondary.bit_capacity == 8);
    assert(secondary.begin == 1 && secondary.end == 4);
    assert(runtime_bitmap_classify_range(
               secondary.bitmap, secondary.bit_capacity, secondary.begin,
               secondary.end) == RUNTIME_BITMAP_RANGE_SET);

    assert(runtime_page_span_attribute_clear(
        &registry, lookup, pages,
        RUNTIME_PAGE_ATTRIBUTE_PRIMARY_SET |
            RUNTIME_PAGE_ATTRIBUTE_SECONDARY_SET,
        0));
    assert(runtime_page_span_attribute_mask(&registry, lookup, pages) == 0);

    assert(runtime_byte_bounds_attribute_claim(
        &registry, lookup, 0x104000, 0x108000,
        RUNTIME_PAGE_ATTRIBUTE_PRIMARY_SET, 0));
    assert(runtime_byte_bounds_attribute_clear(
        &registry, lookup, 0x104000, 0x108000,
        RUNTIME_PAGE_ATTRIBUTE_PRIMARY_SET, 0));
    assert(runtime_memory_range_attribute_claim(
        &registry, lookup,
        (RuntimePageAttributeMemoryRange){.base = 0x104001, .size = 17},
        RUNTIME_PAGE_ATTRIBUTE_SECONDARY_SET, 0));
    assert(
        runtime_memory_range_page_attribute_mask(
            &registry, lookup,
            (RuntimePageAttributeMemoryRange){.base = 0x104001, .size = 17}) ==
        RUNTIME_PAGE_ATTRIBUTE_SECONDARY_SET);
    assert(runtime_memory_range_attribute_clear(
        &registry, lookup,
        (RuntimePageAttributeMemoryRange){.base = 0x104001, .size = 17},
        RUNTIME_PAGE_ATTRIBUTE_SECONDARY_SET, 0));

    const RuntimePageAttributeMemoryRange second_region = {
        .base = 0x140000,
        .size = TEST_PAGE,
    };
    lookup = runtime_page_attribute_registry_lookup(
        &registry, (RuntimePageAttributePageSpan){.begin = 80, .end = 81});
    assert(runtime_memory_range_attribute_claim_flags_6_10000(&registry, lookup,
                                                              second_region));
    assert(runtime_memory_range_page_attribute_mask(&registry, lookup,
                                                    second_region) ==
           RUNTIME_PAGE_ATTRIBUTE_PRIMARY_SET);
    runtime_page_attribute_record_update_range(
        &records[1], (RuntimePageAttributePageSpan){.begin = 80, .end = 81},
        RUNTIME_PAGE_ATTRIBUTE_PRIMARY_SET, false);
}

static void test_outliners_and_rounding(void) {
    const RuntimePageAttributeClaimArguments claim =
        runtime_page_attribute_outlined_prepare_flags_6_10000();
    assert(claim.attribute_flags == 6);
    assert(claim.options == RUNTIME_PAGE_ATTRIBUTE_STATIC_CLAIM);

    const RuntimePageAttributeOptionalArguments first =
        runtime_page_attribute_zero_optional_arguments();
    const RuntimePageAttributeOptionalArguments second =
        runtime_page_attribute_zero_optional_arguments_clone();
    for (size_t index = 0; index < 4; ++index) {
        assert(first.values[index] == 0);
        assert(second.values[index] == 0);
    }
    assert(first.stack_value == 0 && second.stack_value == 0);

    const RuntimePageAttributeStaticRangeArguments arguments =
        runtime_page_attribute_outlined_prepare_static_range_arguments(
            (RuntimePageAttributeMemoryRange){.base = 0x100001, .size = 3}, 6,
            RUNTIME_PAGE_ATTRIBUTE_STATIC_CLAIM);
    assert(arguments.base == 0x100001);
    assert(arguments.size == 3);
    assert(arguments.attribute_flags == 6);
    assert(runtime_page_attribute_rounded_range_size(0x103fff, 2) ==
           2 * TEST_PAGE);
}

int main(void) {
    test_registry_geometry_and_lookup();
    test_policy_and_mutation();
    test_outliners_and_rounding();
    expect_trap(trigger_range_conversion_failure);
    puts("runtime_page_attributes_recovered_tests: ok");
    return 0;
}
