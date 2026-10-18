#include "avp/runtime/memory/runtime_bootstrap_memory_recovered.h"

#include <assert.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>


enum {
    TEST_REGION_COUNT = RUNTIME_PHYSICAL_REGION_ID_MAX + 1,
    TEST_MAPPING_CAPACITY = 16,
};

static jmp_buf panic_environment;
static bool panic_expected;
static RuntimePhysicalMemoryRegion regions[TEST_REGION_COUNT];
static RuntimeBootstrapMemoryMappingRequest mappings[TEST_MAPPING_CAPACITY];
static size_t mapping_count;
static size_t claim_count;
static size_t capability_count;
static size_t domain_publish_count;
static size_t clear_count;
static size_t initialize_count;
static RuntimeMemorySpan last_claim;
static RuntimeMemorySpan cleared_range;
static RuntimeBootstrapMemoryCapabilityMetadata published_metadata;
static RuntimeBootstrapMemoryCapabilityMetadata capability_metadata[2];
static bool boot_owner;
static uint8_t complete_flag;
static uint8_t active_flag;
static uint8_t sealed_flag;
static uint8_t storage_alias_flag;
static uintptr_t shared_cursor;
static RuntimePageAttributeRegistry attribute_registry;
static RuntimeDomain domain;
static RuntimeBootstrapMemorySharedMapping shared_mappings[3];
static RuntimeBootstrapMemoryRegionRecord capability_records[2];
static RuntimeBootstrapMemoryDescriptor descriptor;

_Noreturn void runtime_firebloom_panic(void) {
    assert(panic_expected);
    longjmp(panic_environment, 1);
}

static void clear_bytes(void *storage, size_t size) {
    uint8_t *bytes = storage;
    for (size_t index = 0; index < size; ++index) {
        bytes[index] = 0;
    }
}

static RuntimeFirebloomSpan lookup_region(uint16_t region_id, void *context) {
    assert(context == regions);
    assert(region_id < TEST_REGION_COUNT);
    return (RuntimeFirebloomSpan){
        .current = &regions[region_id],
        .lower_bound = regions,
        .upper_bound = regions + TEST_REGION_COUNT,
    };
}

static bool map_range(
    const RuntimeBootstrapMemoryMappingRequest *request,
    void *context
) {
    assert(context == regions);
    assert(mapping_count < TEST_MAPPING_CAPACITY);
    mappings[mapping_count++] = *request;
    return true;
}

static bool range_is_reserved(
    uintptr_t physical_base,
    size_t size,
    uint16_t region_id,
    void *context
) {
    assert(context == regions);
    return physical_base != 0 && size != 0 && region_id != 0;
}

static bool shared_region_is_forbidden(uint16_t region_id, void *context) {
    assert(context == regions);
    return region_id == 31;
}

static uint32_t attribute_mask(RuntimeMemorySpan range, void *context) {
    assert(context == regions);
    return range.lower_bound == UINT64_C(0x300000) ? 4 : 0;
}

static bool claim_attributes(
    RuntimeMemorySpan range,
    uint32_t attribute_flags,
    uint32_t options,
    void *context
) {
    assert(context == regions);
    assert(attribute_flags == RUNTIME_PAGE_ATTRIBUTE_PRIMARY_SET);
    assert(options == 1);
    last_claim = range;
    ++claim_count;
    return true;
}

static uint16_t publish_capability(
    uint8_t type,
    uint16_t region_id,
    uint8_t rights,
    const RuntimeBootstrapMemoryCapabilityMetadata *metadata,
    void *context
) {
    assert(context == regions);
    assert(type == RUNTIME_BOOTSTRAP_MEMORY_CAPABILITY_TYPE);
    assert(rights == RUNTIME_BOOTSTRAP_MEMORY_CAPABILITY_RIGHTS);
    assert(metadata->region_id == region_id);
    published_metadata = *metadata;
    ++capability_count;
    return (uint16_t)(0x120 + capability_count);
}

static void publish_domain(RuntimeDomainView view, void *context) {
    assert(context == regions);
    assert(view.current == &domain);
    ++domain_publish_count;
}

static void clear_range(
    uintptr_t lower_bound,
    uintptr_t upper_bound,
    void *context
) {
    assert(context == regions);
    cleared_range = (RuntimeMemorySpan){lower_bound, upper_bound};
    ++clear_count;
}

static void initialize_base(void *context) {
    assert(context == regions);
    ++initialize_count;
}

static bool is_boot_owner(void *context) {
    assert(context == regions);
    return boot_owner;
}

static RuntimeBootstrapMemoryRecoveredConfiguration base_configuration(void) {
    return (RuntimeBootstrapMemoryRecoveredConfiguration){
        .lookup_region = lookup_region,
        .map_range = map_range,
        .range_is_reserved = range_is_reserved,
        .shared_region_is_forbidden = shared_region_is_forbidden,
        .attribute_mask = attribute_mask,
        .claim_attributes = claim_attributes,
        .publish_capability = publish_capability,
        .publish_domain = publish_domain,
        .clear_range = clear_range,
        .initialize_base = initialize_base,
        .is_boot_owner = is_boot_owner,
        .capability_metadata = capability_metadata,
        .capability_metadata_capacity = 2,
        .page_attribute_registry = &attribute_registry,
        .current_domain = {&domain, &domain, &domain + 1},
        .shared_mappings = shared_mappings,
        .shared_mapping_count = 3,
        .current_primary_mapping_handle = 9,
        .shared_storage_cursor = &shared_cursor,
        .shared_storage_upper_bound = UINT64_C(0x508000),
        .platform_boot_base = UINT64_C(0x70074000),
        .complete_flag = &complete_flag,
        .active_flag = &active_flag,
        .sealed_flag = &sealed_flag,
        .storage_alias_flag = &storage_alias_flag,
        .context = regions,
    };
}

static void initialize_fixture(void) {
    runtime_bootstrap_memory_recovered_reset();
    clear_bytes(regions, sizeof(regions));
    clear_bytes(mappings, sizeof(mappings));
    clear_bytes(&attribute_registry, sizeof(attribute_registry));
    clear_bytes(&domain, sizeof(domain));
    clear_bytes(shared_mappings, sizeof(shared_mappings));
    clear_bytes(capability_records, sizeof(capability_records));
    clear_bytes(capability_metadata, sizeof(capability_metadata));
    clear_bytes(&descriptor, sizeof(descriptor));
    mapping_count = 0;
    claim_count = 0;
    capability_count = 0;
    domain_publish_count = 0;
    clear_count = 0;
    initialize_count = 0;
    last_claim = (RuntimeMemorySpan){0};
    cleared_range = (RuntimeMemorySpan){0};
    complete_flag = 0;
    active_flag = 0;
    sealed_flag = 0;
    storage_alias_flag = 0;
    shared_cursor = UINT64_C(0x500000);
    boot_owner = true;
    attribute_registry.initialized = true;
    regions[10] = (RuntimePhysicalMemoryRegion){
        .physical_base = UINT64_C(0x100000),
        .size = UINT64_C(0x8000),
    };
    regions[0xa6] = (RuntimePhysicalMemoryRegion){
        .physical_base = UINT64_C(0x600000),
        .size = UINT64_C(0x10000),
    };
    regions[0xa8] = (RuntimePhysicalMemoryRegion){
        .physical_base = UINT64_C(0x620000),
        .size = UINT64_C(0x8000),
    };
    shared_mappings[0] = (RuntimeBootstrapMemorySharedMapping){
        .lower_bound = UINT64_C(0x200000),
        .upper_bound = UINT64_C(0x204000),
        .region_id = 20,
        .capability_handle = 8,
        .enabled = true,
    };
    shared_mappings[1] = (RuntimeBootstrapMemorySharedMapping){
        .lower_bound = UINT64_C(0x300000),
        .upper_bound = UINT64_C(0x304000),
        .region_id = 21,
        .capability_handle = 7,
        .enabled = true,
    };
    shared_mappings[2] = (RuntimeBootstrapMemorySharedMapping){
        .lower_bound = UINT64_C(0x400000),
        .upper_bound = UINT64_C(0x404000),
        .region_id = 22,
        .capability_handle = 9,
        .enabled = true,
    };
    RuntimeBootstrapMemoryRecoveredConfiguration configuration =
        base_configuration();
    runtime_bootstrap_memory_recovered_configure(&configuration);
}

static void expect_panic(void (*operation)(void)) {
    panic_expected = true;
    if (setjmp(panic_environment) == 0) {
        operation();
        assert(false);
    }
    panic_expected = false;
}

static void invalid_clear(void) {
    uint32_t value = 1;
    (void)runtime_bootstrap_outlined_clear_u32_and_return_null_span(
        &value, &value + 1, &value + 1);
}

static void bitmap_test_panic(void) {
    runtime_bootstrap_memory_bitmap_test_bounds_panic();
}

static void bitmap_set_panic(void) {
    runtime_bootstrap_memory_bitmap_set_bounds_panic();
}

static void selector_config_panic(void) {
    runtime_bootstrap_memory_domain_selector_config_failure();
}

static void invalid_profile_selector(void) {
    runtime_bootstrap_memory_prepare_fixed_profile(1);
}

static void repeated_publication(void) {
    runtime_bootstrap_memory_publish_region_capabilities(0, 0);
}

static void test_constants_views_and_outliners(void) {
    initialize_fixture();
    assert(platform_boot_memory_size() == UINT64_C(0x38c000));
    assert(j_platform_boot_memory_size() == UINT64_C(0x38c000));
    runtime_bootstrap_memory_platform_hook_noop();
    runtime_bootstrap_memory_publish_region_hook_noop();
    uint32_t value = 42;
    RuntimeFirebloomSpan empty =
        runtime_bootstrap_outlined_clear_u32_and_return_null_span(
            &value, &value, &value + 1);
    assert(value == 0 && empty.current == 0 && empty.lower_bound == 0 &&
        empty.upper_bound == 0);
    assert(runtime_bootstrap_complete_flag_bounded_view().current ==
        &complete_flag);
    assert(runtime_bootstrap_complete_flag_prepare_result_tuple().current ==
        &complete_flag);
    assert(runtime_bootstrap_memory_active_flag_bounded_view().current ==
        &active_flag);
    assert(runtime_bootstrap_memory_sealed_flag_bounded_view().current ==
        &sealed_flag);
    assert(runtime_bootstrap_memory_storage_alias_flag_bounded_view().current ==
        &storage_alias_flag);
    RuntimeBootstrapMemoryBitmapArguments bitmap =
        runtime_bootstrap_memory_outlined_prepare_region_bitmap_arguments(17);
    assert(bitmap.bitmap == bitmap.lower_bound &&
        bitmap.upper_bound == bitmap.bitmap + 1 && bitmap.region_id == 17);
    expect_panic(invalid_clear);
    expect_panic(bitmap_test_panic);
    expect_panic(bitmap_set_panic);
    expect_panic(selector_config_panic);
    expect_panic(invalid_profile_selector);
}

static void test_region_mapping_and_attributes(void) {
    initialize_fixture();
    runtime_bootstrap_memory_map_identity_region(
        10, UINT64_C(0x100000), UINT64_C(0x8000));
    assert(regions[10].active_alias == UINT64_C(0x100000));
    assert(mappings[0].virtual_address == UINT64_C(0x100000) &&
        mappings[0].physical_address == UINT64_C(0x100000) &&
        mappings[0].flags == RUNTIME_PAGE_ATTRIBUTE_STATIC_CLAIM);
    runtime_bootstrap_memory_publish_region_alias(
        10, UINT64_C(0x900000), UINT64_C(0x100000), UINT64_C(0x8000));
    assert(regions[10].active_alias == UINT64_C(0x900000));
    assert(mappings[1].virtual_address == UINT64_C(0x900000) &&
        mappings[1].physical_address == UINT64_C(0x100000));
    assert(storage_alias_flag == 1);
    runtime_bootstrap_memory_claim_shared_mapping_attributes();
    assert(claim_count == 1);
    assert(last_claim.lower_bound == UINT64_C(0x200000));
    assert(last_claim.upper_bound == UINT64_C(0x204000));
}

static void test_domain_and_task_activation(void) {
    initialize_fixture();
    RuntimeBootstrapMemoryRecoveredConfiguration configuration =
        base_configuration();
    configuration.domain_selector_base = UINT64_C(0x70000000);
    configuration.domain_selector_offset = UINT64_C(0x4000);
    configuration.domain_selector_capacity = 1;
    runtime_bootstrap_memory_recovered_configure(&configuration);
    runtime_bootstrap_memory_map_domain_selector_page();
    assert(mappings[0].virtual_address == UINT64_C(0x70004000));
    assert(mappings[0].regime == 1 && mappings[0].memory_attribute == 4 &&
        mappings[0].flags == RUNTIME_PAGE_ATTRIBUTE_STATIC_CLAIM);

    domain.primary_physical_base = UINT64_C(0x71000000);
    domain.primary_virtual_base = UINT64_C(0x72000000);
    domain.primary_size = UINT64_C(0x10000);
    domain.primary_reserved = UINT64_C(0x4000);
    runtime_bootstrap_memory_activate_current_domain();
    assert(mappings[1].virtual_address == UINT64_C(0x72000000));
    assert(mappings[1].physical_address == UINT64_C(0x71000000));
    assert(domain_publish_count == 1 && clear_count == 1);
    assert(cleared_range.lower_bound == UINT64_C(0x72000000));
    assert(cleared_range.upper_bound == UINT64_C(0x72004000));

    RuntimeTask task = {0};
    runtime_bootstrap_memory_reserve_task_shared_page_storage(
        &task, (uintptr_t)&task, (uintptr_t)(&task + 1));
    assert((uintptr_t)task.lifecycle_aux == UINT64_C(0x500000));
    assert(shared_cursor == UINT64_C(0x508000));
}

static void test_capability_publication(void) {
    initialize_fixture();
    capability_records[0] = (RuntimeBootstrapMemoryRegionRecord){
        .physical_base = UINT64_C(0x800000),
        .size = UINT64_C(0x8000),
        .region_id = 10,
    };
    capability_records[1] = (RuntimeBootstrapMemoryRegionRecord){
        .physical_base = UINT64_C(0x900000),
        .size = UINT64_C(0x4000),
        .region_id = RUNTIME_BOOTSTRAP_MEMORY_SKIPPED_REGION_ID,
    };
    descriptor.bitmap_records = (RuntimeMemoryRegion){
        .base = (uintptr_t)capability_records,
        .size = sizeof(capability_records),
    };
    runtime_bootstrap_memory_publish_region_capabilities(
        &descriptor, sizeof(descriptor));
    assert(complete_flag == 1 && capability_count == 1);
    assert(capability_records[0].capability_handle == 0x121);
    assert(capability_records[0].metadata.current != 0);
    assert(published_metadata.lower_bound == UINT64_C(0x800000));
    assert(published_metadata.upper_bound == UINT64_C(0x808000));
    RuntimeBootstrapMemoryBitmapArguments bitmap =
        runtime_bootstrap_memory_outlined_prepare_region_bitmap_arguments(10);
    assert((bitmap.bitmap->words[0] & (UINT64_C(1) << 10)) != 0);
    expect_panic(repeated_publication);
}

static void test_fixed_profile_activation(void) {
    initialize_fixture();
    RuntimeBootstrapMemoryRecoveredConfiguration configuration =
        base_configuration();
    configuration.fixed_identity_range = (RuntimeMemorySpan){
        UINT64_C(0x73000000), UINT64_C(0x73008000)};
    configuration.fixed_secondary_mapping =
        (RuntimeBootstrapMemoryMappingRequest){
            .virtual_address = UINT64_C(0x74000000),
            .physical_address = UINT64_C(0x640000),
            .size = UINT64_C(0x4000),
            .regime = 0,
            .memory_attribute = 0,
            .flags = 1,
        };
    runtime_bootstrap_memory_recovered_configure(&configuration);
    runtime_bootstrap_memory_activate_fixed_profile();
    assert(initialize_count == 1 && active_flag == 1 && complete_flag == 1);
    assert(regions[0xa6].active_alias == regions[0xa6].physical_base);
    assert(((regions[0xa6].descriptor >> 16) & 0xffff) == 9);
    assert(((regions[0xa8].descriptor >> 16) & 0xffff) ==
        RUNTIME_BOOTSTRAP_MEMORY_CAPABILITY_TYPE);
    assert(mapping_count == 4);
    assert(mappings[0].size == platform_boot_memory_size());
    assert(mappings[2].virtual_address == UINT64_C(0x73000000));
    assert(mappings[3].physical_address == UINT64_C(0x640000));
}

int main(void) {
    test_constants_views_and_outliners();
    test_region_mapping_and_attributes();
    test_domain_and_task_activation();
    test_capability_publication();
    test_fixed_profile_activation();
    puts("runtime bootstrap memory recovered tests passed");
}
