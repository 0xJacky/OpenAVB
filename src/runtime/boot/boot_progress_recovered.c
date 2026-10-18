#include "avp/runtime/boot/boot_progress_recovered.h"

#include "avp/platform/mmio.h"
#include "avp/platform/platform.h"


enum {
    BOOT_PROGRESS_MMIO_OFFSET = 0x64,
    BOOT_PROGRESS_WORD_COUNT = 4,
};

static uint32_t cached_code;
static uint32_t cached_detail;
static BootProgressTransport bound_transport;

static uint32_t boot_progress_mmio_read(void *context, uint32_t index) {
    (void)context;
    return mmio_read32(
        AVP_PLATFORM_BASE + BOOT_PROGRESS_MMIO_OFFSET + index * 4
    );
}

static void boot_progress_mmio_write(
    void *context,
    uint32_t index,
    uint32_t value
) {
    (void)context;
    mmio_write32(
        AVP_PLATFORM_BASE + BOOT_PROGRESS_MMIO_OFFSET + index * 4,
        value
    );
}

static BootProgressTransport boot_progress_transport(void) {
    if (bound_transport.read_word != 0) {
        return bound_transport;
    }
    return (BootProgressTransport){
        .read_word = boot_progress_mmio_read,
        .write_word = boot_progress_mmio_write,
    };
}

void boot_progress_bind_transport(BootProgressTransport transport) {
    if (transport.read_word == 0 || transport.write_word == 0) {
        __builtin_trap();
    }
    bound_transport = transport;
}

void boot_progress_reset_cache(void) {
    cached_code = 0;
    cached_detail = 0;
}

void platform_progress_publish_words(
    uint32_t code,
    uint32_t detail,
    uint32_t auxiliary0,
    uint32_t auxiliary1
) {
    if (code == cached_code && detail == cached_detail) {
        return;
    }
    const BootProgressTransport transport = boot_progress_transport();
    const uint32_t words[BOOT_PROGRESS_WORD_COUNT] = {
        code,
        detail,
        auxiliary0,
        auxiliary1,
    };
    for (uint32_t index = 0; index < BOOT_PROGRESS_WORD_COUNT; ++index) {
        transport.write_word(transport.context, index, words[index]);
    }
    cached_code = code;
    cached_detail = detail;
}

void platform_progress_publish_code(uint32_t code) {
    platform_progress_publish_words(code, 0, 0, 0);
}

void platform_progress_clear(void) {
    platform_progress_publish_code(0);
}

uint32_t platform_progress_read_code(void) {
    const BootProgressTransport transport = boot_progress_transport();
    return transport.read_word(transport.context, 0);
}

void boot_progress_publish_source_attempt_state(
    uint32_t source_type,
    uint32_t state,
    uint32_t auxiliary
) {
    platform_progress_publish_words(
        BOOT_PROGRESS_SOURCE_ATTEMPT,
        state,
        source_type,
        auxiliary
    );
    platform_progress_publish_code(BOOT_PROGRESS_SOURCE_ATTEMPT_COMPLETE);
}

void boot_progress_publish_dfu_active_state(bool active) {
    platform_progress_publish_words(BOOT_PROGRESS_DFU_ACTIVE, active, 0, 0);
}
