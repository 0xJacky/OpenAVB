#pragma once

#include <stdbool.h>
#include <stdint.h>

#define BOOT_PROGRESS_PUBLIC __attribute__((used, retain))

enum {
    BOOT_PROGRESS_SOURCE_ATTEMPT = 0x00020005,
    BOOT_PROGRESS_SOURCE_ATTEMPT_COMPLETE = 0x00020006,
    BOOT_PROGRESS_DFU_ACTIVE = 0x00020002,
};

typedef struct {
    uint32_t (*read_word)(void *context, uint32_t index);
    void (*write_word)(void *context, uint32_t index, uint32_t value);
    void *context;
} BootProgressTransport;

void boot_progress_bind_transport(BootProgressTransport transport);
void boot_progress_reset_cache(void);

BOOT_PROGRESS_PUBLIC void platform_progress_publish_code(uint32_t code);
BOOT_PROGRESS_PUBLIC void platform_progress_publish_words(
    uint32_t code,
    uint32_t detail,
    uint32_t auxiliary0,
    uint32_t auxiliary1
);
BOOT_PROGRESS_PUBLIC void platform_progress_clear(void);
BOOT_PROGRESS_PUBLIC uint32_t platform_progress_read_code(void);
BOOT_PROGRESS_PUBLIC void boot_progress_publish_source_attempt_state(
    uint32_t source_type,
    uint32_t state,
    uint32_t auxiliary
);
BOOT_PROGRESS_PUBLIC void boot_progress_publish_dfu_active_state(bool active);
