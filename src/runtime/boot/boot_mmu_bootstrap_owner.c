#include "avp/runtime/boot/boot_mmu_bootstrap_owner.h"

static bool page_aligned(uintptr_t value) {
    return (value & (BOOT_MMU_PAGE_SIZE - 1u)) == 0;
}

static BootMMUMapping *find_mapping(
    BootMMUBootstrapState *state,
    uintptr_t virtual_address
) {
    for (size_t index = 0; index < state->mapping_count; ++index) {
        if (state->mappings[index].valid &&
            state->mappings[index].virtual_address == virtual_address) {
            return &state->mappings[index];
        }
    }
    return NULL;
}

bool boot_mmu_map_16k_range_x20_x21_x22_w23(
    BootMMUBootstrapState *state,
    uintptr_t physical_address,
    uintptr_t virtual_address,
    size_t length,
    uint32_t attributes,
    bool temporary
) {
    if (state == NULL || length == 0 ||
        !page_aligned(physical_address) || !page_aligned(virtual_address) ||
        !page_aligned(length) ||
        length / BOOT_MMU_PAGE_SIZE >
            BOOT_MMU_MAX_MAPPINGS - state->mapping_count) {
        return false;
    }
    for (size_t offset = 0; offset < length; offset += BOOT_MMU_PAGE_SIZE) {
        if (find_mapping(state, virtual_address + offset) != NULL) {
            return false;
        }
    }
    for (size_t offset = 0; offset < length; offset += BOOT_MMU_PAGE_SIZE) {
        state->mappings[state->mapping_count++] = (BootMMUMapping){
            .virtual_address = virtual_address + offset,
            .physical_address = physical_address + offset,
            .attributes = attributes,
            .valid = true,
            .temporary = temporary,
        };
    }
    return true;
}

uint64_t boot_mmu_configure_mair_el1(BootMMUBootstrapState *state) {
    if (state != NULL) {
        state->mair_el1 = BOOT_MMU_MAIR_EL1_VALUE;
    }
    return BOOT_MMU_MAIR_EL1_VALUE;
}

uint64_t boot_mmu_build_sctlr_el1_value(void) {
    return UINT64_C(0x0488520d);
}

bool boot_mmu_build_and_activate_relocated_address_space(
    BootMMUBootstrapState *state,
    uintptr_t translation_base,
    const BootMMURegion *regions,
    size_t region_count
) {
    if (state == NULL || !page_aligned(translation_base) ||
        (region_count != 0 && regions == NULL)) {
        return false;
    }
    state->mapping_count = 0;
    state->active = false;
    for (size_t index = 0; index < region_count; ++index) {
        const BootMMURegion *region = &regions[index];
        if (!boot_mmu_map_16k_range_x20_x21_x22_w23(
                state, region->physical_address, region->virtual_address,
                region->length, region->attributes, region->temporary)) {
            state->mapping_count = 0;
            return false;
        }
    }
    state->translation_base = translation_base;
    state->mair_el1 = boot_mmu_configure_mair_el1(state);
    state->sctlr_el1 = boot_mmu_build_sctlr_el1_value();
    state->active = true;
    return true;
}

size_t boot_mmu_remove_temporary_work_ram_mappings_x12(
    BootMMUBootstrapState *state,
    uintptr_t work_ram_lower,
    uintptr_t work_ram_upper
) {
    if (state == NULL || work_ram_lower >= work_ram_upper) {
        return 0;
    }
    size_t removed = 0;
    for (size_t index = 0; index < state->mapping_count; ++index) {
        BootMMUMapping *mapping = &state->mappings[index];
        if (mapping->valid && mapping->temporary &&
            mapping->physical_address >= work_ram_lower &&
            mapping->physical_address < work_ram_upper) {
            mapping->valid = false;
            ++removed;
        }
    }
    return removed;
}
