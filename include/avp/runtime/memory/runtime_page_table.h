#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


enum {
    RUNTIME_PAGE_TABLE_PAGE_SHIFT = 14,
    RUNTIME_PAGE_TABLE_PAGE_SIZE = 1u << RUNTIME_PAGE_TABLE_PAGE_SHIFT,
    RUNTIME_PAGE_TABLE_ENTRY_SHIFT = 3,
    RUNTIME_PAGE_TABLE_ENTRY_COUNT =
        RUNTIME_PAGE_TABLE_PAGE_SIZE >> RUNTIME_PAGE_TABLE_ENTRY_SHIFT,
    RUNTIME_PAGE_TABLE_LEVEL_MIN = 0,
    RUNTIME_PAGE_TABLE_LEVEL_MAX = 3,
    RUNTIME_PAGE_TABLE_VIRTUAL_ADDRESS_BITS_MIN = 15,
    RUNTIME_PAGE_TABLE_VIRTUAL_ADDRESS_BITS_MAX = 48,
    RUNTIME_PAGE_TABLE_POOL_MAX_PAGES = 31,
};

#define RUNTIME_PAGE_TABLE_PHYSICAL_ADDRESS_MAX \
    UINT64_C(0x0000ffffffffffff)

enum {
    RUNTIME_PAGE_TABLE_DESCRIPTOR_INVALID = 0,
    RUNTIME_PAGE_TABLE_DESCRIPTOR_BLOCK = 1,
    RUNTIME_PAGE_TABLE_DESCRIPTOR_TABLE_OR_PAGE = 3,
};

typedef enum {
    RUNTIME_PAGE_TABLE_STATUS_OK = 0,
    RUNTIME_PAGE_TABLE_STATUS_INVALID_ARGUMENT = 1,
    RUNTIME_PAGE_TABLE_STATUS_OUT_OF_TABLES = 2,
    RUNTIME_PAGE_TABLE_STATUS_CONFLICT = 3,
    RUNTIME_PAGE_TABLE_STATUS_NOT_MAPPED = 4,
    RUNTIME_PAGE_TABLE_STATUS_CORRUPT_DESCRIPTOR = 5,
} RuntimePageTableStatus;

typedef enum {
    RUNTIME_PAGE_TABLE_DESCRIPTOR_KIND_INVALID = 0,
    RUNTIME_PAGE_TABLE_DESCRIPTOR_KIND_TABLE = 1,
    RUNTIME_PAGE_TABLE_DESCRIPTOR_KIND_BLOCK = 2,
    RUNTIME_PAGE_TABLE_DESCRIPTOR_KIND_PAGE = 3,
} RuntimePageTableDescriptorKind;

typedef enum {
    RUNTIME_PAGE_TABLE_ACCESS_PRIVILEGED_READ_WRITE = 0,
    RUNTIME_PAGE_TABLE_ACCESS_USER_READ_WRITE = 1,
    RUNTIME_PAGE_TABLE_ACCESS_PRIVILEGED_READ_ONLY = 2,
    RUNTIME_PAGE_TABLE_ACCESS_USER_READ_ONLY = 3,
} RuntimePageTableAccess;

typedef enum {
    RUNTIME_PAGE_TABLE_SHAREABILITY_NONE = 0,
    RUNTIME_PAGE_TABLE_SHAREABILITY_OUTER = 2,
    RUNTIME_PAGE_TABLE_SHAREABILITY_INNER = 3,
} RuntimePageTableShareability;

typedef uint64_t RuntimePageTableEntry;

typedef struct {
    _Alignas(RUNTIME_PAGE_TABLE_PAGE_SIZE)
    RuntimePageTableEntry entries[RUNTIME_PAGE_TABLE_ENTRY_COUNT];
} RuntimePageTablePage;

_Static_assert(
    sizeof(RuntimePageTablePage) == RUNTIME_PAGE_TABLE_PAGE_SIZE,
    "translation table page size"
);

typedef struct {
    uint8_t memory_attribute_index;
    bool non_secure;
    RuntimePageTableAccess access;
    RuntimePageTableShareability shareability;
    bool access_flag;
    bool not_global;
    bool privileged_execute_never;
    bool unprivileged_execute_never;
    bool guarded_page;
    bool dirty_bit_management;
    bool contiguous;
} RuntimePageTableAttributes;

typedef struct {
    RuntimePageTableDescriptorKind kind;
    uintptr_t output_address;
    uint64_t attribute_bits;
} RuntimePageTableDescriptor;

typedef struct {
    RuntimePageTablePage *pages;
    uintptr_t physical_base;
    uint8_t page_count;
    uint8_t allocated_count;
    bool allocated[RUNTIME_PAGE_TABLE_POOL_MAX_PAGES];
} RuntimePageTablePool;

typedef struct {
    RuntimePageTablePool *pool;
    RuntimePageTablePage *root;
    uintptr_t root_physical_address;
    uint16_t asid;
    uint8_t virtual_address_bits;
    uint8_t root_level;
} RuntimePageTable;

typedef struct {
    RuntimePageTableEntry *entry;
    RuntimePageTableDescriptor descriptor;
    uint8_t level;
} RuntimePageTableWalk;

typedef struct {
    uintptr_t virtual_address;
    uintptr_t physical_address;
    size_t mapped_size;
    RuntimePageTableAttributes attributes;
    uint8_t level;
} RuntimePageTableTranslation;

uint8_t runtime_page_table_level_shift(uint8_t level);
size_t runtime_page_table_level_span(uint8_t level);
size_t runtime_page_table_level_index(uintptr_t virtual_address, uint8_t level);
uint8_t runtime_page_table_root_level(uint8_t virtual_address_bits);
bool runtime_page_table_address_is_canonical(
    uintptr_t address,
    uint8_t virtual_address_bits
);

RuntimePageTableStatus runtime_page_table_attributes_encode(
    RuntimePageTableAttributes attributes,
    uint64_t *attribute_bits
);
RuntimePageTableStatus runtime_page_table_attributes_decode(
    uint64_t attribute_bits,
    RuntimePageTableAttributes *attributes
);
RuntimePageTableStatus runtime_page_table_descriptor_encode_table(
    uintptr_t next_table_physical_address,
    RuntimePageTableEntry *entry
);
RuntimePageTableStatus runtime_page_table_descriptor_encode_leaf(
    uint8_t level,
    uintptr_t physical_address,
    RuntimePageTableAttributes attributes,
    RuntimePageTableEntry *entry
);
RuntimePageTableStatus runtime_page_table_descriptor_decode(
    RuntimePageTableEntry entry,
    uint8_t level,
    RuntimePageTableDescriptor *descriptor
);

RuntimePageTableStatus runtime_page_table_pool_initialize(
    RuntimePageTablePool *pool,
    RuntimePageTablePage *pages,
    uintptr_t physical_base,
    uint8_t page_count
);
RuntimePageTableStatus runtime_page_table_initialize(
    RuntimePageTable *table,
    RuntimePageTablePool *pool,
    uint8_t virtual_address_bits,
    uint16_t asid
);
RuntimePageTableStatus runtime_page_table_walk(
    RuntimePageTable *table,
    uintptr_t virtual_address,
    uint8_t target_level,
    bool allocate,
    RuntimePageTableWalk *walk
);
RuntimePageTableStatus runtime_page_table_lookup(
    RuntimePageTable *table,
    uintptr_t virtual_address,
    RuntimePageTableTranslation *translation
);
RuntimePageTableStatus runtime_page_table_map(
    RuntimePageTable *table,
    uintptr_t virtual_address,
    uintptr_t physical_address,
    size_t size,
    RuntimePageTableAttributes attributes
);
RuntimePageTableStatus runtime_page_table_unmap(
    RuntimePageTable *table,
    uintptr_t virtual_address,
    size_t size
);
RuntimePageTableStatus runtime_page_table_reprotect(
    RuntimePageTable *table,
    uintptr_t virtual_address,
    size_t size,
    RuntimePageTableAttributes attributes
);

void runtime_page_table_arch_publish(void);
void runtime_page_table_arch_invalidate_all(void);
void runtime_page_table_arch_invalidate_asid(uint16_t asid);
uint64_t runtime_page_table_arch_read_ttbr0_el1(void);
uint64_t runtime_page_table_arch_read_ttbr1_el1(void);
void runtime_page_table_arch_write_ttbr0_el1(uint64_t value);
void runtime_page_table_arch_write_ttbr1_el1(uint64_t value);
uint64_t runtime_page_table_arch_read_sctlr_el1(void);
uint64_t runtime_page_table_arch_read_tcr_el1(void);
uint64_t runtime_page_table_arch_read_mair_el1(void);
uint64_t runtime_page_table_arch_read_id_aa64mmfr0_el1(void);
void runtime_page_table_arch_write_sctlr_el1(uint64_t value);
void runtime_page_table_arch_write_tcr_el1(uint64_t value);
void runtime_page_table_arch_write_mair_el1(uint64_t value);
