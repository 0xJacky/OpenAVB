#include "avp/runtime/boot/boot_virtual_firmware_outliners.h"

#include <assert.h>
#include <stdio.h>

uintptr_t __stack_chk_guard = UINT64_C(0x8f0e1d2c3b4a5968);

int main(void) {
    uint8_t provider_bytes[0x20] = {0};
    RuntimeFirebloomType provider_type = {
        .identity = UINT64_C(0x1474d0),
        .element_size = sizeof(provider_bytes),
        .bytewise_copyable = false,
    };
    const RuntimeFirebloomSpan provider = {
        .current = provider_bytes,
        .lower_bound = provider_bytes,
        .upper_bound = provider_bytes + sizeof(provider_bytes),
        .type = &provider_type,
    };
    const BootVirtualFirmwareProviderCapture capture =
        boot_virtual_firmware_capture_provider_tuple_and_stack_guard(provider);
    assert(capture.provider.current == provider.current);
    assert(capture.provider.lower_bound == provider.lower_bound);
    assert(capture.provider.upper_bound == provider.upper_bound);
    assert(capture.provider.type == provider.type);
    assert(capture.stack_guard == __stack_chk_guard);
    assert(runtime_stack_guard_load_to_x8_provider_scan() ==
           __stack_chk_guard);
    assert(runtime_stack_guard_compare_saved_x8_provider_scan(
        __stack_chk_guard));
    assert(!runtime_stack_guard_compare_saved_x8_provider_scan(0));
    assert(boot_virtual_firmware_restore_provider_tuple(provider).current ==
           provider.current);
    puts("boot virtual firmware outliner tests passed");
    return 0;
}
