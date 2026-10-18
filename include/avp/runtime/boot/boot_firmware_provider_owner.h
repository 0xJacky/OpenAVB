#pragma once

#include <stdbool.h>
#include <stdint.h>

#if __has_attribute(retain)
#define BOOT_FIRMWARE_PROVIDER_PUBLIC __attribute__((used, retain))
#else
#define BOOT_FIRMWARE_PROVIDER_PUBLIC __attribute__((used))
#endif

typedef bool (*BootFirmwareLoadImage)(uint32_t *image_size);

typedef struct {
    bool (*initialize_root_service)(const char *name, void *context);
    bool (*register_provider)(
        const char *name,
        BootFirmwareLoadImage load,
        void *context
    );
    void (*failure)(uint32_t source_line, void *context);
    void *context;
} BootFirmwareProviderTransport;

void boot_firmware_provider_bind_transport(BootFirmwareProviderTransport transport);
void boot_firmware_provider_reset(void);
bool boot_firmware_provider_is_registered(void);

BOOT_FIRMWARE_PROVIDER_PUBLIC bool
register_virtual_firmware_provider(bool required);
BOOT_FIRMWARE_PROVIDER_PUBLIC int boot_initialize_root_firmware_services(void);

