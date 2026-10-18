#include "avp/runtime/boot/boot_platform_hooks.h"

#if defined(__ELF__)
static void *payload_handoff_context
    __attribute__((section(".runtime_auxiliary.state"), used));
#else
static void *payload_handoff_context;
#endif

BootPlatformHandoffTrampolineView
platform_boot_handoff_trampoline_bounded_view(void) {
    return (BootPlatformHandoffTrampolineView){
        .current = (uintptr_t)UINT64_C(0x70048000),
        .lower_bound = (uintptr_t)UINT64_C(0x70048000),
        .upper_bound = (uintptr_t)UINT64_C(0x7004c000),
        .type_identity = (uintptr_t)UINT64_C(0x00144f18),
    };
}

BootPlatformHandoffTrampolineView
j_platform_boot_handoff_trampoline_bounded_view(void) {
    return platform_boot_handoff_trampoline_bounded_view();
}

void boot_root_firmware_services_start_hook_noop(void) {}

void boot_platform_shutdown_phase_hook_noop(uint32_t phase, bool active) {
    (void)phase;
    (void)active;
}

bool boot_image_return_true_thunk(void) {
    return true;
}

bool platform_mmu_prepare_noop_return_true(void) {
    return true;
}

int boot_platform_prepare_payload_handoff_noop(void) {
    return 0;
}

void boot_platform_payload_handoff_policy_hook_noop(void) {}

int boot_root_firmware_services_prepare_status_zero(void) {
    return 0;
}

void boot_platform_payload_handoff_context_bind(void *context) {
    payload_handoff_context = context;
}

void *boot_platform_payload_handoff_context(void) {
    return payload_handoff_context;
}
