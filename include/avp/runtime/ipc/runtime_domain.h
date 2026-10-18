#pragma once

#include "avp/runtime/firebloom/runtime_firebloom.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


enum {
    AVP_RUNTIME_DOMAIN_BOOT = 1,
    AVP_RUNTIME_DOMAIN_SIMPLE_VIRTIO = 3,
    RUNTIME_DOMAIN_ID_MAX = 15,
    RUNTIME_DOMAIN_DESCRIPTOR_KIND = 0x102,
};

typedef struct {
    void *previous;
    uintptr_t previous_lower_bound;
    uintptr_t previous_upper_bound;
    uintptr_t previous_cookie;
    void *next;
    uintptr_t next_lower_bound;
    uintptr_t next_upper_bound;
    uintptr_t next_cookie;
} RuntimeDomainRegistryLink;

_Static_assert(
    sizeof(RuntimeDomainRegistryLink) == 0x40,
    "RuntimeDomainRegistryLink size"
);

typedef struct {
    uint64_t identity;
    uintptr_t optional_virtual_lower_bound;
    uintptr_t optional_virtual_upper_bound;
    uint16_t reserved_18;
    uint16_t descriptor_kind;
    uint8_t domain_id;
    uint8_t reserved_1d[3];
} RuntimeDomainDefinition;

_Static_assert(
    sizeof(RuntimeDomainDefinition) == 0x20,
    "RuntimeDomainDefinition size"
);
_Static_assert(
    offsetof(RuntimeDomainDefinition, descriptor_kind) == 0x1a,
    "domain descriptor kind offset"
);
_Static_assert(
    offsetof(RuntimeDomainDefinition, domain_id) == 0x1c,
    "domain definition id offset"
);

typedef struct {
    uintptr_t physical_base;
    uintptr_t physical_upper_bound;
    size_t size;
} RuntimeDomainPhysicalMemory;

_Static_assert(
    sizeof(RuntimeDomainPhysicalMemory) == 0x18,
    "RuntimeDomainPhysicalMemory size"
);

typedef struct {
    uint64_t identity;
    RuntimeFirebloomSpan definition;
    RuntimeFirebloomSpan physical_memory;
    uint32_t mapping_region;
    uint32_t optional_allocation_handle;
    uint16_t parent_capability_handle;
    uint16_t reserved_52;
    uint32_t mapping_mode;
    uint8_t parent_capability_rights;
    uint8_t parent_capability_type;
    uint8_t parent_domain;
    uint8_t capability_rights;
    uint32_t reserved_5c;
} RuntimeDomainDescriptor;

_Static_assert(
    sizeof(RuntimeDomainDescriptor) == 0x60,
    "RuntimeDomainDescriptor size"
);
_Static_assert(
    offsetof(RuntimeDomainDescriptor, definition) == 0x08,
    "domain definition span offset"
);
_Static_assert(
    offsetof(RuntimeDomainDescriptor, physical_memory) == 0x28,
    "domain physical-memory span offset"
);
_Static_assert(
    offsetof(RuntimeDomainDescriptor, mapping_region) == 0x48,
    "domain mapping-region offset"
);
_Static_assert(
    offsetof(RuntimeDomainDescriptor, parent_capability_handle) == 0x50,
    "domain parent capability offset"
);
_Static_assert(
    offsetof(RuntimeDomainDescriptor, mapping_mode) == 0x54,
    "domain mapping mode offset"
);
_Static_assert(
    offsetof(RuntimeDomainDescriptor, parent_domain) == 0x5a,
    "domain parent id offset"
);

typedef struct RuntimeDomain {
    RuntimeDomainRegistryLink registry_link;
    uintptr_t address_space_root;
    uintptr_t address_space_cookie;
    RuntimeFirebloomSpan definition;
    const uint16_t *service_method_acl;
    uintptr_t service_method_acl_lower_bound;
    uintptr_t service_method_acl_upper_bound;
    uintptr_t service_method_acl_cookie;
    uint16_t service_method_count;
    uint8_t reserved_092[0x06];
    uintptr_t primary_physical_base;
    uintptr_t primary_virtual_base;
    size_t primary_size;
    uint64_t primary_reserved;
    uint8_t reserved_0b8[0x20];
    uintptr_t optional_physical_base;
    uintptr_t optional_virtual_lower_bound;
    uintptr_t optional_virtual_upper_bound;
    uintptr_t address_space_prefix;
    void *service_packet;
    uint16_t service_receive_endpoint_handle;
    uint16_t service_active_token;
    uint16_t service_send_endpoint_handle;
    uint16_t primary_mapping_handle;
    uint16_t runtime_handle;
    uint16_t public_handle;
    uint8_t parent_domain;
    uint8_t domain_id;
    uint8_t service_active_caller_ordinal;
    uint8_t task_ordinal_count;
    uint64_t service_key;
} RuntimeDomain;

_Static_assert(sizeof(RuntimeDomain) == 0x118, "RuntimeDomain size");
_Static_assert(
    offsetof(RuntimeDomain, address_space_root) == 0x40,
    "domain address-space root offset"
);
_Static_assert(
    offsetof(RuntimeDomain, definition) == 0x50,
    "domain definition offset"
);
_Static_assert(
    offsetof(RuntimeDomain, service_method_acl) == 0x70,
    "service ACL offset"
);
_Static_assert(
    offsetof(RuntimeDomain, service_method_count) == 0x90,
    "service method count offset"
);
_Static_assert(
    offsetof(RuntimeDomain, primary_physical_base) == 0x98,
    "primary physical base offset"
);
_Static_assert(
    offsetof(RuntimeDomain, primary_virtual_base) == 0xa0,
    "primary virtual base offset"
);
_Static_assert(
    offsetof(RuntimeDomain, primary_size) == 0xa8,
    "primary size offset"
);
_Static_assert(
    offsetof(RuntimeDomain, optional_physical_base) == 0xd8,
    "optional physical base offset"
);
_Static_assert(
    offsetof(RuntimeDomain, optional_virtual_lower_bound) == 0xe0,
    "optional virtual lower offset"
);
_Static_assert(
    offsetof(RuntimeDomain, address_space_prefix) == 0xf0,
    "domain address-space prefix offset"
);
_Static_assert(
    offsetof(RuntimeDomain, service_packet) == 0xf8,
    "service packet offset"
);
_Static_assert(
    offsetof(RuntimeDomain, service_active_token) == 0x102,
    "service token offset"
);
_Static_assert(
    offsetof(RuntimeDomain, primary_mapping_handle) == 0x106,
    "primary mapping capability offset"
);
_Static_assert(
    offsetof(RuntimeDomain, runtime_handle) == 0x108,
    "runtime handle offset"
);
_Static_assert(
    offsetof(RuntimeDomain, public_handle) == 0x10a,
    "public domain capability offset"
);
_Static_assert(offsetof(RuntimeDomain, domain_id) == 0x10d, "domain id offset");
_Static_assert(
    offsetof(RuntimeDomain, service_active_caller_ordinal) == 0x10e,
    "service caller offset"
);
_Static_assert(
    offsetof(RuntimeDomain, service_key) == 0x110,
    "service key offset"
);

extern uint8_t runtime_execution_domain;
extern const RuntimeFirebloomType runtime_domain_record_type;
extern const RuntimeFirebloomType runtime_domain_definition_type;

void runtime_domain_reset(uint8_t domain);
uint8_t runtime_current_domain(void);
void runtime_domain_set_current(const RuntimeDomain *domain);
bool runtime_domain_register(
    RuntimeDomain *domain,
    uint8_t domain_id,
    uint8_t parent_domain,
    uint16_t runtime_handle
);
void runtime_domain_register_allocation(
    uint32_t handle,
    uintptr_t physical_base,
    size_t size
);
uint64_t runtime_domain_identity_read(RuntimeFirebloomSpan identity);
uint64_t runtime_domain_record_identity(RuntimeFirebloomSpan record);
RuntimeDomain *runtime_domain_from_registry_link(
    RuntimeFirebloomSpan link_record
);
uint16_t runtime_domain_create_and_register(
    const RuntimeDomainDescriptor *descriptor,
    uintptr_t descriptor_lower_bound,
    uintptr_t descriptor_upper_bound,
    RuntimeFirebloomSpan *output_record,
    uintptr_t output_lower_bound,
    uintptr_t output_upper_bound
);
RuntimeDomain *runtime_domain_lookup(uint8_t domain);
bool runtime_domain_registry_contains(uint8_t domain);
bool runtime_domain_resolve_locality(uint8_t requested_domain);
