#include "avp/platform/platform.h"
#include "avp/runtime/exception/pvpanic.h"
#include "avp/protocol/sep/sep.h"


enum {
    RUNTIME_TRANSPORT_TIMEOUT_PROBE_STAGE = 0x5254544f,
};

void runtime_transport_test_clock_arm(void);

void firmware_main(void) {
    pvpanic_initialize();
    platform_stage(RUNTIME_TRANSPORT_TIMEOUT_PROBE_STAGE);
    sep_bootstrap();
    platform_progress(0, 1);
    runtime_transport_test_clock_arm();

    uint32_t word;
    if (sep_read_research_pattern_word(&word)) {
        platform_progress(1, word);
    }
    pvpanic_trigger();
}
