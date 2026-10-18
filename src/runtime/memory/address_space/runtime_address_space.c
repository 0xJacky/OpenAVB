#include "avp/runtime/memory/runtime_address_space.h"

#include "avp/runtime/ipc/runtime_capability.h"
#include "avp/runtime/ipc/runtime_domain.h"
#include "avp/runtime/memory/runtime_physical_memory.h"


enum {
    RUNTIME_ADDRESS_SPACE_MAPPING_CAPACITY = 64,
};

#if defined(__ELF__)
#define RUNTIME_ADDRESS_SPACE_SECTION \
    __attribute__((section(".runtime_task.state")))
#define RUNTIME_ADDRESS_SPACE_RECOVERED \
    __attribute__((noinline, section(".text.runtime_address_space_recovered")))
#else
#define RUNTIME_ADDRESS_SPACE_SECTION
#define RUNTIME_ADDRESS_SPACE_RECOVERED __attribute__((noinline))
#endif

typedef struct {
    RuntimeAddressSpaceMapping mapping;
    bool active;
} RuntimeAddressSpaceSlot;

static RuntimeAddressSpaceSlot mapping_slots[
    RUNTIME_ADDRESS_SPACE_MAPPING_CAPACITY
] RUNTIME_ADDRESS_SPACE_SECTION;
static size_t mapping_count RUNTIME_ADDRESS_SPACE_SECTION;
static bool attached_regions[RUNTIME_PHYSICAL_REGION_ID_MAX + 1]
    RUNTIME_ADDRESS_SPACE_SECTION;
static RuntimeAddressSpaceProfile active_profile
    RUNTIME_ADDRESS_SPACE_SECTION;
static RuntimePageTable *active_page_table RUNTIME_ADDRESS_SPACE_SECTION;

_Noreturn static void runtime_address_space_trap(void) {
    __builtin_trap();
}

static uint16_t runtime_address_space_domain_audience(uint8_t domain) {
    return runtime_capability_audience_for_domain(domain);
}

static bool runtime_address_space_range_valid(
    uintptr_t lower_bound,
    size_t size
) {
    return size >= RUNTIME_ADDRESS_SPACE_PAGE_SIZE &&
        (lower_bound & (RUNTIME_ADDRESS_SPACE_PAGE_SIZE - 1)) == 0 &&
        (size & (RUNTIME_ADDRESS_SPACE_PAGE_SIZE - 1)) == 0 &&
        lower_bound <= UINTPTR_MAX - size;
}

static bool runtime_address_space_ranges_overlap(
    uintptr_t left_lower,
    uintptr_t left_upper,
    uintptr_t right_lower,
    uintptr_t right_upper
) {
    return left_lower < right_upper && right_lower < left_upper;
}

static bool runtime_address_space_mapping_identity_equal(
    const RuntimeAddressSpaceMapping *mapping,
    const RuntimeAddressSpaceMapRequest *request,
    uintptr_t upper_bound
) {
    return mapping->virtual_lower_bound == request->virtual_lower_bound &&
        mapping->virtual_upper_bound == upper_bound &&
        mapping->physical_base == request->physical_base;
}

static void runtime_address_space_validate_request(
    const RuntimeAddressSpaceMapRequest *request
) {
    if (request == 0 ||
        !runtime_address_space_range_valid(
            request->virtual_lower_bound,
            request->size
        ) ||
        (request->physical_base &
            (RUNTIME_ADDRESS_SPACE_PAGE_SIZE - 1)) != 0 ||
        request->audience == 0 ||
        (request->audience & ~RUNTIME_CAPABILITY_AUDIENCE_ALL) != 0 ||
        request->owner_domain == 0 ||
        request->owner_domain > RUNTIME_DOMAIN_ID_MAX ||
        request->memory_attributes > 6 ||
        request->mapping_attributes > 1 ||
        request->rights == 0 ||
        (request->rights & ~UINT8_C(7)) != 0 ||
        (request->flags &
            ~RUNTIME_ADDRESS_SPACE_MAPPING_FLAGS_ALLOWED) != 0 ||
        (request->audience & runtime_address_space_domain_audience(
            request->owner_domain
        )) == 0 ||
        request->physical_region_id > RUNTIME_PHYSICAL_REGION_ID_MAX) {
        runtime_address_space_trap();
    }
}

void runtime_address_space_initialize(void) {
    for (size_t index = 0;
         index < RUNTIME_ADDRESS_SPACE_MAPPING_CAPACITY;
         ++index) {
        mapping_slots[index] = (RuntimeAddressSpaceSlot){0};
    }
    mapping_count = 0;
    active_page_table = 0;
    for (size_t index = 0;
         index <= RUNTIME_PHYSICAL_REGION_ID_MAX;
         ++index) {
        attached_regions[index] = false;
    }
    active_profile = (RuntimeAddressSpaceProfile){
        .low_direct_upper_bound = UINT64_C(0x0000010000000000),
        .high_direct_lower_bound = UINT64_C(0xfffffc0000000000),
        .special_direct_region_id = UINT16_C(0x0163),
    };
}

void runtime_address_space_bind_page_table(RuntimePageTable *page_table) {
    if (page_table == 0 || page_table->root == 0 || page_table->pool == 0 ||
        active_page_table != 0 || mapping_count != 0) {
        runtime_address_space_trap();
    }
    active_page_table = page_table;
}

static RuntimePageTableAttributes runtime_address_space_page_attributes(
    uint8_t memory_attributes,
    uint8_t mapping_attributes,
    uint8_t flags
) {
    const bool execute_never = mapping_attributes != 0 ||
        (flags & RUNTIME_ADDRESS_SPACE_MAPPING_FLAG_DEVICE) != 0;
    return (RuntimePageTableAttributes){
        .memory_attribute_index = memory_attributes,
        .access = RUNTIME_PAGE_TABLE_ACCESS_USER_READ_WRITE,
        .shareability = RUNTIME_PAGE_TABLE_SHAREABILITY_INNER,
        .access_flag = true,
        .not_global = (flags &
            RUNTIME_ADDRESS_SPACE_MAPPING_FLAG_GLOBAL) == 0,
        .privileged_execute_never = execute_never,
        .unprivileged_execute_never = execute_never,
    };
}

static void runtime_address_space_apply_page_table_mapping(
    const RuntimeAddressSpaceMapRequest *request
) {
    if (active_page_table == 0) {
        return;
    }
    const RuntimePageTableAttributes attributes =
        runtime_address_space_page_attributes(
            request->memory_attributes,
            request->mapping_attributes,
            request->flags
        );
    RuntimePageTableTranslation translation;
    RuntimePageTableStatus status = runtime_page_table_lookup(
        active_page_table,
        request->virtual_lower_bound,
        &translation
    );
    if (status == RUNTIME_PAGE_TABLE_STATUS_NOT_MAPPED) {
        status = runtime_page_table_map(
            active_page_table,
            request->virtual_lower_bound,
            request->physical_base,
            request->size,
            attributes
        );
        if (status != RUNTIME_PAGE_TABLE_STATUS_OK) {
            runtime_address_space_trap();
        }
        return;
    }
    if (status != RUNTIME_PAGE_TABLE_STATUS_OK) {
        runtime_address_space_trap();
    }

    bool identity_matches = true;
    uintptr_t virtual_cursor = request->virtual_lower_bound;
    uintptr_t physical_cursor = request->physical_base;
    size_t remaining = request->size;
    while (remaining != 0) {
        status = runtime_page_table_lookup(
            active_page_table,
            virtual_cursor,
            &translation
        );
        if (status != RUNTIME_PAGE_TABLE_STATUS_OK) {
            runtime_address_space_trap();
        }
        if (translation.physical_address != physical_cursor) {
            identity_matches = false;
        }
        const size_t offset = (size_t)(
            virtual_cursor - translation.virtual_address
        );
        const size_t available = translation.mapped_size - offset;
        const size_t consumed = available < remaining ?
            available : remaining;
        remaining -= consumed;
        if (remaining != 0) {
            virtual_cursor += consumed;
            physical_cursor += consumed;
        }
    }
    if (!identity_matches && runtime_page_table_unmap(
            active_page_table,
            request->virtual_lower_bound,
            request->size
        ) != RUNTIME_PAGE_TABLE_STATUS_OK) {
        runtime_address_space_trap();
    }
    status = identity_matches ?
        runtime_page_table_reprotect(
            active_page_table,
            request->virtual_lower_bound,
            request->size,
            attributes
        ) :
        runtime_page_table_map(
            active_page_table,
            request->virtual_lower_bound,
            request->physical_base,
            request->size,
            attributes
        );
    if (status != RUNTIME_PAGE_TABLE_STATUS_OK) {
        runtime_address_space_trap();
    }
}

void runtime_address_space_configure(
    const RuntimeAddressSpaceProfile *profile
) {
    if (profile == 0 ||
        profile->low_direct_upper_bound == 0 ||
        profile->high_direct_lower_bound <=
            profile->low_direct_upper_bound ||
        profile->special_direct_region_id == 0 ||
        profile->special_direct_region_id >
            RUNTIME_PHYSICAL_REGION_ID_MAX) {
        runtime_address_space_trap();
    }
    active_profile = *profile;
}

RUNTIME_ADDRESS_SPACE_RECOVERED
uint16_t runtime_address_space_handle_for_domain(uint8_t domain) {
    RuntimeDomain *record = runtime_domain_lookup(domain);
    if (record == 0 || record->primary_mapping_handle == 0) {
        runtime_address_space_trap();
    }
    return record->primary_mapping_handle;
}

RUNTIME_ADDRESS_SPACE_RECOVERED
uint16_t runtime_address_space_mapping_publish(
    const RuntimeAddressSpaceMapRequest *request
) {
    runtime_address_space_validate_request(request);
    const uintptr_t upper_bound =
        request->virtual_lower_bound + request->size;
    for (size_t index = 0;
         index < RUNTIME_ADDRESS_SPACE_MAPPING_CAPACITY;
         ++index) {
        RuntimeAddressSpaceSlot *slot = &mapping_slots[index];
        if (!slot->active) {
            continue;
        }
        RuntimeAddressSpaceMapping *mapping = &slot->mapping;
        if (runtime_address_space_mapping_identity_equal(
                mapping,
                request,
                upper_bound
            )) {
            if (mapping->physical_region_id !=
                    request->physical_region_id ||
                mapping->owner_domain != request->owner_domain ||
                mapping->memory_attributes !=
                    request->memory_attributes ||
                mapping->mapping_attributes !=
                    request->mapping_attributes ||
                ((mapping->flags ^ request->flags) &
                    ~RUNTIME_ADDRESS_SPACE_MAPPING_FLAG_SHARED) != 0) {
                runtime_address_space_trap();
            }
            mapping->audience |= request->audience;
            mapping->flags |= request->flags &
                RUNTIME_ADDRESS_SPACE_MAPPING_FLAG_SHARED;
            const uint16_t handle = runtime_capability_publish(
                mapping,
                &runtime_memory_mapping_type,
                RUNTIME_CAPABILITY_TYPE_MEMORY_MAPPING,
                request->rights,
                request->audience
            );
            if (handle == 0) {
                runtime_address_space_trap();
            }
            return handle;
        }
        if (runtime_address_space_ranges_overlap(
                mapping->virtual_lower_bound,
                mapping->virtual_upper_bound,
                request->virtual_lower_bound,
                upper_bound
            )) {
            runtime_address_space_trap();
        }
    }
    for (size_t index = 0;
         index < RUNTIME_ADDRESS_SPACE_MAPPING_CAPACITY;
         ++index) {
        RuntimeAddressSpaceSlot *slot = &mapping_slots[index];
        if (slot->active) {
            continue;
        }
        runtime_address_space_apply_page_table_mapping(request);
        slot->mapping = (RuntimeAddressSpaceMapping){
            .virtual_lower_bound = request->virtual_lower_bound,
            .virtual_upper_bound = upper_bound,
            .physical_base = request->physical_base,
            .audience = request->audience,
            .physical_region_id = request->physical_region_id,
            .owner_domain = request->owner_domain,
            .memory_attributes = request->memory_attributes,
            .mapping_attributes = request->mapping_attributes,
            .flags = request->flags,
        };
        slot->active = true;
        ++mapping_count;
        const uint16_t handle = runtime_capability_publish(
            &slot->mapping,
            &runtime_memory_mapping_type,
            RUNTIME_CAPABILITY_TYPE_MEMORY_MAPPING,
            request->rights,
            request->audience
        );
        if (handle == 0) {
            runtime_address_space_trap();
        }
        return handle;
    }
    runtime_address_space_trap();
}

RUNTIME_ADDRESS_SPACE_RECOVERED
RuntimeAddressSpaceMapping *runtime_address_space_mapping_resolve(
    uint16_t handle,
    uint8_t required_rights
) {
    RuntimeAddressSpaceMapping *mapping = runtime_capability_resolve(
        handle,
        RUNTIME_CAPABILITY_TYPE_MEMORY_MAPPING,
        required_rights
    );
    if (mapping == 0) {
        runtime_address_space_trap();
    }
    return mapping;
}

RUNTIME_ADDRESS_SPACE_RECOVERED
bool runtime_address_space_mapping_contains_domain(
    const RuntimeAddressSpaceMapping *mapping,
    uint8_t domain
) {
    return mapping != 0 &&
        (mapping->audience &
            runtime_address_space_domain_audience(domain)) != 0;
}

RUNTIME_ADDRESS_SPACE_RECOVERED
void runtime_address_space_assert_no_conflict(
    uintptr_t virtual_lower_bound,
    uintptr_t virtual_upper_bound,
    bool mark_shared
) {
    if (virtual_lower_bound >= virtual_upper_bound) {
        runtime_address_space_trap();
    }
    const uint16_t current_audience =
        runtime_address_space_domain_audience(runtime_current_domain());
    for (size_t index = 0;
         index < RUNTIME_ADDRESS_SPACE_MAPPING_CAPACITY;
         ++index) {
        RuntimeAddressSpaceSlot *slot = &mapping_slots[index];
        if (!slot->active ||
            !runtime_address_space_ranges_overlap(
                slot->mapping.virtual_lower_bound,
                slot->mapping.virtual_upper_bound,
                virtual_lower_bound,
                virtual_upper_bound
            )) {
            continue;
        }
        if (mark_shared &&
            (slot->mapping.flags &
                RUNTIME_ADDRESS_SPACE_MAPPING_FLAG_SHARED) != 0 &&
            (slot->mapping.audience & current_audience) != 0) {
            slot->mapping.flags |=
                RUNTIME_ADDRESS_SPACE_MAPPING_FLAG_PHYSICAL_ALIAS;
            continue;
        }
        runtime_address_space_trap();
    }
}

RUNTIME_ADDRESS_SPACE_RECOVERED
void runtime_address_space_validate_region_mapping(
    RuntimeFirebloomSpan region,
    const RuntimeAddressSpaceMapping *mapping,
    uint8_t domain
) {
    if (!runtime_physical_memory_region_is_live(region) ||
        mapping == 0 ||
        runtime_physical_memory_region_id(region.current) !=
            mapping->physical_region_id ||
        region.current == 0) {
        runtime_address_space_trap();
    }
    const RuntimePhysicalMemoryRegion *physical_region = region.current;
    if (physical_region->physical_base != mapping->physical_base) {
        runtime_address_space_trap();
    }
    if (runtime_address_space_mapping_contains_domain(mapping, domain)) {
        if (runtime_physical_memory_region_active_base(region) !=
                mapping->virtual_lower_bound ||
            runtime_physical_memory_region_active_upper_bound(region) !=
                mapping->virtual_upper_bound) {
            runtime_address_space_trap();
        }
    }
}

RUNTIME_ADDRESS_SPACE_RECOVERED
bool runtime_address_space_range_is_direct(
    uintptr_t virtual_lower_bound,
    size_t size,
    uint16_t physical_region_id
) {
    if (!runtime_address_space_range_valid(virtual_lower_bound, size)) {
        runtime_address_space_trap();
    }
    if (physical_region_id == active_profile.special_direct_region_id) {
        return true;
    }
    return virtual_lower_bound < active_profile.low_direct_upper_bound ||
        virtual_lower_bound >= active_profile.high_direct_lower_bound;
}

RUNTIME_ADDRESS_SPACE_RECOVERED
RuntimeAddressSpaceMapResult runtime_address_space_map(
    const RuntimeAddressSpaceMapRequest *request
) {
    runtime_address_space_validate_request(request);
    return (RuntimeAddressSpaceMapResult){
        .mapping_handle = runtime_address_space_mapping_publish(request),
        .virtual_lower_bound = request->virtual_lower_bound,
    };
}

RUNTIME_ADDRESS_SPACE_RECOVERED
RuntimeAddressSpaceMapResult runtime_address_space_attach_region(
    uint16_t physical_region_id,
    uintptr_t active_alias,
    uint8_t memory_attributes,
    uint8_t mapping_attributes,
    uint8_t flags,
    uint8_t rights
) {
    RuntimeFirebloomSpan span =
        runtime_physical_memory_region_lookup(physical_region_id);
    if (!runtime_physical_memory_region_is_live(span)) {
        runtime_address_space_trap();
    }
    RuntimePhysicalMemoryRegion *region = span.current;
    const uintptr_t virtual_lower_bound = active_alias == 0
        ? runtime_physical_memory_region_active_base(span)
        : active_alias;
    const uint16_t existing_handle =
        (uint16_t)(region->descriptor >> 48);
    if (existing_handle != 0) {
        RuntimeAddressSpaceMapping *mapping =
            runtime_address_space_mapping_resolve(existing_handle, 3);
        runtime_address_space_validate_region_mapping(
            span,
            mapping,
            runtime_current_domain()
        );
        attached_regions[physical_region_id] = true;
        return (RuntimeAddressSpaceMapResult){
            .mapping_handle = existing_handle,
            .virtual_lower_bound = mapping->virtual_lower_bound,
        };
    }
    const RuntimeAddressSpaceMapRequest request = {
        .virtual_lower_bound = virtual_lower_bound,
        .size = region->size,
        .physical_base = region->physical_base,
        .audience = runtime_address_space_domain_audience(
            runtime_current_domain()
        ),
        .physical_region_id = physical_region_id,
        .owner_domain = runtime_current_domain(),
        .memory_attributes = memory_attributes,
        .mapping_attributes = mapping_attributes,
        .flags = flags,
        .rights = rights,
    };
    const RuntimeAddressSpaceMapResult result =
        runtime_address_space_map(&request);
    region->active_alias = result.virtual_lower_bound;
    region->descriptor =
        (region->descriptor & UINT64_C(0x0000ffffffffffff)) |
        ((uint64_t)result.mapping_handle << 48);
    attached_regions[physical_region_id] = true;
    return result;
}

void runtime_address_space_detach_region(uint16_t physical_region_id) {
    RuntimeFirebloomSpan span =
        runtime_physical_memory_region_lookup(physical_region_id);
    if (!runtime_physical_memory_region_is_live(span) ||
        !attached_regions[physical_region_id]) {
        runtime_address_space_trap();
    }
    RuntimePhysicalMemoryRegion *region = span.current;
    const uint16_t handle = (uint16_t)(region->descriptor >> 48);
    RuntimeAddressSpaceMapping *mapping =
        runtime_address_space_mapping_resolve(handle, 3);
    if (active_page_table != 0 && runtime_page_table_unmap(
            active_page_table,
            mapping->virtual_lower_bound,
            mapping->virtual_upper_bound - mapping->virtual_lower_bound
        ) != RUNTIME_PAGE_TABLE_STATUS_OK) {
        runtime_address_space_trap();
    }
    RuntimeAddressSpaceSlot *slot = (RuntimeAddressSpaceSlot *)mapping;
    if (slot < mapping_slots ||
        slot >= mapping_slots + RUNTIME_ADDRESS_SPACE_MAPPING_CAPACITY ||
        !slot->active) {
        runtime_address_space_trap();
    }
    runtime_capability_revoke_rights(handle, UINT8_C(7));
    slot->active = false;
    --mapping_count;
    region->descriptor &= UINT64_C(0x0000ffffffffffff);
    region->active_alias = 0;
    attached_regions[physical_region_id] = false;
}

void runtime_address_space_reprotect(
    uint16_t mapping_handle,
    uint8_t memory_attributes,
    uint8_t mapping_attributes,
    uint8_t flags
) {
    if (memory_attributes > 6 || mapping_attributes > 1 ||
        (flags & ~RUNTIME_ADDRESS_SPACE_MAPPING_FLAGS_ALLOWED) != 0) {
        runtime_address_space_trap();
    }
    RuntimeAddressSpaceMapping *mapping =
        runtime_address_space_mapping_resolve(mapping_handle, 3);
    if (active_page_table != 0 && runtime_page_table_reprotect(
            active_page_table,
            mapping->virtual_lower_bound,
            mapping->virtual_upper_bound - mapping->virtual_lower_bound,
            runtime_address_space_page_attributes(
                memory_attributes,
                mapping_attributes,
                flags
            )
        ) != RUNTIME_PAGE_TABLE_STATUS_OK) {
        runtime_address_space_trap();
    }
    mapping->memory_attributes = memory_attributes;
    mapping->mapping_attributes = mapping_attributes;
    mapping->flags = flags;
}

RUNTIME_ADDRESS_SPACE_RECOVERED
bool runtime_address_space_region_is_attached(uint16_t physical_region_id) {
    if (physical_region_id == 0 ||
        physical_region_id > RUNTIME_PHYSICAL_REGION_ID_MAX) {
        runtime_address_space_trap();
    }
    return attached_regions[physical_region_id];
}

size_t runtime_address_space_mapping_count(void) {
    return mapping_count;
}
