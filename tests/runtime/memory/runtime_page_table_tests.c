#include "avp/runtime/memory/runtime_page_table.h"

#include <assert.h>
#include <stdio.h>


static unsigned publish_count;
static unsigned invalidate_count;

void runtime_page_table_arch_publish(void) {
    ++publish_count;
}

void runtime_page_table_arch_invalidate_all(void) {
    ++invalidate_count;
}

static RuntimePageTableAttributes normal_memory(void) {
    return (RuntimePageTableAttributes){
        .memory_attribute_index = 4,
        .access = RUNTIME_PAGE_TABLE_ACCESS_PRIVILEGED_READ_WRITE,
        .shareability = RUNTIME_PAGE_TABLE_SHAREABILITY_INNER,
        .access_flag = true,
        .privileged_execute_never = true,
        .unprivileged_execute_never = true,
    };
}

static RuntimePageTableAttributes read_only_memory(void) {
    RuntimePageTableAttributes attributes = normal_memory();
    attributes.access = RUNTIME_PAGE_TABLE_ACCESS_PRIVILEGED_READ_ONLY;
    return attributes;
}

static void assert_attributes_equal(
    RuntimePageTableAttributes left,
    RuntimePageTableAttributes right
) {
    assert(left.memory_attribute_index == right.memory_attribute_index);
    assert(left.non_secure == right.non_secure);
    assert(left.access == right.access);
    assert(left.shareability == right.shareability);
    assert(left.access_flag == right.access_flag);
    assert(left.not_global == right.not_global);
    assert(left.privileged_execute_never ==
        right.privileged_execute_never);
    assert(left.unprivileged_execute_never ==
        right.unprivileged_execute_never);
    assert(left.guarded_page == right.guarded_page);
    assert(left.dirty_bit_management == right.dirty_bit_management);
    assert(left.contiguous == right.contiguous);
}

static void test_geometry(void) {
    assert(RUNTIME_PAGE_TABLE_PAGE_SIZE == UINT64_C(0x4000));
    assert(RUNTIME_PAGE_TABLE_ENTRY_COUNT == 2048);
    assert(runtime_page_table_level_shift(0) == 47);
    assert(runtime_page_table_level_shift(1) == 36);
    assert(runtime_page_table_level_shift(2) == 25);
    assert(runtime_page_table_level_shift(3) == 14);
    assert(runtime_page_table_level_span(1) == UINT64_C(0x1000000000));
    assert(runtime_page_table_level_span(2) == UINT64_C(0x02000000));
    assert(runtime_page_table_level_span(3) == UINT64_C(0x00004000));
    assert(runtime_page_table_level_span(4) == 0);
    assert(runtime_page_table_level_index(
        UINT64_C(0x0000000a46800000), 2) == 0x523);
    assert(runtime_page_table_root_level(15) == 3);
    assert(runtime_page_table_root_level(25) == 3);
    assert(runtime_page_table_root_level(26) == 2);
    assert(runtime_page_table_root_level(36) == 2);
    assert(runtime_page_table_root_level(37) == 1);
    assert(runtime_page_table_root_level(47) == 1);
    assert(runtime_page_table_root_level(48) == 0);
    assert(runtime_page_table_root_level(14) == UINT8_MAX);

    assert(runtime_page_table_address_is_canonical(0, 42));
    assert(runtime_page_table_address_is_canonical(
        (UINT64_C(1) << 42) - 1, 42));
    assert(runtime_page_table_address_is_canonical(
        UINT64_MAX - (UINT64_C(1) << 42) + 1, 42));
    assert(!runtime_page_table_address_is_canonical(
        UINT64_C(1) << 42, 42));
}

static void test_descriptors(void) {
    const uintptr_t last_page_address =
        RUNTIME_PAGE_TABLE_PHYSICAL_ADDRESS_MAX &
        ~(uintptr_t)(RUNTIME_PAGE_TABLE_PAGE_SIZE - 1);
    RuntimePageTableAttributes attributes = normal_memory();
    attributes.non_secure = true;
    attributes.not_global = true;
    attributes.guarded_page = true;
    attributes.dirty_bit_management = true;
    attributes.contiguous = true;

    uint64_t attribute_bits = 0;
    assert(runtime_page_table_attributes_encode(
        attributes, &attribute_bits) == RUNTIME_PAGE_TABLE_STATUS_OK);
    RuntimePageTableAttributes decoded_attributes;
    assert(runtime_page_table_attributes_decode(
        attribute_bits,
        &decoded_attributes
    ) == RUNTIME_PAGE_TABLE_STATUS_OK);
    assert_attributes_equal(attributes, decoded_attributes);

    RuntimePageTableEntry entry = 0;
    assert(runtime_page_table_descriptor_encode_table(
        UINT64_C(0x12344000),
        &entry
    ) == RUNTIME_PAGE_TABLE_STATUS_OK);
    RuntimePageTableDescriptor descriptor;
    assert(runtime_page_table_descriptor_decode(
        entry, 1, &descriptor) == RUNTIME_PAGE_TABLE_STATUS_OK);
    assert(descriptor.kind == RUNTIME_PAGE_TABLE_DESCRIPTOR_KIND_TABLE);
    assert(descriptor.output_address == UINT64_C(0x12344000));

    assert(runtime_page_table_descriptor_encode_leaf(
        2,
        UINT64_C(0x80000000),
        attributes,
        &entry
    ) == RUNTIME_PAGE_TABLE_STATUS_OK);
    assert(runtime_page_table_descriptor_decode(
        entry, 2, &descriptor) == RUNTIME_PAGE_TABLE_STATUS_OK);
    assert(descriptor.kind == RUNTIME_PAGE_TABLE_DESCRIPTOR_KIND_BLOCK);
    assert(descriptor.output_address == UINT64_C(0x80000000));
    assert(descriptor.attribute_bits == attribute_bits);

    assert(runtime_page_table_descriptor_encode_leaf(
        3,
        last_page_address,
        attributes,
        &entry
    ) == RUNTIME_PAGE_TABLE_STATUS_OK);
    assert(runtime_page_table_descriptor_decode(
        entry, 3, &descriptor) == RUNTIME_PAGE_TABLE_STATUS_OK);
    assert(descriptor.kind == RUNTIME_PAGE_TABLE_DESCRIPTOR_KIND_PAGE);
    assert(descriptor.output_address == last_page_address);

    assert(runtime_page_table_descriptor_decode(
        UINT64_C(2), 3, &descriptor) ==
        RUNTIME_PAGE_TABLE_STATUS_CORRUPT_DESCRIPTOR);
    assert(runtime_page_table_descriptor_encode_leaf(
        2,
        UINT64_C(0x80004000),
        attributes,
        &entry
    ) == RUNTIME_PAGE_TABLE_STATUS_INVALID_ARGUMENT);
}

static void initialize_table(
    RuntimePageTablePage *pages,
    uint8_t page_count,
    RuntimePageTablePool *pool,
    RuntimePageTable *table
) {
    assert(runtime_page_table_pool_initialize(
        pool,
        pages,
        UINT64_C(0x10000000),
        page_count
    ) == RUNTIME_PAGE_TABLE_STATUS_OK);
    assert(runtime_page_table_initialize(
        table,
        pool,
        42,
        7
    ) == RUNTIME_PAGE_TABLE_STATUS_OK);
}

static void test_page_mapping(void) {
    static RuntimePageTablePage pages[8];
    RuntimePageTablePool pool;
    RuntimePageTable table;
    initialize_table(pages, 8, &pool, &table);
    publish_count = 0;

    const RuntimePageTableAttributes attributes = normal_memory();
    assert(runtime_page_table_map(
        &table,
        UINT64_C(0x00400000),
        UINT64_C(0x00800000),
        2 * RUNTIME_PAGE_TABLE_PAGE_SIZE,
        attributes
    ) == RUNTIME_PAGE_TABLE_STATUS_OK);
    assert(pool.allocated_count == 3);
    assert(publish_count == 2);

    RuntimePageTableTranslation translation;
    assert(runtime_page_table_lookup(
        &table,
        UINT64_C(0x00404123),
        &translation
    ) == RUNTIME_PAGE_TABLE_STATUS_OK);
    assert(translation.virtual_address == UINT64_C(0x00404000));
    assert(translation.physical_address == UINT64_C(0x00804123));
    assert(translation.mapped_size == RUNTIME_PAGE_TABLE_PAGE_SIZE);
    assert(translation.level == 3);
    assert_attributes_equal(attributes, translation.attributes);

    assert(runtime_page_table_map(
        &table,
        UINT64_C(0x00400000),
        UINT64_C(0x00800000),
        RUNTIME_PAGE_TABLE_PAGE_SIZE,
        attributes
    ) == RUNTIME_PAGE_TABLE_STATUS_CONFLICT);
}

static void test_mapping_pool_exhaustion_is_atomic(void) {
    static RuntimePageTablePage pages[2];
    RuntimePageTablePool pool;
    RuntimePageTable table;
    initialize_table(pages, 2, &pool, &table);

    assert(runtime_page_table_map(
        &table,
        UINT64_C(0x00400000),
        UINT64_C(0x00800000),
        RUNTIME_PAGE_TABLE_PAGE_SIZE,
        normal_memory()
    ) == RUNTIME_PAGE_TABLE_STATUS_OUT_OF_TABLES);
    assert(pool.allocated_count == 1);
    RuntimePageTableTranslation translation;
    assert(runtime_page_table_lookup(
        &table,
        UINT64_C(0x00400000),
        &translation
    ) == RUNTIME_PAGE_TABLE_STATUS_NOT_MAPPED);
}

static void test_walk_pool_exhaustion_is_atomic(void) {
    static RuntimePageTablePage pages[2];
    RuntimePageTablePool pool;
    RuntimePageTable table;
    initialize_table(pages, 2, &pool, &table);

    RuntimePageTableWalk walk;
    assert(runtime_page_table_walk(
        &table,
        UINT64_C(0x00400000),
        3,
        true,
        &walk
    ) == RUNTIME_PAGE_TABLE_STATUS_OUT_OF_TABLES);
    assert(pool.allocated_count == 1);
    assert(table.root->entries[
        runtime_page_table_level_index(UINT64_C(0x00400000), 1)
    ] == 0);
}

static void test_split_pool_exhaustion_is_atomic(void) {
    static RuntimePageTablePage pages[3];
    RuntimePageTablePool pool;
    RuntimePageTable table;
    initialize_table(pages, 3, &pool, &table);

    const uintptr_t virtual_address = UINT64_C(0x40000000);
    const size_t block_span = runtime_page_table_level_span(2);
    assert(runtime_page_table_map(
        &table,
        virtual_address,
        UINT64_C(0x80000000),
        2 * block_span,
        normal_memory()
    ) == RUNTIME_PAGE_TABLE_STATUS_OK);
    assert(pool.allocated_count == 2);

    const size_t partial_size =
        2 * block_span - 2 * RUNTIME_PAGE_TABLE_PAGE_SIZE;
    assert(runtime_page_table_unmap(
        &table,
        virtual_address + RUNTIME_PAGE_TABLE_PAGE_SIZE,
        partial_size
    ) == RUNTIME_PAGE_TABLE_STATUS_OUT_OF_TABLES);
    assert(pool.allocated_count == 2);
    RuntimePageTableTranslation translation;
    assert(runtime_page_table_lookup(
        &table,
        virtual_address + RUNTIME_PAGE_TABLE_PAGE_SIZE,
        &translation
    ) == RUNTIME_PAGE_TABLE_STATUS_OK);
    assert(translation.level == 2);
    assert(runtime_page_table_lookup(
        &table,
        virtual_address + 2 * block_span -
            2 * RUNTIME_PAGE_TABLE_PAGE_SIZE,
        &translation
    ) == RUNTIME_PAGE_TABLE_STATUS_OK);
    assert(translation.level == 2);
}

static void test_block_split_unmap_and_reprotect(void) {
    static RuntimePageTablePage pages[8];
    RuntimePageTablePool pool;
    RuntimePageTable table;
    initialize_table(pages, 8, &pool, &table);

    const uintptr_t virtual_address = UINT64_C(0x40000000);
    const uintptr_t physical_address = UINT64_C(0x80000000);
    assert(runtime_page_table_map(
        &table,
        virtual_address,
        physical_address,
        2 * runtime_page_table_level_span(2),
        normal_memory()
    ) == RUNTIME_PAGE_TABLE_STATUS_OK);
    assert(pool.allocated_count == 2);

    RuntimePageTableTranslation translation;
    assert(runtime_page_table_lookup(
        &table,
        virtual_address + RUNTIME_PAGE_TABLE_PAGE_SIZE,
        &translation
    ) == RUNTIME_PAGE_TABLE_STATUS_OK);
    assert(translation.level == 2);

    invalidate_count = 0;
    assert(runtime_page_table_unmap(
        &table,
        virtual_address + RUNTIME_PAGE_TABLE_PAGE_SIZE,
        RUNTIME_PAGE_TABLE_PAGE_SIZE
    ) == RUNTIME_PAGE_TABLE_STATUS_OK);
    assert(pool.allocated_count == 3);
    assert(invalidate_count >= 2);
    assert(runtime_page_table_lookup(
        &table,
        virtual_address + RUNTIME_PAGE_TABLE_PAGE_SIZE,
        &translation
    ) == RUNTIME_PAGE_TABLE_STATUS_NOT_MAPPED);
    assert(runtime_page_table_lookup(
        &table,
        virtual_address,
        &translation
    ) == RUNTIME_PAGE_TABLE_STATUS_OK);
    assert(translation.level == 3);

    const uintptr_t reprotect_address =
        virtual_address + runtime_page_table_level_span(2) +
        3 * RUNTIME_PAGE_TABLE_PAGE_SIZE;
    assert(runtime_page_table_reprotect(
        &table,
        reprotect_address,
        RUNTIME_PAGE_TABLE_PAGE_SIZE,
        read_only_memory()
    ) == RUNTIME_PAGE_TABLE_STATUS_OK);
    assert(pool.allocated_count == 4);
    assert(runtime_page_table_lookup(
        &table,
        reprotect_address,
        &translation
    ) == RUNTIME_PAGE_TABLE_STATUS_OK);
    assert(translation.attributes.access ==
        RUNTIME_PAGE_TABLE_ACCESS_PRIVILEGED_READ_ONLY);
    assert(runtime_page_table_lookup(
        &table,
        reprotect_address + RUNTIME_PAGE_TABLE_PAGE_SIZE,
        &translation
    ) == RUNTIME_PAGE_TABLE_STATUS_OK);
    assert(translation.attributes.access ==
        RUNTIME_PAGE_TABLE_ACCESS_PRIVILEGED_READ_WRITE);
}

static void test_full_block_unmap_reclaims_child(void) {
    static RuntimePageTablePage pages[4];
    RuntimePageTablePool pool;
    RuntimePageTable table;
    initialize_table(pages, 4, &pool, &table);
    const uintptr_t virtual_address = UINT64_C(0x40000000);

    assert(runtime_page_table_map(
        &table,
        virtual_address,
        UINT64_C(0x80000000),
        runtime_page_table_level_span(2),
        normal_memory()
    ) == RUNTIME_PAGE_TABLE_STATUS_OK);
    assert(pool.allocated_count == 2);
    assert(runtime_page_table_unmap(
        &table,
        virtual_address,
        runtime_page_table_level_span(2)
    ) == RUNTIME_PAGE_TABLE_STATUS_OK);
    assert(pool.allocated_count == 1);
    RuntimePageTableTranslation translation;
    assert(runtime_page_table_lookup(
        &table,
        virtual_address,
        &translation
    ) == RUNTIME_PAGE_TABLE_STATUS_NOT_MAPPED);
}

static void test_top_canonical_page(void) {
    static RuntimePageTablePage pages[4];
    RuntimePageTablePool pool;
    RuntimePageTable table;
    initialize_table(pages, 4, &pool, &table);
    const uintptr_t virtual_address =
        UINTPTR_MAX - RUNTIME_PAGE_TABLE_PAGE_SIZE + 1;
    const uintptr_t physical_address =
        RUNTIME_PAGE_TABLE_PHYSICAL_ADDRESS_MAX &
        ~(uintptr_t)(RUNTIME_PAGE_TABLE_PAGE_SIZE - 1);

    assert(runtime_page_table_map(
        &table,
        virtual_address,
        physical_address,
        RUNTIME_PAGE_TABLE_PAGE_SIZE,
        normal_memory()
    ) == RUNTIME_PAGE_TABLE_STATUS_OK);
    RuntimePageTableTranslation translation;
    assert(runtime_page_table_lookup(
        &table,
        UINTPTR_MAX,
        &translation
    ) == RUNTIME_PAGE_TABLE_STATUS_OK);
    assert(translation.physical_address ==
        RUNTIME_PAGE_TABLE_PHYSICAL_ADDRESS_MAX);
    assert(runtime_page_table_unmap(
        &table,
        virtual_address,
        RUNTIME_PAGE_TABLE_PAGE_SIZE
    ) == RUNTIME_PAGE_TABLE_STATUS_OK);
    assert(pool.allocated_count == 1);
}

int main(void) {
    test_geometry();
    test_descriptors();
    test_page_mapping();
    test_mapping_pool_exhaustion_is_atomic();
    test_walk_pool_exhaustion_is_atomic();
    test_split_pool_exhaustion_is_atomic();
    test_block_split_unmap_and_reprotect();
    test_full_block_unmap_reclaims_child();
    test_top_canonical_page();
    puts("runtime_page_table_tests: ok");
}
