#include "avp/runtime/ipc/runtime_domain.h"

#include "avp/runtime/memory/runtime_address_space.h"
#include "avp/runtime/ipc/runtime_capability.h"
#include "avp/runtime/support/runtime_state.h"


enum {
    RUNTIME_DOMAIN_CAPACITY = RUNTIME_DOMAIN_ID_MAX,
    RUNTIME_DYNAMIC_DOMAIN_CAPACITY = RUNTIME_DOMAIN_CAPACITY - 1,
    RUNTIME_DOMAIN_ALLOCATION_CAPACITY = RUNTIME_DOMAIN_CAPACITY,
    RUNTIME_DOMAIN_PAGE_SIZE = 0x4000,
    RUNTIME_DOMAIN_PRIMARY_SIZE_MAX = 0x0c000000,
    RUNTIME_DOMAIN_OPTIONAL_SIZE_MAX = 0x04000000,
    RUNTIME_DOMAIN_OPTIONAL_ALIGNMENT = 0x40000000,
};

static const uintptr_t runtime_domain_optional_window_lower =
    UINT64_C(0xfffffcf000000000);
static const uintptr_t runtime_domain_optional_window_upper =
    UINT64_C(0xfffffcf400000000);

#if defined(__ELF__)
#define RUNTIME_DOMAIN_SECTION \
    __attribute__((section(".runtime_task.state")))
#else
#define RUNTIME_DOMAIN_SECTION
#endif

typedef struct {
    uint32_t handle;
    uintptr_t physical_base;
    size_t size;
} RuntimeDomainAllocation;

uint8_t runtime_execution_domain = AVP_RUNTIME_DOMAIN_BOOT;

const RuntimeFirebloomType runtime_domain_record_type = {
    .identity = UINT64_C(0x646f6d61696e),
    .element_size = sizeof(RuntimeDomain),
    .bytewise_copyable = false,
};

const RuntimeFirebloomType runtime_domain_definition_type = {
    .identity = UINT64_C(0x646f6d646566),
    .element_size = sizeof(RuntimeDomainDefinition),
    .bytewise_copyable = false,
};

static const RuntimeFirebloomType runtime_domain_identity_type = {
    .identity = UINT64_C(0x646f6d6964),
    .element_size = sizeof(uint64_t),
    .bytewise_copyable = false,
};

static RuntimeDomain boot_domain RUNTIME_DOMAIN_SECTION;
static RuntimeDomain dynamic_domains[RUNTIME_DYNAMIC_DOMAIN_CAPACITY]
    RUNTIME_DOMAIN_SECTION;
static RuntimeAddressSpaceMapping primary_mappings[
    RUNTIME_DYNAMIC_DOMAIN_CAPACITY
] RUNTIME_DOMAIN_SECTION;
static bool dynamic_domain_slots[RUNTIME_DYNAMIC_DOMAIN_CAPACITY]
    RUNTIME_DOMAIN_SECTION;
static RuntimeDomainRegistryLink domain_registry_root
    RUNTIME_DOMAIN_SECTION;
static RuntimeDomainAllocation allocations[
    RUNTIME_DOMAIN_ALLOCATION_CAPACITY
] RUNTIME_DOMAIN_SECTION;

static void runtime_domain_clear(void *storage, size_t size) {
    uint8_t *bytes = storage;
    for (size_t index = 0; index < size; ++index) {
        bytes[index] = 0;
    }
}

_Noreturn static void runtime_domain_trap(void) {
    __builtin_trap();
}

static bool runtime_domain_address_range_contains(
    uintptr_t current,
    uintptr_t lower_bound,
    uintptr_t upper_bound,
    size_t size
) {
    return current >= lower_bound && current < upper_bound &&
        size <= upper_bound - current;
}

static bool runtime_domain_range_is_valid(
    uintptr_t lower_bound,
    size_t size
) {
    return size != 0 && lower_bound <= UINTPTR_MAX - size;
}

static bool runtime_domain_ranges_overlap(
    uintptr_t left_lower,
    size_t left_size,
    uintptr_t right_lower,
    size_t right_size
) {
    return left_lower < right_lower + right_size &&
        right_lower < left_lower + left_size;
}

static uint16_t runtime_domain_audience(uint8_t domain_id) {
    return runtime_capability_audience_for_domain(domain_id);
}

static void runtime_domain_set_link_capability(
    void **current,
    uintptr_t *lower_bound,
    uintptr_t *upper_bound,
    uintptr_t *cookie,
    RuntimeDomainRegistryLink *link
) {
    *current = link;
    *lower_bound = (uintptr_t)link;
    *upper_bound = (uintptr_t)(link + 1);
    *cookie = (uintptr_t)&runtime_domain_record_type;
}

static void runtime_domain_registry_initialize(void) {
    runtime_domain_clear(&domain_registry_root, sizeof(domain_registry_root));
    runtime_domain_set_link_capability(
        &domain_registry_root.previous,
        &domain_registry_root.previous_lower_bound,
        &domain_registry_root.previous_upper_bound,
        &domain_registry_root.previous_cookie,
        &domain_registry_root
    );
    runtime_domain_set_link_capability(
        &domain_registry_root.next,
        &domain_registry_root.next_lower_bound,
        &domain_registry_root.next_upper_bound,
        &domain_registry_root.next_cookie,
        &domain_registry_root
    );
}

static void runtime_domain_registry_insert(RuntimeDomain *domain) {
    RuntimeDomainRegistryLink *link = &domain->registry_link;
    RuntimeDomainRegistryLink *first = domain_registry_root.next;
    if (link->previous != 0 || link->next != 0 || first == 0 ||
        first->previous != &domain_registry_root) {
        runtime_domain_trap();
    }
    runtime_domain_set_link_capability(
        &link->previous,
        &link->previous_lower_bound,
        &link->previous_upper_bound,
        &link->previous_cookie,
        &domain_registry_root
    );
    runtime_domain_set_link_capability(
        &link->next,
        &link->next_lower_bound,
        &link->next_upper_bound,
        &link->next_cookie,
        first
    );
    runtime_domain_set_link_capability(
        &first->previous,
        &first->previous_lower_bound,
        &first->previous_upper_bound,
        &first->previous_cookie,
        link
    );
    runtime_domain_set_link_capability(
        &domain_registry_root.next,
        &domain_registry_root.next_lower_bound,
        &domain_registry_root.next_upper_bound,
        &domain_registry_root.next_cookie,
        link
    );
}

static RuntimeDomain *runtime_domain_dynamic_allocate(void) {
    for (uint8_t index = 0;
         index < RUNTIME_DYNAMIC_DOMAIN_CAPACITY;
         ++index) {
        if (!dynamic_domain_slots[index]) {
            dynamic_domain_slots[index] = true;
            runtime_domain_clear(
                &dynamic_domains[index],
                sizeof(dynamic_domains[index])
            );
            return &dynamic_domains[index];
        }
    }
    runtime_domain_trap();
}

static const RuntimeDomainAllocation *runtime_domain_allocation_lookup(
    uint32_t handle
) {
    for (uint8_t index = 0;
         index < RUNTIME_DOMAIN_ALLOCATION_CAPACITY;
         ++index) {
        if (allocations[index].handle == handle) {
            return &allocations[index];
        }
    }
    return 0;
}

static void runtime_domain_validate_object_span(
    const RuntimeFirebloomSpan *span,
    size_t size
) {
    if (span == 0 || span->type == 0 ||
        !runtime_domain_address_range_contains(
            (uintptr_t)span->current,
            (uintptr_t)span->lower_bound,
            (uintptr_t)span->upper_bound,
            size
        )) {
        runtime_domain_trap();
    }
}

#if defined(__ELF__)
#define RUNTIME_DOMAIN_RECOVERED_ENTRY \
    __attribute__((noinline, section(".text.runtime_domain_recovered")))
#else
#define RUNTIME_DOMAIN_RECOVERED_ENTRY __attribute__((noinline))
#endif

RUNTIME_DOMAIN_RECOVERED_ENTRY
uint64_t runtime_domain_identity_read(RuntimeFirebloomSpan identity) {
    runtime_domain_validate_object_span(&identity, sizeof(uint64_t));
    return *(const uint64_t *)identity.current;
}

RUNTIME_DOMAIN_RECOVERED_ENTRY
uint64_t runtime_domain_record_identity(RuntimeFirebloomSpan record) {
    runtime_domain_validate_object_span(&record, sizeof(RuntimeDomain));
    RuntimeDomain *domain = record.current;
    return runtime_domain_identity_read((RuntimeFirebloomSpan){
        .current = &domain->service_key,
        .lower_bound = domain,
        .upper_bound = domain + 1,
        .type = &runtime_domain_identity_type,
    });
}

RUNTIME_DOMAIN_RECOVERED_ENTRY
RuntimeDomain *runtime_domain_from_registry_link(
    RuntimeFirebloomSpan link_record
) {
    runtime_domain_validate_object_span(
        &link_record,
        sizeof(RuntimeDomain)
    );
    return link_record.current;
}

static RuntimeDomain *runtime_domain_record_from_link(
    RuntimeDomainRegistryLink *link
) {
    return runtime_domain_from_registry_link((RuntimeFirebloomSpan){
        .current = link,
        .lower_bound = link,
        .upper_bound = (RuntimeDomain *)link + 1,
        .type = &runtime_domain_record_type,
    });
}

static void runtime_domain_validate_descriptor(
    const RuntimeDomainDescriptor *descriptor,
    uintptr_t descriptor_lower_bound,
    uintptr_t descriptor_upper_bound
) {
    if (descriptor == 0 ||
        !runtime_domain_address_range_contains(
            (uintptr_t)descriptor,
            descriptor_lower_bound,
            descriptor_upper_bound,
            sizeof(*descriptor)
        )) {
        runtime_domain_trap();
    }
    runtime_domain_validate_object_span(
        &descriptor->definition,
        sizeof(RuntimeDomainDefinition)
    );
    runtime_domain_validate_object_span(
        &descriptor->physical_memory,
        sizeof(RuntimeDomainPhysicalMemory)
    );
}

static void runtime_domain_validate_output(
    RuntimeFirebloomSpan *output_record,
    uintptr_t output_lower_bound,
    uintptr_t output_upper_bound
) {
    if (output_record != 0 &&
        !runtime_domain_address_range_contains(
            (uintptr_t)output_record,
            output_lower_bound,
            output_upper_bound,
            sizeof(*output_record)
        )) {
        runtime_domain_trap();
    }
}

static void runtime_domain_validate_unique(
    uint64_t service_key,
    uint8_t domain_id,
    uintptr_t primary_physical_base,
    size_t primary_size,
    uintptr_t optional_virtual_lower_bound,
    size_t optional_size
) {
    RuntimeDomainRegistryLink *link = domain_registry_root.next;
    while (link != &domain_registry_root) {
        if (link == 0 || link->next == 0 ||
            ((RuntimeDomainRegistryLink *)link->next)->previous != link) {
            runtime_domain_trap();
        }
        RuntimeDomain *existing = runtime_domain_record_from_link(link);
        if (runtime_domain_record_identity((RuntimeFirebloomSpan){
                .current = existing,
                .lower_bound = existing,
                .upper_bound = existing + 1,
                .type = &runtime_domain_record_type,
            }) == service_key ||
            existing->domain_id == domain_id) {
            runtime_domain_trap();
        }
        if (existing->primary_size != 0 &&
            runtime_domain_ranges_overlap(
                existing->primary_physical_base,
                existing->primary_size,
                primary_physical_base,
                primary_size
            )) {
            runtime_domain_trap();
        }
        const size_t existing_optional_size =
            existing->optional_virtual_upper_bound -
            existing->optional_virtual_lower_bound;
        if (optional_size != 0 && existing_optional_size != 0 &&
            runtime_domain_ranges_overlap(
                existing->optional_virtual_lower_bound,
                existing_optional_size,
                optional_virtual_lower_bound,
                optional_size
            )) {
            runtime_domain_trap();
        }
        link = link->next;
    }
}

RuntimeDomain *runtime_domain_lookup(uint8_t domain) {
    RuntimeDomainRegistryLink *link = domain_registry_root.next;
    while (link != &domain_registry_root) {
        if (link == 0 || link->next == 0 ||
            ((RuntimeDomainRegistryLink *)link->next)->previous != link) {
            runtime_domain_trap();
        }
        RuntimeDomain *record = runtime_domain_record_from_link(link);
        if (record->domain_id == domain) {
            return record;
        }
        link = link->next;
    }
    return 0;
}

bool runtime_domain_registry_contains(uint8_t domain) {
    return runtime_domain_lookup(domain) != 0;
}

static void runtime_domain_initialize_record(
    RuntimeDomain *domain,
    uint64_t service_key,
    const RuntimeFirebloomSpan *definition,
    uint8_t domain_id,
    uint8_t parent_domain,
    uintptr_t primary_physical_base,
    size_t primary_size,
    uintptr_t optional_physical_base,
    uintptr_t optional_virtual_lower_bound,
    uintptr_t optional_virtual_upper_bound
) {
    runtime_domain_clear(domain, sizeof(*domain));
    domain->definition = definition == 0
        ? (RuntimeFirebloomSpan){0}
        : *definition;
    domain->primary_physical_base = primary_physical_base;
    domain->primary_virtual_base =
        UINT64_C(0xfffffc0000000000) |
        ((uint64_t)domain_id << 32);
    domain->primary_size = primary_size;
    domain->optional_physical_base = optional_physical_base;
    domain->optional_virtual_lower_bound = optional_virtual_lower_bound;
    domain->optional_virtual_upper_bound = optional_virtual_upper_bound;
    domain->address_space_prefix =
        UINT64_C(0xfffffc0010000000) |
        ((uint64_t)domain_id << 32);
    domain->parent_domain = parent_domain;
    domain->domain_id = domain_id;
    domain->service_key = service_key;
}

void runtime_domain_reset(uint8_t domain) {
    if (domain == 0 || domain > RUNTIME_DOMAIN_ID_MAX) {
        runtime_domain_trap();
    }
    runtime_domain_registry_initialize();
    runtime_domain_clear(dynamic_domains, sizeof(dynamic_domains));
    runtime_domain_clear(primary_mappings, sizeof(primary_mappings));
    runtime_domain_clear(dynamic_domain_slots, sizeof(dynamic_domain_slots));
    runtime_domain_clear(allocations, sizeof(allocations));
    runtime_domain_initialize_record(
        &boot_domain,
        UINT64_C(0x746f6f72),
        0,
        domain,
        0,
        0,
        0,
        0,
        0,
        0
    );
    boot_domain.runtime_handle = 1;
    boot_domain.public_handle = 1;
    runtime_domain_registry_insert(&boot_domain);
    runtime_execution_domain = domain;
    runtime_state_publish_domain(domain, boot_domain.runtime_handle);
}

bool runtime_domain_register(
    RuntimeDomain *domain,
    uint8_t domain_id,
    uint8_t parent_domain,
    uint16_t runtime_handle
) {
    if (domain == 0 || domain_id == 0 ||
        domain_id > RUNTIME_DOMAIN_ID_MAX || runtime_handle == 0 ||
        runtime_domain_lookup(domain_id) != 0 ||
        (parent_domain != 0 && runtime_domain_lookup(parent_domain) == 0)) {
        return false;
    }
    runtime_domain_initialize_record(
        domain,
        0,
        0,
        domain_id,
        parent_domain,
        0,
        0,
        0,
        0,
        0
    );
    domain->runtime_handle = runtime_handle;
    runtime_domain_registry_insert(domain);
    return true;
}

void runtime_domain_register_allocation(
    uint32_t handle,
    uintptr_t physical_base,
    size_t size
) {
    if (handle == 0 ||
        !runtime_domain_range_is_valid(physical_base, size) ||
        (physical_base & (RUNTIME_DOMAIN_PAGE_SIZE - 1)) != 0 ||
        (size & (RUNTIME_DOMAIN_PAGE_SIZE - 1)) != 0 ||
        runtime_domain_allocation_lookup(handle) != 0) {
        runtime_domain_trap();
    }
    for (uint8_t index = 0;
         index < RUNTIME_DOMAIN_ALLOCATION_CAPACITY;
         ++index) {
        if (allocations[index].handle == 0) {
            allocations[index] = (RuntimeDomainAllocation){
                .handle = handle,
                .physical_base = physical_base,
                .size = size,
            };
            return;
        }
    }
    runtime_domain_trap();
}

RUNTIME_DOMAIN_RECOVERED_ENTRY
uint16_t runtime_domain_create_and_register(
    const RuntimeDomainDescriptor *descriptor,
    uintptr_t descriptor_lower_bound,
    uintptr_t descriptor_upper_bound,
    RuntimeFirebloomSpan *output_record,
    uintptr_t output_lower_bound,
    uintptr_t output_upper_bound
) {
    runtime_domain_validate_descriptor(
        descriptor,
        descriptor_lower_bound,
        descriptor_upper_bound
    );
    runtime_domain_validate_output(
        output_record,
        output_lower_bound,
        output_upper_bound
    );

    RuntimeDomainDefinition *definition = descriptor->definition.current;
    RuntimeDomainPhysicalMemory *physical_memory =
        descriptor->physical_memory.current;
    void *parent_object = runtime_capability_resolve(
        descriptor->parent_capability_handle,
        descriptor->parent_capability_type,
        descriptor->parent_capability_rights
    );
    const uint8_t domain_id = definition->domain_id;
    if (parent_object != definition || domain_id == 0 ||
        domain_id > RUNTIME_DOMAIN_ID_MAX ||
        descriptor->parent_domain == 0 ||
        runtime_domain_lookup(descriptor->parent_domain) == 0 ||
        descriptor->mapping_mode > 1 ||
        descriptor->capability_rights == 0 ||
        (descriptor->capability_rights & ~UINT8_C(7)) != 0 ||
        physical_memory->physical_upper_bound !=
            physical_memory->physical_base + physical_memory->size ||
        !runtime_domain_range_is_valid(
            physical_memory->physical_base,
            physical_memory->size
        ) ||
        physical_memory->size > RUNTIME_DOMAIN_PRIMARY_SIZE_MAX ||
        (physical_memory->physical_base &
            (RUNTIME_DOMAIN_PAGE_SIZE - 1)) != 0 ||
        (physical_memory->size & (RUNTIME_DOMAIN_PAGE_SIZE - 1)) != 0) {
        runtime_domain_trap();
    }

    const uint64_t service_key = runtime_domain_identity_read(
        (RuntimeFirebloomSpan){
            .current = (void *)&descriptor->identity,
            .lower_bound = (void *)descriptor,
            .upper_bound = (void *)(descriptor + 1),
            .type = &runtime_domain_identity_type,
        }
    ) & UINT64_C(0x00ffffffffffffff);
    const uintptr_t optional_lower =
        definition->optional_virtual_lower_bound;
    const uintptr_t optional_raw_upper =
        definition->optional_virtual_upper_bound;
    if (optional_raw_upper < optional_lower ||
        optional_raw_upper >
            UINTPTR_MAX - (RUNTIME_DOMAIN_PAGE_SIZE - 1)) {
        runtime_domain_trap();
    }
    const uintptr_t optional_upper = optional_raw_upper == optional_lower
        ? optional_lower
        : (optional_raw_upper + (RUNTIME_DOMAIN_PAGE_SIZE - 1)) &
            ~(uintptr_t)(RUNTIME_DOMAIN_PAGE_SIZE - 1);
    const size_t optional_size = optional_upper - optional_lower;
    uintptr_t optional_physical_base = 0;
    if (optional_size != 0) {
        const RuntimeDomainAllocation *allocation =
            runtime_domain_allocation_lookup(
                descriptor->optional_allocation_handle
            );
        if (allocation == 0 || optional_size > allocation->size ||
            optional_size > RUNTIME_DOMAIN_OPTIONAL_SIZE_MAX ||
            optional_lower < runtime_domain_optional_window_lower ||
            optional_upper >= runtime_domain_optional_window_upper ||
            (optional_lower &
                (RUNTIME_DOMAIN_OPTIONAL_ALIGNMENT - 1)) != 0) {
            runtime_domain_trap();
        }
        optional_physical_base = allocation->physical_base;
    }

    runtime_domain_validate_unique(
        service_key,
        domain_id,
        physical_memory->physical_base,
        physical_memory->size,
        optional_lower,
        optional_size
    );

    RuntimeDomain *domain = runtime_domain_dynamic_allocate();
    runtime_domain_initialize_record(
        domain,
        service_key,
        &descriptor->definition,
        domain_id,
        descriptor->parent_domain,
        physical_memory->physical_base,
        physical_memory->size,
        optional_physical_base,
        optional_lower,
        optional_upper
    );
    const uint16_t private_audience =
        runtime_domain_audience(domain_id) |
        runtime_domain_audience(descriptor->parent_domain);
    domain->runtime_handle = runtime_capability_publish(
        domain,
        &runtime_domain_record_type,
        RUNTIME_CAPABILITY_TYPE_DOMAIN,
        descriptor->capability_rights,
        private_audience
    );
    domain->public_handle = runtime_capability_publish(
        domain,
        &runtime_domain_record_type,
        RUNTIME_CAPABILITY_TYPE_DOMAIN,
        RUNTIME_CAPABILITY_RIGHT_WAIT,
        RUNTIME_CAPABILITY_AUDIENCE_ALL
    );
    const size_t mapping_index = (size_t)(domain - dynamic_domains);
    if (mapping_index >= RUNTIME_DYNAMIC_DOMAIN_CAPACITY) {
        runtime_domain_trap();
    }
    primary_mappings[mapping_index] = (RuntimeAddressSpaceMapping){
        .virtual_lower_bound = domain->primary_virtual_base,
        .virtual_upper_bound =
            domain->primary_virtual_base + domain->primary_size,
        .physical_base = domain->primary_physical_base,
        .audience = runtime_domain_audience(domain_id),
        .physical_region_id = (uint16_t)descriptor->mapping_region,
        .owner_domain = domain_id,
        .memory_attributes = (uint8_t)descriptor->mapping_mode,
    };
    domain->primary_mapping_handle = runtime_capability_publish(
        &primary_mappings[mapping_index],
        &runtime_memory_mapping_type,
        RUNTIME_CAPABILITY_TYPE_MEMORY_MAPPING,
        UINT8_C(7),
        runtime_domain_audience(domain_id)
    );
    if (domain->runtime_handle == 0 || domain->public_handle == 0 ||
        domain->primary_mapping_handle == 0) {
        runtime_domain_trap();
    }
    runtime_domain_registry_insert(domain);

    if (output_record != 0) {
        *output_record = (RuntimeFirebloomSpan){
            .current = domain,
            .lower_bound = domain,
            .upper_bound = domain + 1,
            .type = &runtime_domain_record_type,
        };
    }
    return domain->runtime_handle;
}

uint8_t runtime_current_domain(void) {
    return runtime_execution_domain;
}

void runtime_domain_set_current(const RuntimeDomain *domain) {
    if (domain == 0 || runtime_domain_lookup(domain->domain_id) != domain) {
        runtime_domain_trap();
    }
    runtime_execution_domain = domain->domain_id;
    runtime_state_publish_domain(domain->domain_id, domain->runtime_handle);
}

bool runtime_domain_resolve_locality(uint8_t requested_domain) {
    return runtime_execution_domain == requested_domain ||
        !runtime_domain_registry_contains(requested_domain);
}
