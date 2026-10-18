#include "avp/runtime/memory/runtime_mmu.h"

#include "avp/runtime/exception/pvpanic.h"


enum {
    RUNTIME_MMU_VIRTUAL_ADDRESS_BITS = 42,
    RUNTIME_MMU_ROM_BASE = 0x00100000,
    RUNTIME_MMU_ROM_LIMIT = 0x00180000,
    RUNTIME_MMU_DEVICE_UPPER_BOUND = 0x40000000,
};

#define RUNTIME_MMU_RAM_BASE UINT64_C(0x70000000)
#define RUNTIME_MMU_RAM_SIZE UINT64_C(0x200000000)
#define RUNTIME_MMU_MAIR_EL1 UINT64_C(0x44400ff04)
#define RUNTIME_MMU_TCR_EL1_IPS_36 UINT64_C(0x165d6a516)
#define RUNTIME_MMU_TCR_EL1_IPS_42 UINT64_C(0x365d6a516)
#define RUNTIME_MMU_SCTLR_EL1 UINT64_C(0x0488520d)

static RuntimePageTablePage runtime_mmu_pages[
    RUNTIME_PAGE_TABLE_POOL_MAX_PAGES
] __attribute__((section(".runtime_page_tables")));
static RuntimePageTablePool runtime_mmu_pool;
static RuntimePageTable runtime_mmu_table;
static bool runtime_mmu_initialized;

_Noreturn static void runtime_mmu_panic(void) {
    pvpanic_trigger();
}

static RuntimePageTableAttributes runtime_mmu_device_attributes(void) {
    return (RuntimePageTableAttributes){
        .memory_attribute_index = 0,
        /* EL0 provider tasks own the virtual platform and SimpleVirtio MMIO. */
        .access = RUNTIME_PAGE_TABLE_ACCESS_USER_READ_WRITE,
        .shareability = RUNTIME_PAGE_TABLE_SHAREABILITY_INNER,
        .access_flag = true,
        .privileged_execute_never = true,
        .unprivileged_execute_never = true,
    };
}

static RuntimePageTableAttributes runtime_mmu_code_attributes(void) {
    return (RuntimePageTableAttributes){
        .memory_attribute_index = 1,
        .access = RUNTIME_PAGE_TABLE_ACCESS_USER_READ_ONLY,
        .shareability = RUNTIME_PAGE_TABLE_SHAREABILITY_INNER,
        .access_flag = true,
    };
}

static RuntimePageTableAttributes runtime_mmu_ram_attributes(void) {
    return (RuntimePageTableAttributes){
        .memory_attribute_index = 1,
        .access = RUNTIME_PAGE_TABLE_ACCESS_USER_READ_WRITE,
        .shareability = RUNTIME_PAGE_TABLE_SHAREABILITY_INNER,
        .access_flag = true,
    };
}

static void runtime_mmu_map_identity(
    uintptr_t lower_bound,
    size_t size,
    RuntimePageTableAttributes attributes
) {
    if (runtime_page_table_map(
            &runtime_mmu_table,
            lower_bound,
            lower_bound,
            size,
            attributes
        ) != RUNTIME_PAGE_TABLE_STATUS_OK) {
        runtime_mmu_panic();
    }
}

static uint64_t runtime_mmu_tcr_el1(void) {
    const uint8_t physical_range = (uint8_t)(
        runtime_page_table_arch_read_id_aa64mmfr0_el1() & 0xf
    );
    return physical_range <= 1 ?
        RUNTIME_MMU_TCR_EL1_IPS_36 : RUNTIME_MMU_TCR_EL1_IPS_42;
}

__attribute__((no_stack_protector))
void runtime_mmu_initialize(void) {
    if (runtime_mmu_initialized ||
        (runtime_page_table_arch_read_sctlr_el1() & UINT64_C(1)) != 0) {
        runtime_mmu_panic();
    }
    if (runtime_page_table_pool_initialize(
            &runtime_mmu_pool,
            runtime_mmu_pages,
            (uintptr_t)runtime_mmu_pages,
            RUNTIME_PAGE_TABLE_POOL_MAX_PAGES
        ) != RUNTIME_PAGE_TABLE_STATUS_OK ||
        runtime_page_table_initialize(
            &runtime_mmu_table,
            &runtime_mmu_pool,
            RUNTIME_MMU_VIRTUAL_ADDRESS_BITS,
            0
        ) != RUNTIME_PAGE_TABLE_STATUS_OK) {
        runtime_mmu_panic();
    }

    runtime_mmu_map_identity(
        0,
        RUNTIME_MMU_ROM_BASE,
        runtime_mmu_device_attributes()
    );
    runtime_mmu_map_identity(
        RUNTIME_MMU_ROM_BASE,
        RUNTIME_MMU_ROM_LIMIT - RUNTIME_MMU_ROM_BASE,
        runtime_mmu_code_attributes()
    );
    runtime_mmu_map_identity(
        RUNTIME_MMU_ROM_LIMIT,
        RUNTIME_MMU_DEVICE_UPPER_BOUND - RUNTIME_MMU_ROM_LIMIT,
        runtime_mmu_device_attributes()
    );
    runtime_mmu_map_identity(
        RUNTIME_MMU_RAM_BASE,
        RUNTIME_MMU_RAM_SIZE,
        runtime_mmu_ram_attributes()
    );

    const uint64_t tcr_el1 = runtime_mmu_tcr_el1();
    runtime_page_table_arch_write_mair_el1(RUNTIME_MMU_MAIR_EL1);
    runtime_page_table_arch_write_tcr_el1(tcr_el1);
    runtime_page_table_arch_write_ttbr0_el1(
        runtime_mmu_table.root_physical_address
    );
    runtime_page_table_arch_write_ttbr1_el1(0);
    runtime_page_table_arch_invalidate_all();
    runtime_page_table_arch_write_sctlr_el1(RUNTIME_MMU_SCTLR_EL1);

    const uint64_t enabled_bits = UINT64_C(1) | (UINT64_C(1) << 2) |
        (UINT64_C(1) << 12) | (UINT64_C(1) << 19);
    if ((runtime_page_table_arch_read_sctlr_el1() & enabled_bits) !=
            enabled_bits ||
        runtime_page_table_arch_read_tcr_el1() != tcr_el1 ||
        runtime_page_table_arch_read_mair_el1() != RUNTIME_MMU_MAIR_EL1 ||
        (runtime_page_table_arch_read_ttbr0_el1() &
            RUNTIME_PAGE_TABLE_PHYSICAL_ADDRESS_MAX) !=
            runtime_mmu_table.root_physical_address ||
        runtime_page_table_arch_read_ttbr1_el1() != 0) {
        runtime_mmu_panic();
    }
    runtime_mmu_initialized = true;
}

RuntimePageTable *runtime_mmu_page_table(void) {
    if (!runtime_mmu_initialized) {
        runtime_mmu_panic();
    }
    return &runtime_mmu_table;
}
