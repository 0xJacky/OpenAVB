#include "avp/runtime/boot/boot_platform_hooks.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    const BootPlatformHandoffTrampolineView view =
        platform_boot_handoff_trampoline_bounded_view();
    assert(view.current == (uintptr_t)UINT64_C(0x70048000));
    assert(view.lower_bound == view.current);
    assert(view.upper_bound == (uintptr_t)UINT64_C(0x7004c000));
    assert(view.type_identity == (uintptr_t)UINT64_C(0x00144f18));
    const BootPlatformHandoffTrampolineView veneer =
        j_platform_boot_handoff_trampoline_bounded_view();
    assert(veneer.current == view.current && veneer.upper_bound == view.upper_bound);
    boot_root_firmware_services_start_hook_noop();
    boot_platform_shutdown_phase_hook_noop(2, false);
    boot_platform_payload_handoff_policy_hook_noop();
    assert(boot_image_return_true_thunk());
    assert(platform_mmu_prepare_noop_return_true());
    assert(boot_platform_prepare_payload_handoff_noop() == 0);
    assert(boot_root_firmware_services_prepare_status_zero() == 0);
    uint64_t context = UINT64_C(0xa5a5a5a5);
    boot_platform_payload_handoff_context_bind(&context);
    assert(boot_platform_payload_handoff_context() == &context);
    puts("boot platform hook tests passed");
    return 0;
}
