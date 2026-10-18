#include "avp/runtime/boot/boot_virtual_firmware_outliners.h"

extern uintptr_t __stack_chk_guard;

bool runtime_stack_guard_compare_saved_x8_provider_scan(
    uint64_t saved_guard
) {
    return saved_guard == __stack_chk_guard;
}

BootVirtualFirmwareProviderCapture
boot_virtual_firmware_capture_provider_tuple_and_stack_guard(
    RuntimeFirebloomSpan provider
) {
    return (BootVirtualFirmwareProviderCapture){
        .provider = provider,
        .stack_guard = __stack_chk_guard,
    };
}

uint64_t runtime_stack_guard_load_to_x8_provider_scan(void) {
    return __stack_chk_guard;
}

RuntimeFirebloomSpan boot_virtual_firmware_restore_provider_tuple(
    RuntimeFirebloomSpan provider
) {
    return provider;
}
