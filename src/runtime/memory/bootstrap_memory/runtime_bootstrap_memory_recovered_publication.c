#include "runtime_bootstrap_memory_recovered_private.h"

#include "avp/runtime/memory/runtime_memory_policy_recovered.h"


static bool address_has_bytes(
    const void *current,
    const void *lower_bound,
    const void *upper_bound,
    size_t size
) {
    const uintptr_t current_address = (uintptr_t)current;
    const uintptr_t lower_address = (uintptr_t)lower_bound;
    const uintptr_t upper_address = (uintptr_t)upper_bound;
    return current_address != 0 && lower_address != 0 && upper_address != 0 &&
        current_address >= lower_address && current_address <= upper_address &&
        size <= upper_address - current_address;
}

static RuntimePhysicalMemoryRegion *require_region(uint16_t region_id) {
    RuntimeFirebloomSpan span =
        runtime_bootstrap_memory_recovered_lookup_region(region_id);
    if (!address_has_bytes(
            span.current,
            span.lower_bound,
            span.upper_bound,
            sizeof(RuntimePhysicalMemoryRegion))) {
        runtime_bootstrap_memory_recovered_panic();
    }
    RuntimePhysicalMemoryRegion *region = span.current;
    if (region->physical_base == 0 || region->size == 0 ||
        region->physical_base > UINTPTR_MAX - region->size) {
        runtime_bootstrap_memory_recovered_panic();
    }
    return region;
}

void runtime_bootstrap_memory_map_identity_region(
    uint16_t region_id,
    uintptr_t physical_base,
    size_t size
) {
    RuntimeBootstrapMemoryRecoveredConfiguration configuration =
        runtime_bootstrap_memory_recovered_configuration_snapshot();
    if (physical_base == 0 || size == 0 ||
        physical_base > UINTPTR_MAX - size) {
        runtime_bootstrap_memory_recovered_panic();
    }
    if (!configuration.reset_entry_present) {
        RuntimePhysicalMemoryRegion *region = require_region(region_id);
        if (region->physical_base != physical_base || region->size != size) {
            runtime_bootstrap_memory_recovered_panic();
        }
        region->active_alias = physical_base;
    }
    if (!runtime_bootstrap_memory_recovered_map(
            (RuntimeBootstrapMemoryMappingRequest){
                .virtual_address = physical_base,
                .physical_address = physical_base,
                .size = size,
                .regime = 0,
                .memory_attribute = 0,
                .flags = RUNTIME_PAGE_ATTRIBUTE_STATIC_CLAIM,
            })) {
        runtime_bootstrap_memory_recovered_panic();
    }
}

void runtime_bootstrap_memory_publish_region_alias(
    uint16_t region_id,
    uintptr_t active_alias,
    uintptr_t physical_base,
    size_t size
) {
    RuntimeBootstrapMemoryRecoveredConfiguration configuration =
        runtime_bootstrap_memory_recovered_configuration_snapshot();
    if (active_alias == 0 || physical_base == 0 || size == 0 ||
        active_alias > UINTPTR_MAX - size ||
        physical_base > UINTPTR_MAX - size) {
        runtime_bootstrap_memory_recovered_panic();
    }
    if (!configuration.reset_entry_present) {
        RuntimePhysicalMemoryRegion *region = require_region(region_id);
        if (region->physical_base != physical_base || region->size != size) {
            runtime_bootstrap_memory_recovered_panic();
        }
        region->active_alias = active_alias;
    }
    if (!runtime_bootstrap_memory_recovered_map(
            (RuntimeBootstrapMemoryMappingRequest){
                .virtual_address = active_alias,
                .physical_address = physical_base,
                .size = size,
                .regime = 0,
                .memory_attribute = 0,
                .flags = RUNTIME_PAGE_ATTRIBUTE_STATIC_CLAIM,
            })) {
        runtime_bootstrap_memory_recovered_panic();
    }
    *configuration.storage_alias_flag = 1;
}

static uint32_t shared_attribute_mask(
    RuntimeMemorySpan range,
    RuntimeBootstrapMemoryRecoveredConfiguration configuration
) {
    if (configuration.attribute_mask != 0) {
        return configuration.attribute_mask(range, configuration.context);
    }
#if defined(__aarch64__) && defined(__ELF__)
    if (configuration.page_attribute_registry == 0) {
        runtime_bootstrap_memory_recovered_panic();
    }
    RuntimePageAttributePageSpan pages = {
        .begin = (uint32_t)(range.lower_bound >>
            RUNTIME_PAGE_ATTRIBUTE_PAGE_SHIFT),
        .end = (uint32_t)((range.upper_bound +
            RUNTIME_PAGE_ATTRIBUTE_PAGE_SIZE - 1) >>
            RUNTIME_PAGE_ATTRIBUTE_PAGE_SHIFT),
    };
    RuntimePageAttributeLookupResult lookup =
        runtime_page_attribute_registry_lookup(
            configuration.page_attribute_registry,
            pages
        );
    return runtime_memory_range_page_attribute_mask(
        configuration.page_attribute_registry,
        lookup,
        (RuntimePageAttributeMemoryRange){
            .base = range.lower_bound,
            .size = range.upper_bound - range.lower_bound,
        }
    );
#else
    runtime_bootstrap_memory_recovered_panic();
#endif
}

static bool shared_region_is_forbidden(
    uint16_t region_id,
    RuntimeBootstrapMemoryRecoveredConfiguration configuration
) {
    if (configuration.shared_region_is_forbidden != 0) {
        return configuration.shared_region_is_forbidden(
            region_id,
            configuration.context
        );
    }
#if defined(__aarch64__) && defined(__ELF__)
    return runtime_memory_policy_set_b_contains_region_id(region_id);
#else
    runtime_bootstrap_memory_recovered_panic();
#endif
}

static bool claim_shared_range(
    RuntimeMemorySpan range,
    RuntimeBootstrapMemoryRecoveredConfiguration configuration
) {
    if (configuration.claim_attributes != 0) {
        return configuration.claim_attributes(
            range,
            RUNTIME_PAGE_ATTRIBUTE_PRIMARY_SET,
            1,
            configuration.context
        );
    }
#if defined(__aarch64__) && defined(__ELF__)
    if (configuration.page_attribute_registry == 0) {
        runtime_bootstrap_memory_recovered_panic();
    }
    RuntimePageAttributePageSpan pages = {
        .begin = (uint32_t)(range.lower_bound >>
            RUNTIME_PAGE_ATTRIBUTE_PAGE_SHIFT),
        .end = (uint32_t)((range.upper_bound +
            RUNTIME_PAGE_ATTRIBUTE_PAGE_SIZE - 1) >>
            RUNTIME_PAGE_ATTRIBUTE_PAGE_SHIFT),
    };
    RuntimePageAttributeLookupResult lookup =
        runtime_page_attribute_registry_lookup(
            configuration.page_attribute_registry,
            pages
        );
    return runtime_memory_range_attribute_claim(
        configuration.page_attribute_registry,
        lookup,
        (RuntimePageAttributeMemoryRange){
            .base = range.lower_bound,
            .size = range.upper_bound - range.lower_bound,
        },
        RUNTIME_PAGE_ATTRIBUTE_PRIMARY_SET,
        1
    );
#else
    runtime_bootstrap_memory_recovered_panic();
#endif
}

void runtime_bootstrap_memory_claim_shared_mapping_attributes(void) {
    RuntimeBootstrapMemoryRecoveredConfiguration configuration =
        runtime_bootstrap_memory_recovered_configuration_snapshot();
    if (configuration.page_attribute_registry != 0 &&
        (!configuration.page_attribute_registry->initialized ||
         configuration.page_attribute_registry->policy_active)) {
        runtime_bootstrap_memory_recovered_panic();
    }
    for (size_t remaining = configuration.shared_mapping_count;
         remaining != 0;
         --remaining) {
        const RuntimeBootstrapMemorySharedMapping *mapping =
            &configuration.shared_mappings[remaining - 1];
        if (!mapping->enabled ||
            mapping->capability_handle ==
                configuration.current_primary_mapping_handle) {
            continue;
        }
        if (mapping->lower_bound == 0 ||
            mapping->upper_bound <= mapping->lower_bound) {
            runtime_bootstrap_memory_recovered_panic();
        }
        const RuntimeMemorySpan range = {
            mapping->lower_bound,
            mapping->upper_bound,
        };
        if (shared_attribute_mask(range, configuration) != 0) {
            continue;
        }
        if (shared_region_is_forbidden(mapping->region_id, configuration) ||
            !claim_shared_range(range, configuration)) {
            runtime_bootstrap_memory_recovered_panic();
        }
    }
}

static bool region_range_is_reserved(
    const RuntimeBootstrapMemoryRegionRecord *record,
    RuntimeBootstrapMemoryRecoveredConfiguration configuration
) {
    if (configuration.range_is_reserved != 0) {
        return configuration.range_is_reserved(
            record->physical_base,
            record->size,
            record->region_id,
            configuration.context
        );
    }
#if defined(__aarch64__) && defined(__ELF__)
    return runtime_physical_memory_range_is_reserved(
        record->physical_base,
        record->size,
        record->region_id
    );
#else
    runtime_bootstrap_memory_recovered_panic();
#endif
}

void runtime_bootstrap_memory_publish_region_capabilities(
    const RuntimeBootstrapMemoryDescriptor *descriptor,
    size_t descriptor_size
) {
    RuntimeBootstrapMemoryRecoveredConfiguration configuration =
        runtime_bootstrap_memory_recovered_configuration_snapshot();
    if (*configuration.complete_flag != 0 ||
        !runtime_bootstrap_memory_recovered_boot_owner()) {
        runtime_bootstrap_memory_recovered_panic();
    }
    runtime_bootstrap_memory_publish_region_hook_noop();
    *configuration.complete_flag = 1;
    if (descriptor == 0 && descriptor_size == 0) {
        return;
    }
    if (descriptor == 0 ||
        descriptor_size != RUNTIME_BOOTSTRAP_MEMORY_DESCRIPTOR_SIZE ||
        descriptor->bitmap_records.base == 0 ||
        descriptor->bitmap_records.size <
            RUNTIME_BOOTSTRAP_MEMORY_CAPABILITY_RECORD_SIZE ||
        descriptor->bitmap_records.size %
            RUNTIME_BOOTSTRAP_MEMORY_CAPABILITY_RECORD_SIZE != 0) {
        runtime_bootstrap_memory_recovered_panic();
    }
    RuntimeBootstrapMemoryRegionRecord *records =
        (RuntimeBootstrapMemoryRegionRecord *)descriptor->bitmap_records.base;
    const size_t count = descriptor->bitmap_records.size /
        sizeof(*records);
    if (count > configuration.capability_metadata_capacity ||
        configuration.capability_metadata == 0 ||
        configuration.publish_capability == 0) {
        runtime_bootstrap_memory_recovered_panic();
    }
    size_t capability_metadata_count = 0;
    for (size_t index = 0; index < count; ++index) {
        RuntimeBootstrapMemoryRegionRecord *record = &records[index];
        if (record->region_id == RUNTIME_BOOTSTRAP_MEMORY_SKIPPED_REGION_ID ||
            !region_range_is_reserved(record, configuration)) {
            continue;
        }
        if (record->physical_base == 0 || record->size == 0 ||
            record->physical_base > UINTPTR_MAX - record->size ||
            record->region_id == 0 ||
            record->region_id > RUNTIME_PHYSICAL_REGION_ID_MAX ||
            record->capability_handle != 0) {
            runtime_bootstrap_memory_recovered_panic();
        }
        RuntimeBootstrapMemoryCapabilityMetadata *metadata =
            &configuration
                .capability_metadata[capability_metadata_count++];
        *metadata = (RuntimeBootstrapMemoryCapabilityMetadata){
            .lower_bound = record->physical_base,
            .upper_bound = record->physical_base + record->size,
            .physical_base = record->physical_base,
            .region_id = record->region_id,
        };
        const uint16_t handle = configuration.publish_capability(
            RUNTIME_BOOTSTRAP_MEMORY_CAPABILITY_TYPE,
            record->region_id,
            RUNTIME_BOOTSTRAP_MEMORY_CAPABILITY_RIGHTS,
            metadata,
            configuration.context
        );
        if (handle == 0) {
            runtime_bootstrap_memory_recovered_panic();
        }
        metadata->capability_handle = handle;
        record->capability_handle = handle;
        record->metadata = (RuntimeFirebloomSpan){
            .current = metadata,
            .lower_bound = metadata,
            .upper_bound = metadata + 1,
        };
        RuntimeBootstrapMemoryBitmapArguments arguments =
            runtime_bootstrap_memory_outlined_prepare_region_bitmap_arguments(
                record->region_id);
        if (arguments.region_id > RUNTIME_BOOTSTRAP_MEMORY_PAGE_ID_MAX) {
            runtime_bootstrap_memory_bitmap_set_bounds_panic();
        }
        arguments.bitmap->words[arguments.region_id >> 6] |=
            UINT64_C(1) << (arguments.region_id & 63);
    }
}
