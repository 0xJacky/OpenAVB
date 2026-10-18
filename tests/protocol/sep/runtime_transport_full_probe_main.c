#include "avp/platform/platform.h"
#include "avp/runtime/exception/pvpanic.h"
#include "avp/protocol/sep/sep.h"


enum {
    RUNTIME_TRANSPORT_FULL_PROBE_STAGE = 0x5254464c,
    MAILBOX_RECEIVE_CAPACITY = 8,
};

void firmware_main(void) {
    pvpanic_initialize();
    platform_stage(RUNTIME_TRANSPORT_FULL_PROBE_STAGE);
    sep_bootstrap();

    const SEPMailboxMessage filler = {
        .low = 0x00000000001100ff,
    };
    for (uint32_t index = 0; index < MAILBOX_RECEIVE_CAPACITY; ++index) {
        sep_send(filler);
    }
    platform_progress(0, MAILBOX_RECEIVE_CAPACITY);

    uint32_t word;
    if (sep_read_research_pattern_word(&word)) {
        platform_progress(1, word);
    }
    pvpanic_trigger();
}
