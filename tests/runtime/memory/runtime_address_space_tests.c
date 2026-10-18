#include "avp/runtime/exception/pvpanic.h"
#include "avp/runtime/memory/runtime_address_space.h"
#include "avp/runtime/ipc/runtime_capability.h"
#include "avp/runtime/ipc/runtime_domain.h"
#include "avp/runtime/memory/runtime_physical_memory.h"
#include "avp/runtime/support/runtime_state.h"

#include <assert.h>
#include <signal.h>
#include <stdint.h>
#include <sys/wait.h>
#include <unistd.h>


static RuntimePhysicalMemoryRegion physical_regions[2];

void runtime_page_table_arch_publish(void) {}

void runtime_page_table_arch_invalidate_all(void) {}

_Noreturn void pvpanic_trigger(void) {
    __builtin_trap();
}

static void expect_fatal(void (*operation)(void)) {
    const pid_t child = fork();
    assert(child >= 0);
    if (child == 0) {
        operation();
        _Exit(0);
    }
    int status = 0;
    assert(waitpid(child, &status, 0) == child);
    assert(WIFSIGNALED(status));
}

static void initialize_runtime(void) {
    runtime_state_initialize();
    runtime_capability_initialize();
    runtime_domain_reset(AVP_RUNTIME_DOMAIN_BOOT);
    runtime_address_space_initialize();
    physical_regions[0] = (RuntimePhysicalMemoryRegion){
        .physical_base = UINT64_C(0x200000),
        .active_alias = UINT64_C(0x70000000),
        .size = UINT64_C(0x8000),
        .descriptor =
            (UINT64_C(0x80000009) << 16) | UINT64_C(0x00a6),
    };
    physical_regions[1] = (RuntimePhysicalMemoryRegion){
        .physical_base = UINT64_C(0x300000),
        .active_alias = UINT64_C(0x70010000),
        .size = UINT64_C(0x4000),
        .descriptor =
            (UINT64_C(0x80000009) << 16) | UINT64_C(0x0163),
    };
    runtime_physical_memory_registry_initialize(
        (RuntimePhysicalMemoryRegistry){
            .records = physical_regions,
            .count = 2,
        }
    );
}

static RuntimeAddressSpaceMapRequest mapping_request(
    uintptr_t virtual_lower_bound,
    uintptr_t physical_base,
    uint16_t physical_region_id
) {
    return (RuntimeAddressSpaceMapRequest){
        .virtual_lower_bound = virtual_lower_bound,
        .size = UINT64_C(0x8000),
        .physical_base = physical_base,
        .audience = runtime_capability_audience_for_domain(
            AVP_RUNTIME_DOMAIN_BOOT
        ),
        .physical_region_id = physical_region_id,
        .owner_domain = AVP_RUNTIME_DOMAIN_BOOT,
        .memory_attributes = 3,
        .mapping_attributes = 1,
        .flags = RUNTIME_ADDRESS_SPACE_MAPPING_FLAG_SHARED,
        .rights = 7,
    };
}

static void test_publish_resolve_and_merge(void) {
    initialize_runtime();
    RuntimeAddressSpaceMapRequest request = mapping_request(
        UINT64_C(0x70000000),
        UINT64_C(0x200000),
        UINT16_C(0x00a6)
    );
    const uint16_t handle =
        runtime_address_space_mapping_publish(&request);
    assert(handle != 0);
    RuntimeAddressSpaceMapping *mapping =
        runtime_address_space_mapping_resolve(handle, 7);
    assert(mapping->virtual_lower_bound == UINT64_C(0x70000000));
    assert(mapping->virtual_upper_bound == UINT64_C(0x70008000));
    assert(mapping->physical_base == UINT64_C(0x200000));
    assert(mapping->audience == runtime_capability_audience_for_domain(
        AVP_RUNTIME_DOMAIN_BOOT
    ));
    assert(mapping->physical_region_id == UINT16_C(0x00a6));
    assert(mapping->owner_domain == AVP_RUNTIME_DOMAIN_BOOT);
    assert(mapping->memory_attributes == 3);
    assert(mapping->mapping_attributes == 1);
    assert(mapping->flags == RUNTIME_ADDRESS_SPACE_MAPPING_FLAG_SHARED);
    assert(runtime_address_space_mapping_contains_domain(
        mapping,
        AVP_RUNTIME_DOMAIN_BOOT
    ));
    assert(runtime_address_space_mapping_publish(&request) == handle);
    assert(runtime_address_space_mapping_count() == 1);
}

static void test_attach_region_idempotently(void) {
    initialize_runtime();
    const RuntimeAddressSpaceMapResult first =
        runtime_address_space_attach_region(
            UINT16_C(0x00a6),
            0,
            3,
            1,
            RUNTIME_ADDRESS_SPACE_MAPPING_FLAG_SHARED,
            7
        );
    assert(first.mapping_handle != 0);
    assert(first.virtual_lower_bound == UINT64_C(0x70000000));
    assert((uint16_t)(physical_regions[0].descriptor >> 48) ==
        first.mapping_handle);
    assert(runtime_address_space_region_is_attached(UINT16_C(0x00a6)));
    assert(!runtime_address_space_region_is_attached(UINT16_C(0x0163)));
    const RuntimeAddressSpaceMapResult second =
        runtime_address_space_attach_region(
            UINT16_C(0x00a6),
            0,
            3,
            1,
            RUNTIME_ADDRESS_SPACE_MAPPING_FLAG_SHARED,
            7
        );
    assert(second.mapping_handle == first.mapping_handle);
    assert(runtime_address_space_mapping_count() == 1);
}

static void test_range_classification(void) {
    initialize_runtime();
    assert(runtime_address_space_range_is_direct(
        UINT64_C(0x70000000),
        UINT64_C(0x4000),
        UINT16_C(0x00a6)
    ));
    assert(runtime_address_space_range_is_direct(
        UINT64_C(0xfffffc0000000000),
        UINT64_C(0x4000),
        UINT16_C(0x00a6)
    ));
    assert(runtime_address_space_range_is_direct(
        UINT64_C(0x0000020000000000),
        UINT64_C(0x4000),
        UINT16_C(0x0163)
    ));
    assert(!runtime_address_space_range_is_direct(
        UINT64_C(0x0000020000000000),
        UINT64_C(0x4000),
        UINT16_C(0x00a6)
    ));
    runtime_address_space_configure(&(RuntimeAddressSpaceProfile){
        .low_direct_upper_bound = UINT64_C(0x80000000),
        .high_direct_lower_bound = UINT64_C(0xfffffd0000000000),
        .special_direct_region_id = UINT16_C(0x0102),
    });
    assert(!runtime_address_space_range_is_direct(
        UINT64_C(0x90000000),
        UINT64_C(0x4000),
        UINT16_C(0x0163)
    ));
}

static void test_page_table_lifecycle(void) {
    static RuntimePageTablePage pages[8];
    RuntimePageTablePool pool;
    RuntimePageTable table;
    initialize_runtime();
    assert(runtime_page_table_pool_initialize(
        &pool,
        pages,
        UINT64_C(0x10000000),
        8
    ) == RUNTIME_PAGE_TABLE_STATUS_OK);
    assert(runtime_page_table_initialize(
        &table,
        &pool,
        42,
        0
    ) == RUNTIME_PAGE_TABLE_STATUS_OK);
    runtime_address_space_bind_page_table(&table);

    const uintptr_t virtual_address = UINT64_C(0x60000000);
    const RuntimeAddressSpaceMapResult result =
        runtime_address_space_attach_region(
            UINT16_C(0x00a6),
            virtual_address,
            3,
            1,
            RUNTIME_ADDRESS_SPACE_MAPPING_FLAG_SHARED,
            7
        );
    RuntimePageTableTranslation translation;
    assert(runtime_page_table_lookup(
        &table,
        virtual_address + RUNTIME_ADDRESS_SPACE_PAGE_SIZE,
        &translation
    ) == RUNTIME_PAGE_TABLE_STATUS_OK);
    assert(translation.physical_address ==
        UINT64_C(0x200000) + RUNTIME_ADDRESS_SPACE_PAGE_SIZE);
    assert(translation.attributes.memory_attribute_index == 3);
    assert(translation.attributes.privileged_execute_never);
    assert(translation.attributes.unprivileged_execute_never);

    runtime_address_space_reprotect(
        result.mapping_handle,
        4,
        0,
        RUNTIME_ADDRESS_SPACE_MAPPING_FLAG_SHARED
    );
    assert(runtime_page_table_lookup(
        &table,
        virtual_address,
        &translation
    ) == RUNTIME_PAGE_TABLE_STATUS_OK);
    assert(translation.attributes.memory_attribute_index == 4);
    assert(!translation.attributes.privileged_execute_never);
    assert(!translation.attributes.unprivileged_execute_never);

    runtime_address_space_detach_region(UINT16_C(0x00a6));
    assert(!runtime_address_space_region_is_attached(UINT16_C(0x00a6)));
    assert(runtime_address_space_mapping_count() == 0);
    assert(runtime_page_table_lookup(
        &table,
        virtual_address,
        &translation
    ) == RUNTIME_PAGE_TABLE_STATUS_NOT_MAPPED);
}

static void overlapping_mapping(void) {
    initialize_runtime();
    RuntimeAddressSpaceMapRequest first = mapping_request(
        UINT64_C(0x70000000),
        UINT64_C(0x200000),
        UINT16_C(0x00a6)
    );
    (void)runtime_address_space_mapping_publish(&first);
    RuntimeAddressSpaceMapRequest second = mapping_request(
        UINT64_C(0x70004000),
        UINT64_C(0x300000),
        UINT16_C(0x0163)
    );
    (void)runtime_address_space_mapping_publish(&second);
}

static void unaligned_mapping(void) {
    initialize_runtime();
    RuntimeAddressSpaceMapRequest request = mapping_request(
        UINT64_C(0x70000001),
        UINT64_C(0x200000),
        UINT16_C(0x00a6)
    );
    (void)runtime_address_space_mapping_publish(&request);
}

static void incompatible_merge(void) {
    initialize_runtime();
    RuntimeAddressSpaceMapRequest request = mapping_request(
        UINT64_C(0x70000000),
        UINT64_C(0x200000),
        UINT16_C(0x00a6)
    );
    (void)runtime_address_space_mapping_publish(&request);
    request.memory_attributes = 4;
    (void)runtime_address_space_mapping_publish(&request);
}

static void conflict_scan(void) {
    initialize_runtime();
    RuntimeAddressSpaceMapRequest request = mapping_request(
        UINT64_C(0x70000000),
        UINT64_C(0x200000),
        UINT16_C(0x00a6)
    );
    (void)runtime_address_space_mapping_publish(&request);
    runtime_address_space_assert_no_conflict(
        UINT64_C(0x70004000),
        UINT64_C(0x7000c000),
        false
    );
}

int main(void) {
    test_publish_resolve_and_merge();
    test_attach_region_idempotently();
    test_range_classification();
    test_page_table_lifecycle();
    expect_fatal(overlapping_mapping);
    expect_fatal(unaligned_mapping);
    expect_fatal(incompatible_merge);
    expect_fatal(conflict_scan);
}
