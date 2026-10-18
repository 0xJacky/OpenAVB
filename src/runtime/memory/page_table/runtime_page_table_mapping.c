#include "runtime_page_table_private.h"


typedef struct {
    uintptr_t virtual_prefix;
    uint8_t level;
} RuntimePageTablePendingTable;

static RuntimePageTableStatus runtime_page_table_validate_range(
    const RuntimePageTable *table,
    uintptr_t virtual_address,
    uintptr_t physical_address,
    size_t size,
    bool has_physical_address
) {
    if (table == 0 || size == 0 ||
        (virtual_address & (RUNTIME_PAGE_TABLE_PAGE_SIZE - 1)) != 0 ||
        (size & (RUNTIME_PAGE_TABLE_PAGE_SIZE - 1)) != 0 ||
        virtual_address > UINTPTR_MAX - (size - 1)) {
        return RUNTIME_PAGE_TABLE_STATUS_INVALID_ARGUMENT;
    }
    const uintptr_t last_virtual_address = virtual_address + size - 1;
    const uint8_t region = runtime_page_table_canonical_region(
        virtual_address,
        table->virtual_address_bits
    );
    if (region == 0 || region != runtime_page_table_canonical_region(
            last_virtual_address,
            table->virtual_address_bits
        )) {
        return RUNTIME_PAGE_TABLE_STATUS_INVALID_ARGUMENT;
    }
    if (has_physical_address &&
        ((physical_address & (RUNTIME_PAGE_TABLE_PAGE_SIZE - 1)) != 0 ||
         physical_address > RUNTIME_PAGE_TABLE_OUTPUT_ADDRESS_MASK ||
         size - 1 >
            RUNTIME_PAGE_TABLE_PHYSICAL_ADDRESS_MAX - physical_address)) {
        return RUNTIME_PAGE_TABLE_STATUS_INVALID_ARGUMENT;
    }
    return RUNTIME_PAGE_TABLE_STATUS_OK;
}

static RuntimePageTableStatus runtime_page_table_mapping_level_available(
    RuntimePageTable *table,
    uintptr_t virtual_address,
    uint8_t target_level,
    bool *available
) {
    RuntimePageTablePage *page = table->root;
    for (uint8_t level = table->root_level; level <= target_level; ++level) {
        RuntimePageTableDescriptor descriptor;
        RuntimePageTableStatus status = runtime_page_table_descriptor_decode(
            page->entries[
                runtime_page_table_level_index(virtual_address, level)
            ],
            level,
            &descriptor
        );
        if (status != RUNTIME_PAGE_TABLE_STATUS_OK) {
            return status;
        }
        if (descriptor.kind == RUNTIME_PAGE_TABLE_DESCRIPTOR_KIND_INVALID) {
            *available = true;
            return RUNTIME_PAGE_TABLE_STATUS_OK;
        }
        if (descriptor.kind != RUNTIME_PAGE_TABLE_DESCRIPTOR_KIND_TABLE) {
            return RUNTIME_PAGE_TABLE_STATUS_CONFLICT;
        }
        if (level == target_level) {
            *available = false;
            return RUNTIME_PAGE_TABLE_STATUS_OK;
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

static RuntimePageTableStatus runtime_page_table_select_mapping_level(
    RuntimePageTable *table,
    uintptr_t virtual_address,
    uintptr_t physical_address,
    size_t remaining,
    uint8_t *selected_level
) {
    uint8_t first_level = table->root_level;
    if (first_level == 0) {
        first_level = 1;
    }
    for (uint8_t level = first_level; level <= 3; ++level) {
        const size_t span = runtime_page_table_level_span(level);
        if (remaining < span || (virtual_address & (span - 1)) != 0 ||
            (physical_address & (span - 1)) != 0) {
            continue;
        }
        bool available = false;
        RuntimePageTableStatus status =
            runtime_page_table_mapping_level_available(
                table,
                virtual_address,
                level,
                &available
            );
        if (status != RUNTIME_PAGE_TABLE_STATUS_OK) {
            return status;
        }
        if (available) {
            *selected_level = level;
            return RUNTIME_PAGE_TABLE_STATUS_OK;
        }
    }
    return RUNTIME_PAGE_TABLE_STATUS_CONFLICT;
}

static uintptr_t runtime_page_table_table_prefix(
    uintptr_t virtual_address,
    uint8_t level
) {
    const uint8_t coverage_shift = (uint8_t)(
        runtime_page_table_level_shift(level) + 11
    );
    return virtual_address & ~(((uintptr_t)1 << coverage_shift) - 1);
}

static bool runtime_page_table_pending_contains(
    const RuntimePageTablePendingTable *pending,
    uint8_t pending_count,
    uintptr_t virtual_prefix,
    uint8_t level
) {
    for (uint8_t index = 0; index < pending_count; ++index) {
        if (pending[index].virtual_prefix == virtual_prefix &&
            pending[index].level == level) {
            return true;
        }
    }
    return false;
}

static RuntimePageTableStatus runtime_page_table_collect_pending(
    RuntimePageTable *table,
    uintptr_t virtual_address,
    uint8_t target_level,
    RuntimePageTablePendingTable *pending,
    uint8_t *pending_count
) {
    RuntimePageTablePage *page = table->root;
    for (uint8_t level = table->root_level; level < target_level; ++level) {
        RuntimePageTableDescriptor descriptor;
        RuntimePageTableStatus status = runtime_page_table_descriptor_decode(
            page->entries[
                runtime_page_table_level_index(virtual_address, level)
            ],
            level,
            &descriptor
        );
        if (status != RUNTIME_PAGE_TABLE_STATUS_OK) {
            return status;
        }
        if (descriptor.kind == RUNTIME_PAGE_TABLE_DESCRIPTOR_KIND_INVALID) {
            for (uint8_t child_level = (uint8_t)(level + 1);
                 child_level <= target_level;
                 ++child_level) {
                const uintptr_t prefix = runtime_page_table_table_prefix(
                    virtual_address,
                    child_level
                );
                if (!runtime_page_table_pending_contains(
                        pending,
                        *pending_count,
                        prefix,
                        child_level
                    )) {
                    if (*pending_count ==
                            RUNTIME_PAGE_TABLE_POOL_MAX_PAGES) {
                        return RUNTIME_PAGE_TABLE_STATUS_OUT_OF_TABLES;
                    }
                    pending[*pending_count] =
                        (RuntimePageTablePendingTable){prefix, child_level};
                    ++*pending_count;
                }
            }
            return RUNTIME_PAGE_TABLE_STATUS_OK;
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
    return RUNTIME_PAGE_TABLE_STATUS_OK;
}

static uint8_t runtime_page_table_pool_free_count(
    const RuntimePageTablePool *pool
) {
    return (uint8_t)(pool->page_count - pool->allocated_count);
}

RuntimePageTableStatus runtime_page_table_map(
    RuntimePageTable *table,
    uintptr_t virtual_address,
    uintptr_t physical_address,
    size_t size,
    RuntimePageTableAttributes attributes
) {
    RuntimePageTableStatus status = runtime_page_table_validate_range(
        table,
        virtual_address,
        physical_address,
        size,
        true
    );
    uint64_t ignored_attribute_bits = 0;
    if (status != RUNTIME_PAGE_TABLE_STATUS_OK ||
        runtime_page_table_attributes_encode(
            attributes,
            &ignored_attribute_bits
        ) != RUNTIME_PAGE_TABLE_STATUS_OK) {
        return RUNTIME_PAGE_TABLE_STATUS_INVALID_ARGUMENT;
    }
    RuntimePageTablePendingTable pending[
        RUNTIME_PAGE_TABLE_POOL_MAX_PAGES
    ];
    /* Preflight keeps a finite bootstrap pool from exposing a partial map. */
    uint8_t pending_count = 0;
    uintptr_t cursor = virtual_address;
    uintptr_t physical_cursor = physical_address;
    size_t remaining = size;
    while (remaining != 0) {
        uint8_t level = 0;
        status = runtime_page_table_select_mapping_level(
            table,
            cursor,
            physical_cursor,
            remaining,
            &level
        );
        if (status != RUNTIME_PAGE_TABLE_STATUS_OK) {
            return status;
        }
        status = runtime_page_table_collect_pending(
            table,
            cursor,
            level,
            pending,
            &pending_count
        );
        if (status != RUNTIME_PAGE_TABLE_STATUS_OK) {
            return status;
        }
        const size_t span = runtime_page_table_level_span(level);
        cursor += span;
        physical_cursor += span;
        remaining -= span;
    }
    if (pending_count > runtime_page_table_pool_free_count(table->pool)) {
        return RUNTIME_PAGE_TABLE_STATUS_OUT_OF_TABLES;
    }

    cursor = virtual_address;
    physical_cursor = physical_address;
    remaining = size;
    while (remaining != 0) {
        uint8_t level = 0;
        status = runtime_page_table_select_mapping_level(
            table,
            cursor,
            physical_cursor,
            remaining,
            &level
        );
        if (status != RUNTIME_PAGE_TABLE_STATUS_OK) {
            return status;
        }
        RuntimePageTableWalk walk;
        status = runtime_page_table_walk(
            table,
            cursor,
            level,
            true,
            &walk
        );
        if (status != RUNTIME_PAGE_TABLE_STATUS_OK ||
            walk.descriptor.kind !=
                RUNTIME_PAGE_TABLE_DESCRIPTOR_KIND_INVALID) {
            return status == RUNTIME_PAGE_TABLE_STATUS_OK ?
                RUNTIME_PAGE_TABLE_STATUS_CONFLICT : status;
        }
        status = runtime_page_table_descriptor_encode_leaf(
            level,
            physical_cursor,
            attributes,
            walk.entry
        );
        if (status != RUNTIME_PAGE_TABLE_STATUS_OK) {
            return status;
        }
        const size_t span = runtime_page_table_level_span(level);
        cursor += span;
        physical_cursor += span;
        remaining -= span;
    }
    runtime_page_table_arch_publish();
    return RUNTIME_PAGE_TABLE_STATUS_OK;
}

static RuntimePageTableStatus runtime_page_table_require_mapped_range(
    RuntimePageTable *table,
    uintptr_t virtual_address,
    size_t size
) {
    uintptr_t cursor = virtual_address;
    size_t remaining = size;
    while (remaining != 0) {
        RuntimePageTableTranslation translation;
        RuntimePageTableStatus status = runtime_page_table_lookup(
            table,
            cursor,
            &translation
        );
        if (status != RUNTIME_PAGE_TABLE_STATUS_OK) {
            return status;
        }
        const size_t offset = (size_t)(
            cursor - translation.virtual_address
        );
        const size_t available = translation.mapped_size - offset;
        const size_t consumed = available < remaining ?
            available : remaining;
        remaining -= consumed;
        if (remaining != 0) {
            cursor += consumed;
        }
    }
    return RUNTIME_PAGE_TABLE_STATUS_OK;
}

static size_t runtime_page_table_split_count(
    uint8_t level,
    size_t offset,
    size_t size
) {
    const size_t span = runtime_page_table_level_span(level);
    if ((offset == 0 && size == span) || level == 3) {
        return 0;
    }

    const uint8_t child_level = (uint8_t)(level + 1);
    const size_t child_span = runtime_page_table_level_span(child_level);
    const size_t upper_bound = offset + size;
    const size_t first_child = offset / child_span;
    const size_t last_child = (upper_bound - 1) / child_span;
    size_t count = 1;
    for (size_t child = first_child; child <= last_child; ++child) {
        const size_t child_lower = child * child_span;
        const size_t overlap_lower = offset > child_lower ?
            offset : child_lower;
        const size_t child_upper = child_lower + child_span;
        const size_t overlap_upper = upper_bound < child_upper ?
            upper_bound : child_upper;
        count += runtime_page_table_split_count(
            child_level,
            overlap_lower - child_lower,
            overlap_upper - overlap_lower
        );
    }
    return count;
}

static RuntimePageTableStatus runtime_page_table_require_split_capacity(
    RuntimePageTable *table,
    uintptr_t virtual_address,
    size_t size
) {
    /* Boundary blocks survive mutation, so every required split stays live. */
    uintptr_t cursor = virtual_address;
    size_t remaining = size;
    size_t required_pages = 0;
    while (remaining != 0) {
        RuntimePageTableTranslation translation;
        RuntimePageTableStatus status = runtime_page_table_lookup(
            table,
            cursor,
            &translation
        );
        if (status != RUNTIME_PAGE_TABLE_STATUS_OK) {
            return status;
        }
        const size_t offset = (size_t)(
            cursor - translation.virtual_address
        );
        const size_t available = translation.mapped_size - offset;
        const size_t consumed = available < remaining ?
            available : remaining;
        required_pages += runtime_page_table_split_count(
            translation.level,
            offset,
            consumed
        );
        if (required_pages > runtime_page_table_pool_free_count(
                table->pool
            )) {
            return RUNTIME_PAGE_TABLE_STATUS_OUT_OF_TABLES;
        }
        remaining -= consumed;
        if (remaining != 0) {
            cursor += consumed;
        }
    }
    return RUNTIME_PAGE_TABLE_STATUS_OK;
}

static RuntimePageTableStatus runtime_page_table_split_leaf(
    RuntimePageTable *table,
    uintptr_t virtual_address,
    uint8_t level
) {
    RuntimePageTableWalk walk;
    RuntimePageTableStatus status = runtime_page_table_walk(
        table,
        virtual_address,
        level,
        false,
        &walk
    );
    if (status != RUNTIME_PAGE_TABLE_STATUS_OK || level >= 3 ||
        walk.descriptor.kind != RUNTIME_PAGE_TABLE_DESCRIPTOR_KIND_BLOCK) {
        return status == RUNTIME_PAGE_TABLE_STATUS_OK ?
            RUNTIME_PAGE_TABLE_STATUS_CONFLICT : status;
    }
    RuntimePageTableAttributes attributes;
    status = runtime_page_table_attributes_decode(
        walk.descriptor.attribute_bits,
        &attributes
    );
    if (status != RUNTIME_PAGE_TABLE_STATUS_OK) {
        return status;
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
    const uint8_t child_level = (uint8_t)(level + 1);
    const size_t child_span = runtime_page_table_level_span(child_level);
    uintptr_t output_address = walk.descriptor.output_address;
    for (size_t index = 0;
         index < RUNTIME_PAGE_TABLE_ENTRY_COUNT;
         ++index) {
        status = runtime_page_table_descriptor_encode_leaf(
            child_level,
            output_address,
            attributes,
            &child->entries[index]
        );
        if (status != RUNTIME_PAGE_TABLE_STATUS_OK) {
            return status;
        }
        output_address += child_span;
    }
    /* ARM requires break-before-make when a live block becomes a table. */
    *walk.entry = 0;
    runtime_page_table_arch_invalidate_all();
    status = runtime_page_table_descriptor_encode_table(
        child_physical_address,
        walk.entry
    );
    if (status != RUNTIME_PAGE_TABLE_STATUS_OK) {
        return status;
    }
    runtime_page_table_arch_publish();
    return RUNTIME_PAGE_TABLE_STATUS_OK;
}

static RuntimePageTableStatus runtime_page_table_collect_path(
    RuntimePageTable *table,
    uintptr_t virtual_address,
    uint8_t target_level,
    RuntimePageTablePage *pages[4],
    RuntimePageTableEntry *entries[4]
) {
    RuntimePageTablePage *page = table->root;
    for (uint8_t level = table->root_level; level <= target_level; ++level) {
        pages[level] = page;
        entries[level] = &page->entries[
            runtime_page_table_level_index(virtual_address, level)
        ];
        if (level == target_level) {
            return RUNTIME_PAGE_TABLE_STATUS_OK;
        }
        RuntimePageTableDescriptor descriptor;
        RuntimePageTableStatus status = runtime_page_table_descriptor_decode(
            *entries[level],
            level,
            &descriptor
        );
        if (status != RUNTIME_PAGE_TABLE_STATUS_OK ||
            descriptor.kind != RUNTIME_PAGE_TABLE_DESCRIPTOR_KIND_TABLE) {
            return status == RUNTIME_PAGE_TABLE_STATUS_OK ?
                RUNTIME_PAGE_TABLE_STATUS_CORRUPT_DESCRIPTOR : status;
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

static RuntimePageTableStatus runtime_page_table_prune_empty_path(
    RuntimePageTable *table,
    uintptr_t virtual_address,
    uint8_t cleared_level
) {
    RuntimePageTablePage *pages[4] = {0};
    RuntimePageTableEntry *entries[4] = {0};
    RuntimePageTableStatus status = runtime_page_table_collect_path(
            table,
            virtual_address,
            cleared_level,
            pages,
            entries
        );
    if (status != RUNTIME_PAGE_TABLE_STATUS_OK) {
        return status;
    }
    for (uint8_t level = cleared_level;
         level > table->root_level;
         --level) {
        if (!runtime_page_table_page_is_empty(pages[level])) {
            break;
        }
        *entries[level - 1] = 0;
        runtime_page_table_arch_invalidate_all();
        status = runtime_page_table_pool_release(table->pool, pages[level]);
        if (status != RUNTIME_PAGE_TABLE_STATUS_OK) {
            return status;
        }
    }
    return RUNTIME_PAGE_TABLE_STATUS_OK;
}

RuntimePageTableStatus runtime_page_table_unmap(
    RuntimePageTable *table,
    uintptr_t virtual_address,
    size_t size
) {
    RuntimePageTableStatus status = runtime_page_table_validate_range(
        table,
        virtual_address,
        0,
        size,
        false
    );
    if (status != RUNTIME_PAGE_TABLE_STATUS_OK) {
        return status;
    }
    status = runtime_page_table_require_mapped_range(
        table,
        virtual_address,
        size
    );
    if (status != RUNTIME_PAGE_TABLE_STATUS_OK) {
        return status;
    }
    status = runtime_page_table_require_split_capacity(
        table,
        virtual_address,
        size
    );
    if (status != RUNTIME_PAGE_TABLE_STATUS_OK) {
        return status;
    }
    uintptr_t cursor = virtual_address;
    size_t remaining = size;
    while (remaining != 0) {
        RuntimePageTableTranslation translation;
        status = runtime_page_table_lookup(table, cursor, &translation);
        if (status != RUNTIME_PAGE_TABLE_STATUS_OK) {
            return status;
        }
        if (cursor != translation.virtual_address ||
            remaining < translation.mapped_size) {
            status = runtime_page_table_split_leaf(
                table,
                cursor,
                translation.level
            );
            if (status != RUNTIME_PAGE_TABLE_STATUS_OK) {
                return status;
            }
            continue;
        }
        RuntimePageTableWalk walk;
        status = runtime_page_table_walk(
            table,
            cursor,
            translation.level,
            false,
            &walk
        );
        if (status != RUNTIME_PAGE_TABLE_STATUS_OK) {
            return status;
        }
        *walk.entry = 0;
        runtime_page_table_arch_invalidate_all();
        status = runtime_page_table_prune_empty_path(
            table,
            cursor,
            translation.level
        );
        if (status != RUNTIME_PAGE_TABLE_STATUS_OK) {
            return status;
        }
        remaining -= translation.mapped_size;
        if (remaining != 0) {
            cursor += translation.mapped_size;
        }
    }
    return RUNTIME_PAGE_TABLE_STATUS_OK;
}

RuntimePageTableStatus runtime_page_table_reprotect(
    RuntimePageTable *table,
    uintptr_t virtual_address,
    size_t size,
    RuntimePageTableAttributes attributes
) {
    RuntimePageTableStatus status = runtime_page_table_validate_range(
        table,
        virtual_address,
        0,
        size,
        false
    );
    uint64_t ignored_attribute_bits = 0;
    if (status != RUNTIME_PAGE_TABLE_STATUS_OK ||
        runtime_page_table_attributes_encode(
            attributes,
            &ignored_attribute_bits
        ) != RUNTIME_PAGE_TABLE_STATUS_OK) {
        return RUNTIME_PAGE_TABLE_STATUS_INVALID_ARGUMENT;
    }
    status = runtime_page_table_require_mapped_range(
        table,
        virtual_address,
        size
    );
    if (status != RUNTIME_PAGE_TABLE_STATUS_OK) {
        return status;
    }
    status = runtime_page_table_require_split_capacity(
        table,
        virtual_address,
        size
    );
    if (status != RUNTIME_PAGE_TABLE_STATUS_OK) {
        return status;
    }
    uintptr_t cursor = virtual_address;
    size_t remaining = size;
    while (remaining != 0) {
        RuntimePageTableTranslation translation;
        status = runtime_page_table_lookup(table, cursor, &translation);
        if (status != RUNTIME_PAGE_TABLE_STATUS_OK) {
            return status;
        }
        if (cursor != translation.virtual_address ||
            remaining < translation.mapped_size) {
            status = runtime_page_table_split_leaf(
                table,
                cursor,
                translation.level
            );
            if (status != RUNTIME_PAGE_TABLE_STATUS_OK) {
                return status;
            }
            continue;
        }
        RuntimePageTableWalk walk;
        status = runtime_page_table_walk(
            table,
            cursor,
            translation.level,
            false,
            &walk
        );
        if (status != RUNTIME_PAGE_TABLE_STATUS_OK) {
            return status;
        }
        RuntimePageTableEntry replacement = 0;
        status = runtime_page_table_descriptor_encode_leaf(
            translation.level,
            translation.physical_address,
            attributes,
            &replacement
        );
        if (status != RUNTIME_PAGE_TABLE_STATUS_OK) {
            return status;
        }
        if (*walk.entry != replacement) {
            *walk.entry = 0;
            runtime_page_table_arch_invalidate_all();
            *walk.entry = replacement;
            runtime_page_table_arch_publish();
        }
        remaining -= translation.mapped_size;
        if (remaining != 0) {
            cursor += translation.mapped_size;
        }
    }
    return RUNTIME_PAGE_TABLE_STATUS_OK;
}

