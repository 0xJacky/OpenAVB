#include "avp/platform/mmio.h"
#include "avp/runtime/exception/pvpanic.h"


enum {
    PVPANIC_BASE = 0x20070000,
    PVPANIC_PANICKED = 1,
    PVPANIC_CRASH_LOADED = 2,
};

static uint8_t capabilities;
static uintptr_t device_address = PVPANIC_BASE;

uint8_t pvpanic_select_panic_event(uint8_t active_capabilities) {
    if ((active_capabilities & PVPANIC_CRASH_LOADED) != 0) {
        return PVPANIC_CRASH_LOADED;
    }
    if ((active_capabilities & PVPANIC_PANICKED) != 0) {
        return PVPANIC_PANICKED;
    }
    return 0;
}

int pvpanic_probe_and_cache(void) {
    device_address = PVPANIC_BASE;
    capabilities = mmio_read8(device_address);
    return 0;
}

uint8_t pvpanic_initialize(void) {
    (void)pvpanic_probe_and_cache();
    return capabilities;
}

void pvpanic_raise_panic_event(void) {
    uint8_t event = pvpanic_select_panic_event(capabilities);
    if (event != 0) {
        mmio_write8(device_address, event);
    }
}

_Noreturn void pvpanic_trigger(void) {
    if (capabilities == 0) {
        (void)pvpanic_probe_and_cache();
    }
    pvpanic_raise_panic_event();
    for (;;) {
        __asm__ volatile("wfe");
    }
}
