#include "runtime_page_table_private.h"


static void runtime_page_table_clear_page(RuntimePageTablePage *page) {
    for (size_t index = 0;
         index < RUNTIME_PAGE_TABLE_ENTRY_COUNT;
         ++index) {
        page->entries[index] = 0;
    }
}

RuntimePageTableStatus runtime_page_table_pool_initialize(
    RuntimePageTablePool *pool,
    RuntimePageTablePage *pages,
    uintptr_t physical_base,
    uint8_t page_count
) {
    const size_t storage_size =
        (size_t)page_count * RUNTIME_PAGE_TABLE_PAGE_SIZE;
    if (pool == 0 || pages == 0 || page_count == 0 ||
        page_count > RUNTIME_PAGE_TABLE_POOL_MAX_PAGES ||
        ((uintptr_t)pages & (RUNTIME_PAGE_TABLE_PAGE_SIZE - 1)) != 0 ||
        (physical_base & (RUNTIME_PAGE_TABLE_PAGE_SIZE - 1)) != 0 ||
        physical_base > RUNTIME_PAGE_TABLE_OUTPUT_ADDRESS_MASK ||
        storage_size - 1 >
            RUNTIME_PAGE_TABLE_PHYSICAL_ADDRESS_MAX - physical_base) {
        return RUNTIME_PAGE_TABLE_STATUS_INVALID_ARGUMENT;
    }
    *pool = (RuntimePageTablePool){
        .pages = pages,
        .physical_base = physical_base,
        .page_count = page_count,
    };
    for (uint8_t index = 0; index < page_count; ++index) {
        runtime_page_table_clear_page(&pages[index]);
    }
    return RUNTIME_PAGE_TABLE_STATUS_OK;
}

RuntimePageTableStatus runtime_page_table_pool_allocate(
    RuntimePageTablePool *pool,
    RuntimePageTablePage **page,
    uintptr_t *physical_address
) {
    for (uint8_t index = 0; index < pool->page_count; ++index) {
        if (!pool->allocated[index]) {
            pool->allocated[index] = true;
            ++pool->allocated_count;
            runtime_page_table_clear_page(&pool->pages[index]);
            *page = &pool->pages[index];
            *physical_address = pool->physical_base +
                (uintptr_t)index * RUNTIME_PAGE_TABLE_PAGE_SIZE;
            return RUNTIME_PAGE_TABLE_STATUS_OK;
        }
    }
    return RUNTIME_PAGE_TABLE_STATUS_OUT_OF_TABLES;
}

RuntimePageTablePage *runtime_page_table_pool_resolve(
    RuntimePageTablePool *pool,
    uintptr_t physical_address
) {
    if (physical_address < pool->physical_base ||
        (physical_address & (RUNTIME_PAGE_TABLE_PAGE_SIZE - 1)) != 0) {
        return 0;
    }
    const uintptr_t offset = physical_address - pool->physical_base;
    const size_t index = offset / RUNTIME_PAGE_TABLE_PAGE_SIZE;
    if (index >= pool->page_count || !pool->allocated[index]) {
        return 0;
    }
    return &pool->pages[index];
}

bool runtime_page_table_page_is_empty(
    const RuntimePageTablePage *page
) {
    for (size_t index = 0;
         index < RUNTIME_PAGE_TABLE_ENTRY_COUNT;
         ++index) {
        if (page->entries[index] != 0) {
            return false;
        }
    }
    return true;
}

RuntimePageTableStatus runtime_page_table_pool_release(
    RuntimePageTablePool *pool,
    RuntimePageTablePage *page
) {
    const size_t index = (size_t)(page - pool->pages);
    if (index >= pool->page_count || !pool->allocated[index] ||
        !runtime_page_table_page_is_empty(page)) {
        return RUNTIME_PAGE_TABLE_STATUS_CORRUPT_DESCRIPTOR;
    }
    pool->allocated[index] = false;
    --pool->allocated_count;
    return RUNTIME_PAGE_TABLE_STATUS_OK;
}

RuntimePageTableStatus runtime_page_table_initialize(
    RuntimePageTable *table,
    RuntimePageTablePool *pool,
    uint8_t virtual_address_bits,
    uint16_t asid
) {
    const uint8_t root_level = runtime_page_table_root_level(
        virtual_address_bits
    );
    if (table == 0 || pool == 0 || pool->pages == 0 ||
        root_level == UINT8_MAX) {
        return RUNTIME_PAGE_TABLE_STATUS_INVALID_ARGUMENT;
    }
    RuntimePageTablePage *root = 0;
    uintptr_t root_physical_address = 0;
    RuntimePageTableStatus status = runtime_page_table_pool_allocate(
        pool,
        &root,
        &root_physical_address
    );
    if (status != RUNTIME_PAGE_TABLE_STATUS_OK) {
        return status;
    }
    *table = (RuntimePageTable){
        .pool = pool,
        .root = root,
        .root_physical_address = root_physical_address,
        .asid = asid,
        .virtual_address_bits = virtual_address_bits,
        .root_level = root_level,
    };
    return RUNTIME_PAGE_TABLE_STATUS_OK;
}

RuntimePageTableStatus runtime_page_table_walk(
    RuntimePageTable *table,
    uintptr_t virtual_address,
    uint8_t target_level,
    bool allocate,
    RuntimePageTableWalk *walk
) {
    if (table == 0 || table->pool == 0 || table->root == 0 || walk == 0 ||
        target_level < table->root_level || target_level > 3 ||
        !runtime_page_table_address_is_canonical(
            virtual_address,
            table->virtual_address_bits
        )) {
        return RUNTIME_PAGE_TABLE_STATUS_INVALID_ARGUMENT;
    }
    RuntimePageTablePage *page = table->root;
    bool allocated_child = false;
    for (uint8_t level = table->root_level; level <= target_level; ++level) {
        RuntimePageTableEntry *entry = &page->entries[
            runtime_page_table_level_index(virtual_address, level)
        ];
        RuntimePageTableDescriptor descriptor;
        RuntimePageTableStatus status = runtime_page_table_descriptor_decode(
            *entry,
            level,
            &descriptor
        );
        if (status != RUNTIME_PAGE_TABLE_STATUS_OK) {
            return status;
        }
        if (level == target_level) {
            *walk = (RuntimePageTableWalk){entry, descriptor, level};
            if (allocated_child) {
                runtime_page_table_arch_publish();
            }
            return RUNTIME_PAGE_TABLE_STATUS_OK;
        }
        if (descriptor.kind == RUNTIME_PAGE_TABLE_DESCRIPTOR_KIND_INVALID) {
            if (!allocate) {
                return RUNTIME_PAGE_TABLE_STATUS_NOT_MAPPED;
            }
            const uint8_t required_pages = (uint8_t)(target_level - level);
            const uint8_t free_pages = (uint8_t)(
                table->pool->page_count - table->pool->allocated_count
            );
            if (free_pages < required_pages) {
                return RUNTIME_PAGE_TABLE_STATUS_OUT_OF_TABLES;
            }
            RuntimePageTablePage *child = 0;
            uintptr_t child_physical_address = 0;
            status = runtime_page_table_pool_allocate(
                table->pool,
                &child,
                &child_physical_address
            );
            if (status != RUNTIME_PAGE_TABLE_STATUS_OK) {
                return status;
            }
            status = runtime_page_table_descriptor_encode_table(
                child_physical_address,
                entry
            );
            if (status != RUNTIME_PAGE_TABLE_STATUS_OK) {
                return status;
            }
            allocated_child = true;
            page = child;
            continue;
        }
        if (descriptor.kind != RUNTIME_PAGE_TABLE_DESCRIPTOR_KIND_TABLE) {
            return RUNTIME_PAGE_TABLE_STATUS_CONFLICT;
        }
        page = runtime_page_table_pool_resolve(
            table->pool,
            descriptor.output_address
        );
        if (page == 0) {
            return RUNTIME_PAGE_TABLE_STATUS_CORRUPT_DESCRIPTOR;
        }
    }
    return RUNTIME_PAGE_TABLE_STATUS_CORRUPT_DESCRIPTOR;
}

RuntimePageTableStatus runtime_page_table_lookup(
    RuntimePageTable *table,
    uintptr_t virtual_address,
    RuntimePageTableTranslation *translation
) {
    if (table == 0 || translation == 0 ||
        !runtime_page_table_address_is_canonical(
            virtual_address,
            table->virtual_address_bits
        )) {
        return RUNTIME_PAGE_TABLE_STATUS_INVALID_ARGUMENT;
    }
    RuntimePageTablePage *page = table->root;
    for (uint8_t level = table->root_level; level <= 3; ++level) {
        const RuntimePageTableEntry entry = page->entries[
            runtime_page_table_level_index(virtual_address, level)
        ];
        RuntimePageTableDescriptor descriptor;
        RuntimePageTableStatus status = runtime_page_table_descriptor_decode(
            entry,
            level,
            &descriptor
        );
        if (status != RUNTIME_PAGE_TABLE_STATUS_OK) {
            return status;
        }
        if (descriptor.kind == RUNTIME_PAGE_TABLE_DESCRIPTOR_KIND_INVALID) {
            return RUNTIME_PAGE_TABLE_STATUS_NOT_MAPPED;
        }
        if (descriptor.kind == RUNTIME_PAGE_TABLE_DESCRIPTOR_KIND_TABLE) {
            page = runtime_page_table_pool_resolve(
                table->pool,
                descriptor.output_address
            );
            if (page == 0) {
                return RUNTIME_PAGE_TABLE_STATUS_CORRUPT_DESCRIPTOR;
            }
            continue;
        }
        RuntimePageTableAttributes attributes;
        status = runtime_page_table_attributes_decode(
            descriptor.attribute_bits,
            &attributes
        );
        if (status != RUNTIME_PAGE_TABLE_STATUS_OK) {
            return status;
        }
        const size_t span = runtime_page_table_level_span(level);
        *translation = (RuntimePageTableTranslation){
            .virtual_address = virtual_address & ~(uintptr_t)(span - 1),
            .physical_address = descriptor.output_address |
                (virtual_address & (span - 1)),
            .mapped_size = span,
            .attributes = attributes,
            .level = level,
        };
        return RUNTIME_PAGE_TABLE_STATUS_OK;
    }
    return RUNTIME_PAGE_TABLE_STATUS_CORRUPT_DESCRIPTOR;
}

