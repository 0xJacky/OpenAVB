#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    AVP_HUFA_MAGIC = UINT32_C(0x41465548),
    AVP_HUFA_VERSION = 1,
    AVP_HUFA_HEADER_SIZE = 64,
    AVP_HUFA_SIGNED_SIZE = 32,
    AVP_HUFA_DIGEST_PREFIX_SIZE = 32,
    AVP_HUFA_SCAN_SIZE = UINT32_C(0x20000),
    AVP_HUFA_SCAN_STRIDE = UINT32_C(0x1000),
    AVP_HUFA_MAX_CANDIDATES = 2,
    AVP_HUFA_PROGRESS_SELECTED = UINT32_C(0x00130008),
    AVP_HUFA_PROGRESS_NO_HEADER = UINT32_C(0x40130005),
};

typedef struct {
    uint8_t bytes[AVP_HUFA_HEADER_SIZE];
} AVPHUFAHeader;

typedef struct {
    AVPHUFAHeader candidates[AVP_HUFA_MAX_CANDIDATES];
    uint64_t storage_offsets[AVP_HUFA_MAX_CANDIDATES];
    size_t count;
} AVPHUFAScanResult;

typedef size_t (*BootVirtualFirmwareRead)(
    void *context,
    uint32_t offset,
    void *output,
    size_t size
);

typedef void (*BootVirtualFirmwareDiagnostic)(
    void *context,
    uint64_t code,
    uint32_t detail,
    size_t candidate_count
);

typedef struct {
    void *context;
    BootVirtualFirmwareRead read;
    BootVirtualFirmwareDiagnostic diagnostic;
} BootVirtualFirmwarePlatform;

typedef struct {
    uint32_t code;
    uint32_t source_offset;
} BootVirtualFirmwareProgress;

#define BOOT_VIRTUAL_FIRMWARE_PUBLIC __attribute__((used, retain))

BOOT_VIRTUAL_FIRMWARE_PUBLIC
    size_t boot_virtual_firmware_scan_redundant_headers(
        const BootVirtualFirmwarePlatform *platform,
        AVPHUFAScanResult *result
    );
BOOT_VIRTUAL_FIRMWARE_PUBLIC uint32_t
boot_virtual_firmware_select_latest_source_offset(
    const BootVirtualFirmwarePlatform *platform,
    AVPHUFAScanResult *result
);
BOOT_VIRTUAL_FIRMWARE_PUBLIC void boot_virtual_firmware_log_generated_location(
    const BootVirtualFirmwarePlatform *platform,
    uint32_t line
);
BOOT_VIRTUAL_FIRMWARE_PUBLIC BootVirtualFirmwareProgress
boot_virtual_firmware_prepare_selection_progress(uint32_t source_offset);
BOOT_VIRTUAL_FIRMWARE_PUBLIC size_t boot_virtual_firmware_prepare_provider_read(
    const BootVirtualFirmwarePlatform *platform,
    uint32_t offset,
    void *output,
    size_t size
);
BOOT_VIRTUAL_FIRMWARE_PUBLIC bool
boot_virtual_firmware_compare_header_magic(const AVPHUFAHeader *header);
BOOT_VIRTUAL_FIRMWARE_PUBLIC uint32_t
boot_virtual_firmware_no_header_progress_code(void);
BOOT_VIRTUAL_FIRMWARE_PUBLIC size_t
boot_virtual_firmware_dispatch_provider_read(
    const BootVirtualFirmwarePlatform *platform,
    uint32_t offset,
    void *output,
    size_t size
);
BOOT_VIRTUAL_FIRMWARE_PUBLIC void
boot_virtual_firmware_clear_header_candidates(AVPHUFAScanResult *result);
