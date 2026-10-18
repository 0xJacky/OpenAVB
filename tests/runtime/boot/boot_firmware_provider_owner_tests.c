#include "avp/runtime/boot/boot_firmware_provider_owner.h"

#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>

static bool allow_registration;
static unsigned initialize_count;
static unsigned registration_count;
static jmp_buf failure_jump;

bool virtual_firmware_load(uint32_t *image_size) {
    *image_size = 0x4000u;
    return true;
}

static bool initialize_root_service(const char *name, void *context) {
    (void)context;
    assert(strcmp(name, "root_firmware") == 0);
    ++initialize_count;
    return true;
}

static bool register_provider(
    const char *name,
    BootFirmwareLoadImage load,
    void *context
) {
    (void)context;
    assert(strcmp(name, "virt_firmware") == 0);
    uint32_t image_size = 0;
    assert(load(&image_size));
    assert(image_size == 0x4000u);
    ++registration_count;
    return allow_registration;
}

static void failure(uint32_t source_line, void *context) {
    (void)context;
    assert(source_line != 0);
    longjmp(failure_jump, 1);
}

int main(void) {
    BootFirmwareProviderTransport transport = {
        .initialize_root_service = initialize_root_service,
        .register_provider = register_provider,
        .failure = failure,
    };
    boot_firmware_provider_bind_transport(transport);

    boot_firmware_provider_reset();
    allow_registration = true;
    assert(boot_initialize_root_firmware_services() == 0);
    assert(boot_firmware_provider_is_registered());
    assert(initialize_count == 1);
    assert(registration_count == 1);
    assert(register_virtual_firmware_provider(true));
    assert(registration_count == 1);

    boot_firmware_provider_reset();
    allow_registration = false;
    assert(!register_virtual_firmware_provider(false));
    if (setjmp(failure_jump) == 0) {
        register_virtual_firmware_provider(true);
        assert(false);
    }

    puts("boot firmware provider owner tests passed");
    return 0;
}

