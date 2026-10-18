#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BOOT_MMU_BOOTSTRAP_PUBLIC __attribute__((used, retain))

enum {
    BOOT_MMU_PAGE_SIZE = 0x4000,
    BOOT_MMU_MAX_MAPPINGS = 256,
    BOOT_MMU_MAIR_EL1_VALUE = 0xff00,
};

typedef struct {
    uintptr_t virtual_address;
    uintptr_t physical_address;
    uint32_t attributes;
    bool valid;
    bool temporary;
} BootMMUMapping;

typedef struct {
    BootMMUMapping mappings[BOOT_MMU_MAX_MAPPINGS];
    size_t mapping_count;
    uint64_t mair_el1;
    uint64_t sctlr_el1;
    uintptr_t translation_base;
    bool active;
} BootMMUBootstrapState;

typedef struct {
    uintptr_t physical_address;
    uintptr_t virtual_address;
    size_t length;
    uint32_t attributes;
    bool temporary;
} BootMMURegion;

BOOT_MMU_BOOTSTRAP_PUBLIC bool boot_mmu_map_16k_range_x20_x21_x22_w23(
    BootMMUBootstrapState *state,
    uintptr_t physical_address,
    uintptr_t virtual_address,
    size_t length,
    uint32_t attributes,
    bool temporary
);
BOOT_MMU_BOOTSTRAP_PUBLIC uint64_t boot_mmu_configure_mair_el1(
    BootMMUBootstrapState *state
);
BOOT_MMU_BOOTSTRAP_PUBLIC uint64_t boot_mmu_build_sctlr_el1_value(void);
BOOT_MMU_BOOTSTRAP_PUBLIC bool
boot_mmu_build_and_activate_relocated_address_space(
    BootMMUBootstrapState *state,
    uintptr_t translation_base,
    const BootMMURegion *regions,
    size_t region_count
);
BOOT_MMU_BOOTSTRAP_PUBLIC size_t
boot_mmu_remove_temporary_work_ram_mappings_x12(
    BootMMUBootstrapState *state,
    uintptr_t work_ram_lower,
    uintptr_t work_ram_upper
);
