#pragma once

#include <stdbool.h>
#include <stdint.h>


enum {
    SOURCE_IMAGE_BASE = 0x7006c000,
    VIRTUAL_FIRMWARE_IMAGE_BASE = 0x70074000,
    VIRTUAL_FIRMWARE_RESIDENT_LIMIT = 0x70140000,
    VIRTUAL_FIRMWARE_SRAM_IMAGE_BASE = 0x1fc050000,
    VIRTUAL_FIRMWARE_SRAM_IMAGE_CAPACITY = 0x00400000,
    SOURCE_IMAGE_UPLOAD_BASE = 0x71000000,
    /*
     * Two megabytes, not one.  18.6.2's iBSS im4p is 1,054,399 bytes and
     * 21A329's was 1,004,593: the old window fell between the two, so the
     * newer image died on the last chunk of its own upload -- 513 of 515,
     * a USBError, and nothing anywhere to say the window was the reason.
     * The patch store, which started where this window ended, moves up.
     */
    SOURCE_IMAGE_CAPACITY = 0x00200000,
    SOURCE_IMAGE_MAGIC = 0x53424953,
    SOURCE_IMAGE_VERSION = 1,
    SOURCE_IMAGE_BOOT_MODE_NORMAL = 0,
    SOURCE_IMAGE_BOOT_MODE_DFU = 2,
};

#define SOURCE_IMAGE_IDENTITY UINT64_C(0x5352434942535331)

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t version;
    uint16_t header_size;
    uint32_t image_size;
    uint32_t entry_offset;
    uint64_t identity;
    uint32_t content_crc32;
    uint32_t reserved;
} SourceImageHeader;

_Static_assert(sizeof(SourceImageHeader) == 32, "SourceImageHeader size");

bool source_image_validate(uint32_t downloaded_size);
bool source_image_measure(
    const uint8_t *prefix,
    uint32_t prefix_size,
    uint32_t *image_size
);
bool source_image_prepare_virtual_firmware(uint32_t image_size);
uint32_t source_image_size(void);
uintptr_t source_image_base(void);
uintptr_t source_image_entry(void);
void source_image_handoff(
    uintptr_t image_base,
    uint32_t image_size,
    uintptr_t entry,
    uint32_t boot_mode
) __attribute__((noreturn));
