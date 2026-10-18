#include "avp/runtime/ipc/runtime_capability.h"
#include "avp/runtime/memory/runtime_address_space.h"
#include "avp/runtime/ipc/runtime_domain.h"
#include "avp/runtime/support/runtime_state.h"

#include <assert.h>
#include <signal.h>
#include <stdint.h>
#include <sys/wait.h>
#include <unistd.h>


static const RuntimeFirebloomType definition_type = {
    .identity = UINT64_C(0x646f6d646566),
    .element_size = sizeof(RuntimeDomainDefinition),
    .bytewise_copyable = false,
};

static const RuntimeFirebloomType physical_memory_type = {
    .identity = UINT64_C(0x706879736d656d),
    .element_size = sizeof(RuntimeDomainPhysicalMemory),
    .bytewise_copyable = false,
};

typedef struct {
    RuntimeDomainDefinition definition;
    RuntimeDomainPhysicalMemory physical_memory;
    RuntimeDomainDescriptor descriptor;
    RuntimeFirebloomSpan output;
} DomainFixture;

static RuntimeFirebloomSpan span(
    void *object,
    size_t size,
    const RuntimeFirebloomType *type
) {
    return (RuntimeFirebloomSpan){
        .current = object,
        .lower_bound = object,
        .upper_bound = (uint8_t *)object + size,
        .type = type,
    };
}

static void runtime_initialize(void) {
    runtime_state_initialize();
    runtime_capability_initialize();
    runtime_domain_reset(AVP_RUNTIME_DOMAIN_BOOT);
}

static void fixture_initialize(
    DomainFixture *fixture,
    uint8_t domain_id,
    uint64_t identity,
    uintptr_t physical_base,
    size_t physical_size,
    uintptr_t optional_virtual_lower,
    size_t optional_size,
    uint32_t allocation_handle,
    uintptr_t allocation_physical_base
) {
    fixture->definition = (RuntimeDomainDefinition){
        .identity = identity,
        .optional_virtual_lower_bound = optional_virtual_lower,
        .optional_virtual_upper_bound = optional_virtual_lower + optional_size,
        .descriptor_kind = RUNTIME_DOMAIN_DESCRIPTOR_KIND,
        .domain_id = domain_id,
    };
    fixture->physical_memory = (RuntimeDomainPhysicalMemory){
        .physical_base = physical_base,
        .physical_upper_bound = physical_base + physical_size,
        .size = physical_size,
    };
    const uint16_t definition_handle = runtime_capability_publish(
        &fixture->definition,
        &runtime_domain_definition_type,
        RUNTIME_CAPABILITY_TYPE_DOMAIN_DEFINITION,
        UINT8_C(7),
        runtime_capability_audience_for_domain(AVP_RUNTIME_DOMAIN_BOOT)
    );
    assert(definition_handle != 0);
    fixture->descriptor = (RuntimeDomainDescriptor){
        .identity = identity,
        .definition = span(
            &fixture->definition,
            sizeof(fixture->definition),
            &definition_type
        ),
        .physical_memory = span(
            &fixture->physical_memory,
            sizeof(fixture->physical_memory),
            &physical_memory_type
        ),
        .mapping_region = 0xa6,
        .optional_allocation_handle = allocation_handle,
        .parent_capability_handle = definition_handle,
        .mapping_mode = 0,
        .parent_capability_rights = 7,
        .parent_capability_type =
            RUNTIME_CAPABILITY_TYPE_DOMAIN_DEFINITION,
        .parent_domain = AVP_RUNTIME_DOMAIN_BOOT,
        .capability_rights = 7,
    };
    fixture->output = (RuntimeFirebloomSpan){0};
    if (optional_size != 0) {
        runtime_domain_register_allocation(
            allocation_handle,
            allocation_physical_base,
            optional_size
        );
    }
}

static uint16_t fixture_create(DomainFixture *fixture) {
    return runtime_domain_create_and_register(
        &fixture->descriptor,
        (uintptr_t)&fixture->descriptor,
        (uintptr_t)(&fixture->descriptor + 1),
        &fixture->output,
        (uintptr_t)&fixture->output,
        (uintptr_t)(&fixture->output + 1)
    );
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

static void test_create_and_publish(void) {
    runtime_initialize();
    DomainFixture fixture;
    fixture_initialize(
        &fixture,
        2,
        UINT64_C(0xaa0000006c697475),
        UINT64_C(0x00200000),
        UINT64_C(0x00008000),
        UINT64_C(0xfffffcf000000000),
        UINT64_C(0x00008000),
        0x102,
        UINT64_C(0x00800000)
    );
    const uint16_t runtime_handle = fixture_create(&fixture);
    RuntimeDomain *domain = fixture.output.current;
    assert(domain != 0);
    assert(fixture.output.lower_bound == domain);
    assert(fixture.output.upper_bound == domain + 1);
    assert(fixture.output.type == &runtime_domain_record_type);
    assert(runtime_domain_lookup(2) == domain);
    assert(runtime_handle == domain->runtime_handle);
    assert(domain->runtime_handle != domain->public_handle);
    assert(domain->runtime_handle != domain->primary_mapping_handle);
    assert(domain->service_key == UINT64_C(0x000000006c697475));
    assert(runtime_domain_record_identity(fixture.output) == domain->service_key);
    assert(runtime_domain_from_registry_link(fixture.output) == domain);
    assert(domain->definition.current == &fixture.definition);
    assert(domain->primary_physical_base == UINT64_C(0x00200000));
    assert(domain->primary_virtual_base == UINT64_C(0xfffffc0200000000));
    assert(domain->primary_size == UINT64_C(0x00008000));
    assert(domain->optional_physical_base == UINT64_C(0x00800000));
    assert(
        domain->optional_virtual_lower_bound ==
        UINT64_C(0xfffffcf000000000)
    );
    assert(
        domain->optional_virtual_upper_bound ==
        UINT64_C(0xfffffcf000008000)
    );
    assert(domain->address_space_prefix == UINT64_C(0xfffffc0210000000));
    assert(domain->parent_domain == AVP_RUNTIME_DOMAIN_BOOT);
    assert(domain->domain_id == 2);
    assert(!runtime_domain_resolve_locality(2));
    assert(runtime_capability_resolve(
        runtime_handle,
        RUNTIME_CAPABILITY_TYPE_DOMAIN,
        UINT8_C(7)
    ) == domain);

    runtime_domain_set_current(domain);
    assert(runtime_current_domain() == 2);
    assert(runtime_domain_resolve_locality(2));
    RuntimeAddressSpaceMapping *primary_mapping = runtime_capability_resolve(
        domain->primary_mapping_handle,
        RUNTIME_CAPABILITY_TYPE_MEMORY_MAPPING,
        UINT8_C(7)
    );
    assert(primary_mapping != 0);
    assert(primary_mapping->virtual_lower_bound == domain->primary_virtual_base);
    assert(primary_mapping->virtual_upper_bound ==
        domain->primary_virtual_base + domain->primary_size);
    assert(primary_mapping->physical_base == domain->primary_physical_base);
    assert(primary_mapping->physical_region_id == 0xa6);
    assert(primary_mapping->owner_domain == 2);
    volatile RuntimeGlobalState *state = runtime_state_get();
    assert(state->current_domain == 2);
    assert(state->runtime_handle == runtime_handle);
}

static void fatal_duplicate_identity(void) {
    runtime_initialize();
    DomainFixture first;
    DomainFixture second;
    fixture_initialize(
        &first, 2, UINT64_C(0x6c697475),
        0x200000, 0x4000, 0, 0, 0, 0
    );
    (void)fixture_create(&first);
    fixture_initialize(
        &second, 3, UINT64_C(0x6c697475),
        0x300000, 0x4000, 0, 0, 0, 0
    );
    (void)fixture_create(&second);
}

static void fatal_duplicate_domain_id(void) {
    runtime_initialize();
    DomainFixture first;
    DomainFixture second;
    fixture_initialize(
        &first, 2, UINT64_C(0x6c697475),
        0x200000, 0x4000, 0, 0, 0, 0
    );
    (void)fixture_create(&first);
    fixture_initialize(
        &second, 2, UINT64_C(0x737973),
        0x300000, 0x4000, 0, 0, 0, 0
    );
    (void)fixture_create(&second);
}

static void fatal_primary_overlap(void) {
    runtime_initialize();
    DomainFixture first;
    DomainFixture second;
    fixture_initialize(
        &first, 2, UINT64_C(0x6c697475),
        0x200000, 0x8000, 0, 0, 0, 0
    );
    (void)fixture_create(&first);
    fixture_initialize(
        &second, 3, UINT64_C(0x737973),
        0x204000, 0x4000, 0, 0, 0, 0
    );
    (void)fixture_create(&second);
}

static void fatal_optional_overlap(void) {
    runtime_initialize();
    DomainFixture first;
    DomainFixture second;
    fixture_initialize(
        &first, 2, UINT64_C(0x6c697475),
        0x200000, 0x4000, UINT64_C(0xfffffcf000000000),
        0x8000, 0x102, 0x800000
    );
    (void)fixture_create(&first);
    fixture_initialize(
        &second, 3, UINT64_C(0x737973),
        0x300000, 0x4000, UINT64_C(0xfffffcf000000000),
        0x4000, 0x103, 0x900000
    );
    (void)fixture_create(&second);
}

static void fatal_descriptor_bounds(void) {
    runtime_initialize();
    DomainFixture fixture;
    fixture_initialize(
        &fixture, 2, UINT64_C(0x6c697475),
        0x200000, 0x4000, 0, 0, 0, 0
    );
    (void)runtime_domain_create_and_register(
        &fixture.descriptor,
        (uintptr_t)&fixture.descriptor,
        (uintptr_t)&fixture.descriptor + 0x50,
        0,
        0,
        0
    );
}

static void fatal_parent_capability_mismatch(void) {
    runtime_initialize();
    DomainFixture fixture;
    RuntimeDomainDefinition foreign_definition = {0};
    fixture_initialize(
        &fixture, 2, UINT64_C(0x6c697475),
        0x200000, 0x4000, 0, 0, 0, 0
    );
    fixture.descriptor.parent_capability_handle = runtime_capability_publish(
        &foreign_definition,
        &runtime_domain_definition_type,
        RUNTIME_CAPABILITY_TYPE_DOMAIN_DEFINITION,
        UINT8_C(7),
        runtime_capability_audience_for_domain(AVP_RUNTIME_DOMAIN_BOOT)
    );
    (void)fixture_create(&fixture);
}

static void fatal_unaligned_primary_mapping(void) {
    runtime_initialize();
    DomainFixture fixture;
    fixture_initialize(
        &fixture, 2, UINT64_C(0x6c697475),
        0x201000, 0x4000, 0, 0, 0, 0
    );
    (void)fixture_create(&fixture);
}

int main(void) {
    test_create_and_publish();
    expect_fatal(fatal_duplicate_identity);
    expect_fatal(fatal_duplicate_domain_id);
    expect_fatal(fatal_primary_overlap);
    expect_fatal(fatal_optional_overlap);
    expect_fatal(fatal_descriptor_bounds);
    expect_fatal(fatal_parent_capability_mismatch);
    expect_fatal(fatal_unaligned_primary_mapping);
}
