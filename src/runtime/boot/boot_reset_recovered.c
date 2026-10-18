#include "avp/runtime/boot/boot_reset_recovered.h"

#include "avp/runtime/exception/pvpanic.h"


static PlatformPSCIInvoke bound_invoke;
static void *bound_context;

#if defined(__aarch64__) && !defined(__APPLE__)
static int64_t platform_psci_invoke_arch(
    uint32_t function_id,
    uint64_t argument0,
    uint64_t argument1,
    uint64_t argument2,
    void *context
) {
    (void)context;
    register uint64_t x0 __asm__("x0") = function_id;
    register uint64_t x1 __asm__("x1") = argument0;
    register uint64_t x2 __asm__("x2") = argument1;
    register uint64_t x3 __asm__("x3") = argument2;
    __asm__ volatile(
        "svc #0xa0"
        : "+r"(x0)
        : "r"(x1), "r"(x2), "r"(x3)
        : "memory"
    );
    return (int64_t)x0;
}
#endif

static int64_t platform_psci_invoke(uint32_t function_id) {
    if (bound_invoke != 0) {
        return bound_invoke(function_id, 0, 0, 0, bound_context);
    }
#if defined(__aarch64__) && !defined(__APPLE__)
    return platform_psci_invoke_arch(function_id, 0, 0, 0, 0);
#else
    __builtin_trap();
#endif
}

void platform_psci_bind_invoke(PlatformPSCIInvoke invoke, void *context) {
    if (invoke == 0) {
        __builtin_trap();
    }
    bound_invoke = invoke;
    bound_context = context;
}

bool platform_psci_version_is_supported(uint32_t version) {
    return version == PSCI_VERSION_0_2 ||
        version == PSCI_VERSION_1_0 ||
        version == PSCI_VERSION_1_1;
}

void avp_reset_early_platform_hook_noop(
    uintptr_t image_start,
    uintptr_t image_end
) {
    (void)image_start;
    (void)image_end;
}

void platform_psci_require_supported_version(void) {
    const uint32_t version = (uint32_t)platform_psci_invoke(
        PSCI_VERSION_FUNCTION_ID
    );
    if (!platform_psci_version_is_supported(version)) {
        platform_psci_unsupported_version_fatal();
    }
}

_Noreturn void platform_psci_unsupported_version_fatal(void) {
    pvpanic_trigger();
}

_Noreturn void platform_psci_system_reset(void) {
    platform_psci_require_supported_version();
    (void)platform_psci_invoke(PSCI_SYSTEM_RESET_FUNCTION_ID);
    pvpanic_trigger();
}

_Noreturn void platform_system_reset(uint32_t reason) {
    (void)reason;
    platform_psci_system_reset();
}
