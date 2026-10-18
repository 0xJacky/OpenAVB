#pragma once

#include "avp/runtime/firebloom/runtime_firebloom.h"

#include <stdbool.h>
#include <stdint.h>

#define BOOT_VIRTUAL_FIRMWARE_OUTLINER_PUBLIC __attribute__((used, retain))

typedef struct {
    RuntimeFirebloomSpan provider;
    uint64_t stack_guard;
} BootVirtualFirmwareProviderCapture;

BOOT_VIRTUAL_FIRMWARE_OUTLINER_PUBLIC bool
runtime_stack_guard_compare_saved_x8_provider_scan(uint64_t saved_guard);
BOOT_VIRTUAL_FIRMWARE_OUTLINER_PUBLIC BootVirtualFirmwareProviderCapture
boot_virtual_firmware_capture_provider_tuple_and_stack_guard(
    RuntimeFirebloomSpan provider
);
BOOT_VIRTUAL_FIRMWARE_OUTLINER_PUBLIC uint64_t
runtime_stack_guard_load_to_x8_provider_scan(void);
BOOT_VIRTUAL_FIRMWARE_OUTLINER_PUBLIC RuntimeFirebloomSpan
boot_virtual_firmware_restore_provider_tuple(RuntimeFirebloomSpan provider);
