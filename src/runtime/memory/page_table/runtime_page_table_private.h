#pragma once

#include "avp/runtime/memory/runtime_page_table.h"


#define RUNTIME_PAGE_TABLE_OUTPUT_ADDRESS_MASK \
    UINT64_C(0x0000ffffffffc000)

uint8_t runtime_page_table_canonical_region(
    uintptr_t address,
    uint8_t virtual_address_bits
);
RuntimePageTableStatus runtime_page_table_pool_allocate(
    RuntimePageTablePool *pool,
    RuntimePageTablePage **page,
    uintptr_t *physical_address
);
RuntimePageTablePage *runtime_page_table_pool_resolve(
    RuntimePageTablePool *pool,
    uintptr_t physical_address
);
bool runtime_page_table_page_is_empty(const RuntimePageTablePage *page);
RuntimePageTableStatus runtime_page_table_pool_release(
    RuntimePageTablePool *pool,
    RuntimePageTablePage *page
);
