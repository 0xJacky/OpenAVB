#include "avp/runtime/boot/boot_mmu_bootstrap_owner.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    BootMMUBootstrapState state = {0};
    const BootMMURegion regions[] = {
        {.physical_address = 0x100000,
         .virtual_address = 0x100000,
         .length = 0x8000,
         .attributes = 1,
         .temporary = false},
        {.physical_address = 0x70000000,
         .virtual_address = 0x70000000,
         .length = 0xc000,
         .attributes = 2,
         .temporary = true},
    };
    assert(boot_mmu_build_and_activate_relocated_address_space(
        &state, 0x70028000, regions, 2));
    assert(state.active);
    assert(state.mapping_count == 5);
    assert(state.mair_el1 == UINT64_C(0xff00));
    assert(state.sctlr_el1 == UINT64_C(0x0488520d));
    assert(state.mappings[1].physical_address == 0x104000);
    assert(state.mappings[4].virtual_address == 0x70008000);

    assert(!boot_mmu_map_16k_range_x20_x21_x22_w23(
        &state, 0x200000, 0x100000, 0x4000, 1, false));
    assert(!boot_mmu_map_16k_range_x20_x21_x22_w23(
        &state, 0x200001, 0x200000, 0x4000, 1, false));

    assert(boot_mmu_remove_temporary_work_ram_mappings_x12(
               &state, 0x70000000, 0x70028000) == 3);
    assert(!state.mappings[2].valid);
    assert(state.mappings[0].valid);

    puts("boot MMU bootstrap owner tests passed");
    return 0;
}
