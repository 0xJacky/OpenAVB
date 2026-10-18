#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "avp/runtime/firebloom/runtime_firebloom.h"
#include "avp/runtime/memory/runtime_page_table.h"


enum {
    RUNTIME_ADDRESS_SPACE_PAGE_SIZE = RUNTIME_PAGE_TABLE_PAGE_SIZE,
    RUNTIME_ADDRESS_SPACE_MAPPING_FLAG_SHARED = 0x01,
    RUNTIME_ADDRESS_SPACE_MAPPING_FLAG_PHYSICAL_ALIAS = 0x02,
    RUNTIME_ADDRESS_SPACE_MAPPING_FLAG_TRANSIENT = 0x08,
    RUNTIME_ADDRESS_SPACE_MAPPING_FLAG_GLOBAL = 0x20,
    RUNTIME_ADDRESS_SPACE_MAPPING_FLAG_DEVICE = 0x40,
    RUNTIME_ADDRESS_SPACE_MAPPING_FLAGS_ALLOWED =
        RUNTIME_ADDRESS_SPACE_MAPPING_FLAG_SHARED |
        RUNTIME_ADDRESS_SPACE_MAPPING_FLAG_PHYSICAL_ALIAS |
        RUNTIME_ADDRESS_SPACE_MAPPING_FLAG_TRANSIENT |
        RUNTIME_ADDRESS_SPACE_MAPPING_FLAG_GLOBAL |
        RUNTIME_ADDRESS_SPACE_MAPPING_FLAG_DEVICE,
};

typedef struct {
    uintptr_t virtual_lower_bound;
    uintptr_t virtual_upper_bound;
    uintptr_t physical_base;
    uint16_t audience;
    uint16_t physical_region_id;
    uint8_t owner_domain;
    uint8_t memory_attributes;
    uint8_t mapping_attributes;
    uint8_t flags;
} RuntimeAddressSpaceMapping;

_Static_assert(
    sizeof(RuntimeAddressSpaceMapping) == 0x20,
    "RuntimeAddressSpaceMapping size"
);
_Static_assert(
    offsetof(RuntimeAddressSpaceMapping, audience) == 0x18,
    "address-space audience offset"
);
_Static_assert(
    offsetof(RuntimeAddressSpaceMapping, physical_region_id) == 0x1a,
    "address-space region offset"
);
_Static_assert(
    offsetof(RuntimeAddressSpaceMapping, owner_domain) == 0x1c,
    "address-space owner offset"
);
_Static_assert(
    offsetof(RuntimeAddressSpaceMapping, flags) == 0x1f,
    "address-space flags offset"
);

typedef struct {
    uintptr_t virtual_lower_bound;
    size_t size;
    uintptr_t physical_base;
    uint16_t audience;
    uint16_t physical_region_id;
    uint8_t owner_domain;
    uint8_t memory_attributes;
    uint8_t mapping_attributes;
    uint8_t flags;
    uint8_t rights;
} RuntimeAddressSpaceMapRequest;

typedef struct {
    uint16_t mapping_handle;
    uintptr_t virtual_lower_bound;
} RuntimeAddressSpaceMapResult;

typedef struct {
    uintptr_t low_direct_upper_bound;
    uintptr_t high_direct_lower_bound;
    uint16_t special_direct_region_id;
} RuntimeAddressSpaceProfile;

void runtime_address_space_initialize(void);
void runtime_address_space_bind_page_table(RuntimePageTable *page_table);
void runtime_address_space_configure(
    const RuntimeAddressSpaceProfile *profile
);
uint16_t runtime_address_space_handle_for_domain(uint8_t domain);
uint16_t runtime_address_space_mapping_publish(
    const RuntimeAddressSpaceMapRequest *request
);
RuntimeAddressSpaceMapping *runtime_address_space_mapping_resolve(
    uint16_t handle,
    uint8_t required_rights
);
bool runtime_address_space_mapping_contains_domain(
    const RuntimeAddressSpaceMapping *mapping,
    uint8_t domain
);
void runtime_address_space_assert_no_conflict(
    uintptr_t virtual_lower_bound,
    uintptr_t virtual_upper_bound,
    bool mark_shared
);
void runtime_address_space_validate_region_mapping(
    RuntimeFirebloomSpan region,
    const RuntimeAddressSpaceMapping *mapping,
    uint8_t domain
);
bool runtime_address_space_range_is_direct(
    uintptr_t virtual_lower_bound,
    size_t size,
    uint16_t physical_region_id
);
RuntimeAddressSpaceMapResult runtime_address_space_map(
    const RuntimeAddressSpaceMapRequest *request
);
RuntimeAddressSpaceMapResult runtime_address_space_attach_region(
    uint16_t physical_region_id,
    uintptr_t active_alias,
    uint8_t memory_attributes,
    uint8_t mapping_attributes,
    uint8_t flags,
    uint8_t rights
);
void runtime_address_space_detach_region(uint16_t physical_region_id);
void runtime_address_space_reprotect(
    uint16_t mapping_handle,
    uint8_t memory_attributes,
    uint8_t mapping_attributes,
    uint8_t flags
);
bool runtime_address_space_region_is_attached(uint16_t physical_region_id);
size_t runtime_address_space_mapping_count(void);
