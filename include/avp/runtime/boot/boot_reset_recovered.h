#pragma once

#include <stdbool.h>
#include <stdint.h>

#define BOOT_RESET_PUBLIC __attribute__((used, retain))

enum {
    PSCI_VERSION_FUNCTION_ID = 0x84000000,
    PSCI_SYSTEM_RESET_FUNCTION_ID = 0x84000009,
    PSCI_VERSION_0_2 = 0x00000002,
    PSCI_VERSION_1_0 = 0x00010000,
    PSCI_VERSION_1_1 = 0x00010001,
};

typedef int64_t (*PlatformPSCIInvoke)(
    uint32_t function_id,
    uint64_t argument0,
    uint64_t argument1,
    uint64_t argument2,
    void *context
);

void platform_psci_bind_invoke(PlatformPSCIInvoke invoke, void *context);
bool platform_psci_version_is_supported(uint32_t version);

BOOT_RESET_PUBLIC void avp_reset_entry(void);
BOOT_RESET_PUBLIC void avp_reset_early_platform_hook_noop(
    uintptr_t image_start,
    uintptr_t image_end
);
BOOT_RESET_PUBLIC _Noreturn void platform_system_reset(uint32_t reason);
BOOT_RESET_PUBLIC _Noreturn void platform_psci_system_reset(void);
BOOT_RESET_PUBLIC void platform_psci_require_supported_version(void);
BOOT_RESET_PUBLIC _Noreturn void platform_psci_unsupported_version_fatal(void);
