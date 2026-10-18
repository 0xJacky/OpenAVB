#include "avp/runtime/memory/runtime_page_table.h"


enum {
    RUNTIME_PAGE_TABLE_DESCRIPTOR_TYPE_MASK = 3,
    RUNTIME_PAGE_TABLE_ATTRIBUTE_INDEX_SHIFT = 2,
    RUNTIME_PAGE_TABLE_ACCESS_SHIFT = 6,
    RUNTIME_PAGE_TABLE_SHAREABILITY_SHIFT = 8,
};

#define RUNTIME_PAGE_TABLE_OUTPUT_ADDRESS_MASK \
    UINT64_C(0x0000ffffffffc000)
#define RUNTIME_PAGE_TABLE_LOWER_ATTRIBUTE_MASK UINT64_C(0x0000000000000ffc)
#define RUNTIME_PAGE_TABLE_UPPER_ATTRIBUTE_MASK UINT64_C(0x007c000000000000)
#define RUNTIME_PAGE_TABLE_LEAF_ATTRIBUTE_MASK \
    (RUNTIME_PAGE_TABLE_LOWER_ATTRIBUTE_MASK | \
        RUNTIME_PAGE_TABLE_UPPER_ATTRIBUTE_MASK)

#define RUNTIME_PAGE_TABLE_ACCESS_FLAG (UINT64_C(1) << 10)
#define RUNTIME_PAGE_TABLE_NON_SECURE (UINT64_C(1) << 5)
#define RUNTIME_PAGE_TABLE_NOT_GLOBAL (UINT64_C(1) << 11)
#define RUNTIME_PAGE_TABLE_GUARDED_PAGE (UINT64_C(1) << 50)
#define RUNTIME_PAGE_TABLE_DIRTY_BIT_MANAGEMENT (UINT64_C(1) << 51)
#define RUNTIME_PAGE_TABLE_CONTIGUOUS (UINT64_C(1) << 52)
#define RUNTIME_PAGE_TABLE_PRIVILEGED_EXECUTE_NEVER (UINT64_C(1) << 53)
#define RUNTIME_PAGE_TABLE_UNPRIVILEGED_EXECUTE_NEVER (UINT64_C(1) << 54)

static bool runtime_page_table_level_is_valid(uint8_t level) {
    return level <= RUNTIME_PAGE_TABLE_LEVEL_MAX;
}

uint8_t __attribute__((used, retain))
runtime_page_table_level_shift(uint8_t level) {
    static const uint8_t shifts[] = {47, 36, 25, 14};
    return runtime_page_table_level_is_valid(level) ? shifts[level] : 0;
}

size_t runtime_page_table_level_span(uint8_t level) {
    if (!runtime_page_table_level_is_valid(level)) {
        return 0;
    }
    return (size_t)1 << runtime_page_table_level_shift(level);
}

size_t runtime_page_table_level_index(
    uintptr_t virtual_address,
    uint8_t level
) {
    if (!runtime_page_table_level_is_valid(level)) {
        return RUNTIME_PAGE_TABLE_ENTRY_COUNT;
    }
    return (virtual_address >> runtime_page_table_level_shift(level)) &
        (RUNTIME_PAGE_TABLE_ENTRY_COUNT - 1);
}

uint8_t __attribute__((used, retain))
runtime_page_table_root_level(uint8_t virtual_address_bits) {
    if (virtual_address_bits <
            RUNTIME_PAGE_TABLE_VIRTUAL_ADDRESS_BITS_MIN ||
        virtual_address_bits >
            RUNTIME_PAGE_TABLE_VIRTUAL_ADDRESS_BITS_MAX) {
        return UINT8_MAX;
    }
    for (uint8_t level = RUNTIME_PAGE_TABLE_LEVEL_MAX;; --level) {
        const uint8_t coverage = (uint8_t)(
            runtime_page_table_level_shift(level) + 11
        );
        if (coverage >= virtual_address_bits) {
            return level;
        }
        if (level == RUNTIME_PAGE_TABLE_LEVEL_MIN) {
            break;
        }
    }
    return UINT8_MAX;
}

uint8_t runtime_page_table_canonical_region(
    uintptr_t address,
    uint8_t virtual_address_bits
) {
    if (virtual_address_bits <
            RUNTIME_PAGE_TABLE_VIRTUAL_ADDRESS_BITS_MIN ||
        virtual_address_bits >
            RUNTIME_PAGE_TABLE_VIRTUAL_ADDRESS_BITS_MAX) {
        return 0;
    }
    const uintptr_t region_size = (uintptr_t)1 << virtual_address_bits;
    if (address < region_size) {
        return 1;
    }
    if (address >= UINTPTR_MAX - region_size + 1) {
        return 2;
    }
    return 0;
}

bool runtime_page_table_address_is_canonical(
    uintptr_t address,
    uint8_t virtual_address_bits
) {
    return runtime_page_table_canonical_region(
        address,
        virtual_address_bits
    ) != 0;
}

static bool runtime_page_table_shareability_is_valid(
    RuntimePageTableShareability shareability
) {
    return shareability == RUNTIME_PAGE_TABLE_SHAREABILITY_NONE ||
        shareability == RUNTIME_PAGE_TABLE_SHAREABILITY_OUTER ||
        shareability == RUNTIME_PAGE_TABLE_SHAREABILITY_INNER;
}

RuntimePageTableStatus runtime_page_table_attributes_encode(
    RuntimePageTableAttributes attributes,
    uint64_t *attribute_bits
) {
    if (attribute_bits == 0 || attributes.memory_attribute_index > 7 ||
        attributes.access > RUNTIME_PAGE_TABLE_ACCESS_USER_READ_ONLY ||
        !runtime_page_table_shareability_is_valid(attributes.shareability)) {
        return RUNTIME_PAGE_TABLE_STATUS_INVALID_ARGUMENT;
    }
    uint64_t encoded =
        (uint64_t)attributes.memory_attribute_index <<
            RUNTIME_PAGE_TABLE_ATTRIBUTE_INDEX_SHIFT |
        (uint64_t)attributes.access << RUNTIME_PAGE_TABLE_ACCESS_SHIFT |
        (uint64_t)attributes.shareability <<
            RUNTIME_PAGE_TABLE_SHAREABILITY_SHIFT;
    if (attributes.non_secure) {
        encoded |= RUNTIME_PAGE_TABLE_NON_SECURE;
    }
    if (attributes.access_flag) {
        encoded |= RUNTIME_PAGE_TABLE_ACCESS_FLAG;
    }
    if (attributes.not_global) {
        encoded |= RUNTIME_PAGE_TABLE_NOT_GLOBAL;
    }
    if (attributes.guarded_page) {
        encoded |= RUNTIME_PAGE_TABLE_GUARDED_PAGE;
    }
    if (attributes.dirty_bit_management) {
        encoded |= RUNTIME_PAGE_TABLE_DIRTY_BIT_MANAGEMENT;
    }
    if (attributes.contiguous) {
        encoded |= RUNTIME_PAGE_TABLE_CONTIGUOUS;
    }
    if (attributes.privileged_execute_never) {
        encoded |= RUNTIME_PAGE_TABLE_PRIVILEGED_EXECUTE_NEVER;
    }
    if (attributes.unprivileged_execute_never) {
        encoded |= RUNTIME_PAGE_TABLE_UNPRIVILEGED_EXECUTE_NEVER;
    }
    *attribute_bits = encoded;
    return RUNTIME_PAGE_TABLE_STATUS_OK;
}

RuntimePageTableStatus runtime_page_table_attributes_decode(
    uint64_t attribute_bits,
    RuntimePageTableAttributes *attributes
) {
    if (attributes == 0 ||
        (attribute_bits & ~RUNTIME_PAGE_TABLE_LEAF_ATTRIBUTE_MASK) != 0) {
        return RUNTIME_PAGE_TABLE_STATUS_CORRUPT_DESCRIPTOR;
    }
    const RuntimePageTableShareability shareability =
        (RuntimePageTableShareability)(
            (attribute_bits >> RUNTIME_PAGE_TABLE_SHAREABILITY_SHIFT) & 3
        );
    if (!runtime_page_table_shareability_is_valid(shareability)) {
        return RUNTIME_PAGE_TABLE_STATUS_CORRUPT_DESCRIPTOR;
    }
    *attributes = (RuntimePageTableAttributes){
        .memory_attribute_index = (uint8_t)(
            (attribute_bits >> RUNTIME_PAGE_TABLE_ATTRIBUTE_INDEX_SHIFT) & 7
        ),
        .non_secure = (attribute_bits &
            RUNTIME_PAGE_TABLE_NON_SECURE) != 0,
        .access = (RuntimePageTableAccess)(
            (attribute_bits >> RUNTIME_PAGE_TABLE_ACCESS_SHIFT) & 3
        ),
        .shareability = shareability,
        .access_flag = (attribute_bits &
            RUNTIME_PAGE_TABLE_ACCESS_FLAG) != 0,
        .not_global = (attribute_bits & RUNTIME_PAGE_TABLE_NOT_GLOBAL) != 0,
        .privileged_execute_never = (attribute_bits &
            RUNTIME_PAGE_TABLE_PRIVILEGED_EXECUTE_NEVER) != 0,
        .unprivileged_execute_never = (attribute_bits &
            RUNTIME_PAGE_TABLE_UNPRIVILEGED_EXECUTE_NEVER) != 0,
        .guarded_page = (attribute_bits &
            RUNTIME_PAGE_TABLE_GUARDED_PAGE) != 0,
        .dirty_bit_management = (attribute_bits &
            RUNTIME_PAGE_TABLE_DIRTY_BIT_MANAGEMENT) != 0,
        .contiguous = (attribute_bits & RUNTIME_PAGE_TABLE_CONTIGUOUS) != 0,
    };
    return RUNTIME_PAGE_TABLE_STATUS_OK;
}

static bool runtime_page_table_physical_address_is_valid(
    uintptr_t physical_address
) {
    return physical_address <= RUNTIME_PAGE_TABLE_OUTPUT_ADDRESS_MASK;
}

RuntimePageTableStatus runtime_page_table_descriptor_encode_table(
    uintptr_t next_table_physical_address,
    RuntimePageTableEntry *entry
) {
    if (entry == 0 ||
        (next_table_physical_address &
            (RUNTIME_PAGE_TABLE_PAGE_SIZE - 1)) != 0 ||
        !runtime_page_table_physical_address_is_valid(
            next_table_physical_address
        )) {
        return RUNTIME_PAGE_TABLE_STATUS_INVALID_ARGUMENT;
    }
    *entry = next_table_physical_address |
        RUNTIME_PAGE_TABLE_DESCRIPTOR_TABLE_OR_PAGE;
    return RUNTIME_PAGE_TABLE_STATUS_OK;
}

RuntimePageTableStatus runtime_page_table_descriptor_encode_leaf(
    uint8_t level,
    uintptr_t physical_address,
    RuntimePageTableAttributes attributes,
    RuntimePageTableEntry *entry
) {
    if (entry == 0 || level < 1 || level > 3 ||
        !runtime_page_table_physical_address_is_valid(physical_address) ||
        (physical_address & (runtime_page_table_level_span(level) - 1)) != 0) {
        return RUNTIME_PAGE_TABLE_STATUS_INVALID_ARGUMENT;
    }
    uint64_t attribute_bits = 0;
    RuntimePageTableStatus status = runtime_page_table_attributes_encode(
        attributes,
        &attribute_bits
    );
    if (status != RUNTIME_PAGE_TABLE_STATUS_OK) {
        return status;
    }
    const uint64_t type = level == 3 ?
        RUNTIME_PAGE_TABLE_DESCRIPTOR_TABLE_OR_PAGE :
        RUNTIME_PAGE_TABLE_DESCRIPTOR_BLOCK;
    *entry = physical_address | attribute_bits | type;
    return RUNTIME_PAGE_TABLE_STATUS_OK;
}

RuntimePageTableStatus runtime_page_table_descriptor_decode(
    RuntimePageTableEntry entry,
    uint8_t level,
    RuntimePageTableDescriptor *descriptor
) {
    if (descriptor == 0 || !runtime_page_table_level_is_valid(level)) {
        return RUNTIME_PAGE_TABLE_STATUS_INVALID_ARGUMENT;
    }
    if (entry == 0) {
        *descriptor = (RuntimePageTableDescriptor){0};
        return RUNTIME_PAGE_TABLE_STATUS_OK;
    }
    const uint64_t type = entry & RUNTIME_PAGE_TABLE_DESCRIPTOR_TYPE_MASK;
    if (type == RUNTIME_PAGE_TABLE_DESCRIPTOR_TABLE_OR_PAGE && level < 3) {
        if ((entry & ~(RUNTIME_PAGE_TABLE_OUTPUT_ADDRESS_MASK |
                UINT64_C(3))) != 0) {
            return RUNTIME_PAGE_TABLE_STATUS_CORRUPT_DESCRIPTOR;
        }
        *descriptor = (RuntimePageTableDescriptor){
            .kind = RUNTIME_PAGE_TABLE_DESCRIPTOR_KIND_TABLE,
            .output_address = (uintptr_t)(entry &
                RUNTIME_PAGE_TABLE_OUTPUT_ADDRESS_MASK),
        };
        return RUNTIME_PAGE_TABLE_STATUS_OK;
    }
    const bool page = type ==
            RUNTIME_PAGE_TABLE_DESCRIPTOR_TABLE_OR_PAGE && level == 3;
    const bool block = type == RUNTIME_PAGE_TABLE_DESCRIPTOR_BLOCK &&
        level >= 1 && level <= 2;
    if (!page && !block) {
        return RUNTIME_PAGE_TABLE_STATUS_CORRUPT_DESCRIPTOR;
    }
    if ((entry & ~(RUNTIME_PAGE_TABLE_OUTPUT_ADDRESS_MASK |
            RUNTIME_PAGE_TABLE_LEAF_ATTRIBUTE_MASK | UINT64_C(3))) != 0) {
        return RUNTIME_PAGE_TABLE_STATUS_CORRUPT_DESCRIPTOR;
    }
    const uintptr_t output_address = (uintptr_t)(entry &
        RUNTIME_PAGE_TABLE_OUTPUT_ADDRESS_MASK);
    if ((output_address & (runtime_page_table_level_span(level) - 1)) != 0) {
        return RUNTIME_PAGE_TABLE_STATUS_CORRUPT_DESCRIPTOR;
    }
    const uint64_t attribute_bits = entry &
        RUNTIME_PAGE_TABLE_LEAF_ATTRIBUTE_MASK;
    RuntimePageTableAttributes ignored;
    RuntimePageTableStatus status = runtime_page_table_attributes_decode(
        attribute_bits,
        &ignored
    );
    if (status != RUNTIME_PAGE_TABLE_STATUS_OK) {
        return status;
    }
    *descriptor = (RuntimePageTableDescriptor){
        .kind = page ? RUNTIME_PAGE_TABLE_DESCRIPTOR_KIND_PAGE :
            RUNTIME_PAGE_TABLE_DESCRIPTOR_KIND_BLOCK,
        .output_address = output_address,
        .attribute_bits = attribute_bits,
    };
    return RUNTIME_PAGE_TABLE_STATUS_OK;
}

