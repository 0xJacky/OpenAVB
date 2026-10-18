#include "runtime_bootstrap_memory_recovered_private.h"


static bool view_contains_domain(RuntimeDomainView view) {
    const uintptr_t current = (uintptr_t)view.current;
    const uintptr_t lower = (uintptr_t)view.lower_bound;
    const uintptr_t upper = (uintptr_t)view.upper_bound;
    return current != 0 && lower != 0 && upper != 0 && current >= lower &&
        current <= upper && sizeof(*view.current) <= upper - current;
}

void runtime_bootstrap_memory_map_domain_selector_page(void) {
    RuntimeBootstrapMemoryRecoveredConfiguration configuration =
        runtime_bootstrap_memory_recovered_configuration_snapshot();
    if (configuration.domain_selector_offset == 0) {
        return;
    }
    if (configuration.domain_selector_capacity == 0) {
        runtime_bootstrap_memory_domain_selector_config_failure();
    }
    if (configuration.domain_selector_base >
        UINTPTR_MAX - configuration.domain_selector_offset) {
        runtime_bootstrap_memory_domain_selector_config_failure();
    }
    const uintptr_t address = configuration.domain_selector_base +
        configuration.domain_selector_offset;
    if ((address & (RUNTIME_BOOTSTRAP_MEMORY_PAGE_SIZE - 1)) != 0) {
        return;
    }
    if (!runtime_bootstrap_memory_recovered_map(
            (RuntimeBootstrapMemoryMappingRequest){
                .virtual_address = address,
                .physical_address = address,
                .size = RUNTIME_BOOTSTRAP_MEMORY_PAGE_SIZE,
                .regime = 1,
                .memory_attribute = 4,
                .flags = RUNTIME_PAGE_ATTRIBUTE_STATIC_CLAIM,
            })) {
        runtime_bootstrap_memory_domain_selector_config_failure();
    }
}

_Noreturn void runtime_bootstrap_memory_domain_selector_config_failure(void) {
    runtime_bootstrap_memory_recovered_panic();
}

static void publish_domain(
    RuntimeDomainView domain,
    RuntimeBootstrapMemoryRecoveredConfiguration configuration
) {
    if (configuration.publish_domain != 0) {
        configuration.publish_domain(domain, configuration.context);
        return;
    }
#if defined(__aarch64__) && defined(__ELF__)
    runtime_current_domain_metadata_publish(domain);
#else
    runtime_bootstrap_memory_recovered_panic();
#endif
}

static void clear_domain_prefix(
    uintptr_t lower_bound,
    uintptr_t upper_bound,
    RuntimeBootstrapMemoryRecoveredConfiguration configuration
) {
    if (configuration.clear_range != 0) {
        configuration.clear_range(
            lower_bound,
            upper_bound,
            configuration.context
        );
        return;
    }
#if defined(__aarch64__) && defined(__ELF__)
    uint8_t *bytes = (uint8_t *)lower_bound;
    for (size_t index = 0; index < upper_bound - lower_bound; ++index) {
        bytes[index] = 0;
    }
#else
    runtime_bootstrap_memory_recovered_panic();
#endif
}

void runtime_bootstrap_memory_activate_current_domain(void) {
    RuntimeBootstrapMemoryRecoveredConfiguration configuration =
        runtime_bootstrap_memory_recovered_configuration_snapshot();
    RuntimeDomainView domain_view = configuration.current_domain;
    if (!view_contains_domain(domain_view)) {
        runtime_bootstrap_memory_recovered_panic();
    }
    RuntimeDomain *domain = domain_view.current;
    if (domain->primary_physical_base == 0 ||
        domain->primary_virtual_base == 0 || domain->primary_size == 0 ||
        domain->primary_physical_base > UINTPTR_MAX - domain->primary_size ||
        domain->primary_virtual_base > UINTPTR_MAX - domain->primary_size) {
        runtime_bootstrap_memory_recovered_panic();
    }
    if (!runtime_bootstrap_memory_recovered_map(
            (RuntimeBootstrapMemoryMappingRequest){
                .virtual_address = domain->primary_virtual_base,
                .physical_address = domain->primary_physical_base,
                .size = domain->primary_size,
                .regime = 0,
                .memory_attribute = 0,
                .flags = 1,
            })) {
        runtime_bootstrap_memory_recovered_panic();
    }
    publish_domain(domain_view, configuration);
    if (domain->primary_reserved != 0) {
        if (domain->primary_reserved > domain->primary_size) {
            runtime_bootstrap_memory_recovered_panic();
        }
        clear_domain_prefix(
            domain->primary_virtual_base,
            domain->primary_virtual_base + domain->primary_reserved,
            configuration
        );
    }
}

void runtime_bootstrap_memory_reserve_task_shared_page_storage(
    RuntimeTask *task,
    uintptr_t task_lower_bound,
    uintptr_t task_upper_bound
) {
    const uintptr_t task_address = (uintptr_t)task;
    if (task_address == 0 || task_address < task_lower_bound ||
        task_address > task_upper_bound ||
        sizeof(*task) > task_upper_bound - task_address ||
        task->lifecycle_aux != 0 || task->join_target != 0) {
        runtime_bootstrap_memory_recovered_panic();
    }
    RuntimeBootstrapMemoryRecoveredConfiguration configuration =
        runtime_bootstrap_memory_recovered_configuration_snapshot();
    const uintptr_t cursor = *configuration.shared_storage_cursor;
    if (cursor == 0) {
        return;
    }
    if (cursor > UINTPTR_MAX -
            RUNTIME_BOOTSTRAP_MEMORY_SHARED_STORAGE_SIZE) {
        runtime_bootstrap_memory_recovered_panic();
    }
    const uintptr_t next = cursor +
        RUNTIME_BOOTSTRAP_MEMORY_SHARED_STORAGE_SIZE;
    if (next > configuration.shared_storage_upper_bound) {
        runtime_bootstrap_memory_recovered_panic();
    }
    *configuration.shared_storage_cursor = next;
    task->lifecycle_aux = (void *)cursor;
}

void runtime_bootstrap_memory_prepare_fixed_profile(uint32_t selector) {
    if (selector != 0) {
        runtime_bootstrap_memory_recovered_panic();
    }
    RuntimeBootstrapMemoryRecoveredConfiguration configuration =
        runtime_bootstrap_memory_recovered_configuration_snapshot();
    if (configuration.initialize_base != 0) {
        configuration.initialize_base(configuration.context);
    }
    if (!runtime_bootstrap_memory_recovered_map(
            (RuntimeBootstrapMemoryMappingRequest){
                .virtual_address = configuration.platform_boot_base,
                .physical_address = configuration.platform_boot_base,
                .size = platform_boot_memory_size(),
                .regime = 0,
                .memory_attribute = 0,
                .flags = 1,
            })) {
        runtime_bootstrap_memory_recovered_panic();
    }
    runtime_bootstrap_memory_publish_region_capabilities(
        configuration.fixed_descriptor,
        configuration.fixed_descriptor_size
    );
}

static RuntimePhysicalMemoryRegion *fixed_region(uint16_t region_id) {
    RuntimeFirebloomSpan span =
        runtime_bootstrap_memory_recovered_lookup_region(region_id);
    const uintptr_t current = (uintptr_t)span.current;
    const uintptr_t lower = (uintptr_t)span.lower_bound;
    const uintptr_t upper = (uintptr_t)span.upper_bound;
    if (current == 0 || lower == 0 || upper == 0 || current < lower ||
        current > upper || sizeof(RuntimePhysicalMemoryRegion) > upper - current) {
        runtime_bootstrap_memory_recovered_panic();
    }
    return span.current;
}

static bool span_is_present(RuntimeMemorySpan span) {
    return span.lower_bound != 0 && span.upper_bound > span.lower_bound;
}

void runtime_bootstrap_memory_activate_fixed_profile(void) {
    RuntimeBootstrapMemoryRecoveredConfiguration configuration =
        runtime_bootstrap_memory_recovered_configuration_snapshot();
    if (*configuration.active_flag != 0) {
        runtime_bootstrap_memory_recovered_panic();
    }
    RuntimePhysicalMemoryRegion *root =
        fixed_region(configuration.fixed_root_region_id);
    RuntimePhysicalMemoryRegion *auxiliary =
        fixed_region(configuration.fixed_auxiliary_region_id);
    if (auxiliary->active_alias == 0) {
        auxiliary->descriptor =
            (auxiliary->descriptor & ~UINT64_C(0xffff0000)) |
            ((uint64_t)RUNTIME_BOOTSTRAP_MEMORY_CAPABILITY_TYPE << 16);
    }
    runtime_bootstrap_memory_prepare_fixed_profile(0);
    runtime_bootstrap_memory_publish_region_alias(
        configuration.fixed_root_region_id,
        root->physical_base,
        root->physical_base,
        root->size
    );
    root->descriptor = (root->descriptor & ~UINT64_C(0xffff0000)) |
        (UINT64_C(9) << 16);
    if (span_is_present(configuration.fixed_identity_range) &&
        !runtime_bootstrap_memory_recovered_map(
            (RuntimeBootstrapMemoryMappingRequest){
                .virtual_address =
                    configuration.fixed_identity_range.lower_bound,
                .physical_address =
                    configuration.fixed_identity_range.lower_bound,
                .size = configuration.fixed_identity_range.upper_bound -
                    configuration.fixed_identity_range.lower_bound,
                .regime = 0,
                .memory_attribute = 0,
                .flags = RUNTIME_PAGE_ATTRIBUTE_STATIC_CLAIM,
            })) {
        runtime_bootstrap_memory_recovered_panic();
    }
    if (configuration.fixed_secondary_mapping.size != 0 &&
        !runtime_bootstrap_memory_recovered_map(
            configuration.fixed_secondary_mapping)) {
        runtime_bootstrap_memory_recovered_panic();
    }
    *configuration.active_flag = 1;
}
