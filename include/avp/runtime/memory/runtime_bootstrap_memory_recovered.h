#pragma once

#include "avp/runtime/memory/runtime_bootstrap_memory.h"
#include "avp/runtime/ipc/runtime_domain_recovered.h"
#include "avp/runtime/firebloom/runtime_firebloom.h"
#include "avp/runtime/memory/runtime_page_attributes_recovered.h"
#include "avp/runtime/memory/runtime_physical_memory.h"
#include "avp/runtime/task/runtime_task.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


#if __has_attribute(retain)
#define RUNTIME_BOOTSTRAP_MEMORY_RECOVERED_PUBLIC \
    __attribute__((used, retain))
#else
#define RUNTIME_BOOTSTRAP_MEMORY_RECOVERED_PUBLIC __attribute__((used))
#endif

enum {
    RUNTIME_BOOTSTRAP_MEMORY_PLATFORM_SIZE = 0x38c000,
    RUNTIME_BOOTSTRAP_MEMORY_SHARED_STORAGE_SIZE = 0x8000,
    RUNTIME_BOOTSTRAP_MEMORY_CAPABILITY_RECORD_SIZE = 0x40,
    RUNTIME_BOOTSTRAP_MEMORY_CAPABILITY_TYPE = 7,
    RUNTIME_BOOTSTRAP_MEMORY_CAPABILITY_RIGHTS = 3,
    RUNTIME_BOOTSTRAP_MEMORY_SKIPPED_REGION_ID = 0x00ab,
};

typedef struct {
    uint8_t *current;
    uint8_t *lower_bound;
    uint8_t *upper_bound;
} RuntimeBootstrapMemoryByteView;

typedef struct {
    uint32_t *current;
    uint32_t *lower_bound;
    uint32_t *upper_bound;
} RuntimeBootstrapMemoryWordView;

typedef struct {
    uintptr_t virtual_address;
    uintptr_t physical_address;
    size_t size;
    uint8_t regime;
    uint8_t memory_attribute;
    uint32_t flags;
} RuntimeBootstrapMemoryMappingRequest;

typedef struct {
    uintptr_t lower_bound;
    uintptr_t upper_bound;
    uint16_t region_id;
    uint16_t capability_handle;
    bool enabled;
} RuntimeBootstrapMemorySharedMapping;

typedef struct {
    uintptr_t lower_bound;
    uintptr_t upper_bound;
    uintptr_t physical_base;
    uint16_t region_id;
    uint16_t capability_handle;
} RuntimeBootstrapMemoryCapabilityMetadata;

_Static_assert(
    sizeof(RuntimeBootstrapMemoryCapabilityMetadata) == 0x20,
    "bootstrap capability metadata size"
);

typedef struct {
    uintptr_t physical_base;
    uintptr_t active_alias;
    size_t size;
    uint16_t region_id;
    uint16_t policy_region_id;
    uint8_t flags;
    uint8_t reserved_1d;
    uint16_t capability_handle;
    RuntimeFirebloomSpan metadata;
} RuntimeBootstrapMemoryRegionRecord;

_Static_assert(
    sizeof(RuntimeBootstrapMemoryRegionRecord) ==
        RUNTIME_BOOTSTRAP_MEMORY_CAPABILITY_RECORD_SIZE,
    "bootstrap region record size"
);
_Static_assert(
    offsetof(RuntimeBootstrapMemoryRegionRecord, region_id) == 0x18,
    "bootstrap region id offset"
);
_Static_assert(
    offsetof(RuntimeBootstrapMemoryRegionRecord, capability_handle) == 0x1e,
    "bootstrap capability handle offset"
);

typedef struct {
    RuntimeBootstrapMemoryBitmap *bitmap;
    RuntimeBootstrapMemoryBitmap *lower_bound;
    RuntimeBootstrapMemoryBitmap *upper_bound;
    uint16_t region_id;
} RuntimeBootstrapMemoryBitmapArguments;

typedef RuntimeFirebloomSpan (*RuntimeBootstrapMemoryLookupRegion)(
    uint16_t region_id,
    void *context
);
typedef bool (*RuntimeBootstrapMemoryMapRange)(
    const RuntimeBootstrapMemoryMappingRequest *request,
    void *context
);
typedef bool (*RuntimeBootstrapMemoryRangeReserved)(
    uintptr_t physical_base,
    size_t size,
    uint16_t region_id,
    void *context
);
typedef bool (*RuntimeBootstrapMemoryRegionPredicate)(
    uint16_t region_id,
    void *context
);
typedef uint32_t (*RuntimeBootstrapMemoryAttributeMask)(
    RuntimeMemorySpan range,
    void *context
);
typedef bool (*RuntimeBootstrapMemoryClaimAttributes)(
    RuntimeMemorySpan range,
    uint32_t attribute_flags,
    uint32_t options,
    void *context
);
typedef uint16_t (*RuntimeBootstrapMemoryPublishCapability)(
    uint8_t type,
    uint16_t region_id,
    uint8_t rights,
    const RuntimeBootstrapMemoryCapabilityMetadata *metadata,
    void *context
);
typedef void (*RuntimeBootstrapMemoryPublishDomain)(
    RuntimeDomainView domain,
    void *context
);
typedef void (*RuntimeBootstrapMemoryClearRange)(
    uintptr_t lower_bound,
    uintptr_t upper_bound,
    void *context
);
typedef void (*RuntimeBootstrapMemoryHook)(void *context);
typedef bool (*RuntimeBootstrapMemoryBootOwner)(void *context);

typedef struct {
    RuntimeBootstrapMemoryLookupRegion lookup_region;
    RuntimeBootstrapMemoryMapRange map_range;
    RuntimeBootstrapMemoryRangeReserved range_is_reserved;
    RuntimeBootstrapMemoryRegionPredicate shared_region_is_forbidden;
    RuntimeBootstrapMemoryAttributeMask attribute_mask;
    RuntimeBootstrapMemoryClaimAttributes claim_attributes;
    RuntimeBootstrapMemoryPublishCapability publish_capability;
    RuntimeBootstrapMemoryPublishDomain publish_domain;
    RuntimeBootstrapMemoryClearRange clear_range;
    RuntimeBootstrapMemoryHook initialize_base;
    RuntimeBootstrapMemoryBootOwner is_boot_owner;
    RuntimeBootstrapMemoryCapabilityMetadata *capability_metadata;
    size_t capability_metadata_capacity;
    RuntimePageAttributeRegistry *page_attribute_registry;
    RuntimeDomainView current_domain;
    const RuntimeBootstrapMemorySharedMapping *shared_mappings;
    size_t shared_mapping_count;
    uint16_t current_primary_mapping_handle;
    uintptr_t *shared_storage_cursor;
    uintptr_t shared_storage_upper_bound;
    uintptr_t domain_selector_base;
    uintptr_t domain_selector_offset;
    size_t domain_selector_capacity;
    uintptr_t platform_boot_base;
    const RuntimeBootstrapMemoryDescriptor *fixed_descriptor;
    size_t fixed_descriptor_size;
    uint16_t fixed_root_region_id;
    uint16_t fixed_auxiliary_region_id;
    RuntimeMemorySpan fixed_identity_range;
    RuntimeBootstrapMemoryMappingRequest fixed_secondary_mapping;
    bool reset_entry_present;
    uint8_t *complete_flag;
    uint8_t *active_flag;
    uint8_t *sealed_flag;
    uint8_t *storage_alias_flag;
    void *context;
} RuntimeBootstrapMemoryRecoveredConfiguration;

void runtime_bootstrap_memory_recovered_configure(
    const RuntimeBootstrapMemoryRecoveredConfiguration *configuration
);
void runtime_bootstrap_memory_recovered_reset(void);

RUNTIME_BOOTSTRAP_MEMORY_RECOVERED_PUBLIC size_t platform_boot_memory_size(void);
RUNTIME_BOOTSTRAP_MEMORY_RECOVERED_PUBLIC size_t
j_platform_boot_memory_size(void);
RUNTIME_BOOTSTRAP_MEMORY_RECOVERED_PUBLIC void
runtime_bootstrap_memory_prepare_fixed_profile(uint32_t selector);
RUNTIME_BOOTSTRAP_MEMORY_RECOVERED_PUBLIC void
runtime_bootstrap_memory_platform_hook_noop(void);
RUNTIME_BOOTSTRAP_MEMORY_RECOVERED_PUBLIC void
runtime_bootstrap_memory_publish_region_hook_noop(void);
RUNTIME_BOOTSTRAP_MEMORY_RECOVERED_PUBLIC RuntimeFirebloomSpan
runtime_bootstrap_outlined_clear_u32_and_return_null_span(
    uint32_t *current,
    uint32_t *lower_bound,
    uint32_t *upper_bound
);
RUNTIME_BOOTSTRAP_MEMORY_RECOVERED_PUBLIC _Noreturn void
runtime_bootstrap_clear_u32_bounds_panic(void);
RUNTIME_BOOTSTRAP_MEMORY_RECOVERED_PUBLIC RuntimeBootstrapMemoryByteView
runtime_bootstrap_complete_flag_bounded_view(void);
RUNTIME_BOOTSTRAP_MEMORY_RECOVERED_PUBLIC RuntimeBootstrapMemoryByteView
runtime_bootstrap_memory_active_flag_bounded_view(void);
RUNTIME_BOOTSTRAP_MEMORY_RECOVERED_PUBLIC void
runtime_bootstrap_memory_claim_shared_mapping_attributes(void);
RUNTIME_BOOTSTRAP_MEMORY_RECOVERED_PUBLIC _Noreturn void
runtime_bootstrap_memory_bitmap_test_bounds_panic(void);
RUNTIME_BOOTSTRAP_MEMORY_RECOVERED_PUBLIC RuntimeBootstrapMemoryByteView
runtime_bootstrap_memory_sealed_flag_bounded_view(void);
RUNTIME_BOOTSTRAP_MEMORY_RECOVERED_PUBLIC void
runtime_bootstrap_memory_map_identity_region(
    uint16_t region_id,
    uintptr_t physical_base,
    size_t size
);
RUNTIME_BOOTSTRAP_MEMORY_RECOVERED_PUBLIC void
runtime_bootstrap_memory_publish_region_alias(
    uint16_t region_id,
    uintptr_t active_alias,
    uintptr_t physical_base,
    size_t size
);
RUNTIME_BOOTSTRAP_MEMORY_RECOVERED_PUBLIC RuntimeBootstrapMemoryByteView
runtime_bootstrap_memory_storage_alias_flag_bounded_view(void);
RUNTIME_BOOTSTRAP_MEMORY_RECOVERED_PUBLIC _Noreturn void
runtime_bootstrap_memory_bitmap_set_bounds_panic(void);
RUNTIME_BOOTSTRAP_MEMORY_RECOVERED_PUBLIC void
runtime_bootstrap_memory_map_domain_selector_page(void);
RUNTIME_BOOTSTRAP_MEMORY_RECOVERED_PUBLIC _Noreturn void
runtime_bootstrap_memory_domain_selector_config_failure(void);
RUNTIME_BOOTSTRAP_MEMORY_RECOVERED_PUBLIC void
runtime_bootstrap_memory_activate_current_domain(void);
RUNTIME_BOOTSTRAP_MEMORY_RECOVERED_PUBLIC void
runtime_bootstrap_memory_reserve_task_shared_page_storage(
    RuntimeTask *task,
    uintptr_t task_lower_bound,
    uintptr_t task_upper_bound
);
RUNTIME_BOOTSTRAP_MEMORY_RECOVERED_PUBLIC void
runtime_bootstrap_memory_publish_region_capabilities(
    const RuntimeBootstrapMemoryDescriptor *descriptor,
    size_t descriptor_size
);
RUNTIME_BOOTSTRAP_MEMORY_RECOVERED_PUBLIC void
runtime_bootstrap_memory_activate_fixed_profile(void);
RUNTIME_BOOTSTRAP_MEMORY_RECOVERED_PUBLIC RuntimeBootstrapMemoryByteView
runtime_bootstrap_complete_flag_prepare_result_tuple(void);
RUNTIME_BOOTSTRAP_MEMORY_RECOVERED_PUBLIC RuntimeBootstrapMemoryBitmapArguments
runtime_bootstrap_memory_outlined_prepare_region_bitmap_arguments(
    uint16_t region_id
);
