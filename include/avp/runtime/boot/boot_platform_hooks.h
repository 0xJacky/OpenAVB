#pragma once

#include <stdbool.h>
#include <stdint.h>

#define BOOT_PLATFORM_HOOK_PUBLIC __attribute__((used, retain))

typedef struct {
    uintptr_t current;
    uintptr_t lower_bound;
    uintptr_t upper_bound;
    uintptr_t type_identity;
} BootPlatformHandoffTrampolineView;

BOOT_PLATFORM_HOOK_PUBLIC BootPlatformHandoffTrampolineView
platform_boot_handoff_trampoline_bounded_view(void);
BOOT_PLATFORM_HOOK_PUBLIC BootPlatformHandoffTrampolineView
j_platform_boot_handoff_trampoline_bounded_view(void);
BOOT_PLATFORM_HOOK_PUBLIC void boot_root_firmware_services_start_hook_noop(void);
BOOT_PLATFORM_HOOK_PUBLIC void boot_platform_shutdown_phase_hook_noop(
    uint32_t phase,
    bool active
);
BOOT_PLATFORM_HOOK_PUBLIC bool boot_image_return_true_thunk(void);
BOOT_PLATFORM_HOOK_PUBLIC bool platform_mmu_prepare_noop_return_true(void);
BOOT_PLATFORM_HOOK_PUBLIC int boot_platform_prepare_payload_handoff_noop(void);
BOOT_PLATFORM_HOOK_PUBLIC void
boot_platform_payload_handoff_policy_hook_noop(void);
BOOT_PLATFORM_HOOK_PUBLIC int
boot_root_firmware_services_prepare_status_zero(void);
void boot_platform_payload_handoff_context_bind(void *context);
BOOT_PLATFORM_HOOK_PUBLIC void *boot_platform_payload_handoff_context(void);
