#include "avp/runtime/boot/boot_firmware_provider_owner.h"

#include "avp/image/boot/virtual_firmware.h"

static BootFirmwareProviderTransport provider_transport;
static bool provider_registered;

static _Noreturn void provider_fail(uint32_t source_line) {
    if (provider_transport.failure != 0) {
        provider_transport.failure(source_line, provider_transport.context);
    }
    __builtin_trap();
}

void boot_firmware_provider_bind_transport(
    BootFirmwareProviderTransport transport
) {
    provider_transport = transport;
}

void boot_firmware_provider_reset(void) {
    provider_registered = false;
}

bool boot_firmware_provider_is_registered(void) {
    return provider_registered;
}

bool register_virtual_firmware_provider(bool required) {
    if (provider_registered) {
        return true;
    }
    const bool registered = provider_transport.register_provider != 0 &&
        provider_transport.register_provider(
            "virt_firmware",
            virtual_firmware_load,
            provider_transport.context
        );
    if (!registered && required) {
        provider_fail(0x00cc9u);
    }
    provider_registered = registered;
    return registered;
}

int boot_initialize_root_firmware_services(void) {
    if (provider_transport.initialize_root_service == 0 ||
        !provider_transport.initialize_root_service(
            "root_firmware",
            provider_transport.context
        )) {
        provider_fail(0x00cd1u);
    }
    (void)register_virtual_firmware_provider(false);
    return 0;
}

