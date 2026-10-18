#include "avp/platform/platform.h"
#include "avp/runtime/exception/pvpanic.h"
#include "avp/protocol/sep/sep.h"


enum {
    BOOTSTRAP_PROBE_STAGE = 0x53455038,
};

void firmware_main(void) {
    pvpanic_initialize();
    platform_stage(BOOTSTRAP_PROBE_STAGE);

    const SEPBootstrapResult result = sep_bootstrap_with_finalize();
    uint32_t configuration_or = 0;
    for (uint32_t index = 0;
         index < sizeof(result.configuration_words) / sizeof(uint32_t);
         ++index) {
        configuration_or |= result.configuration_words[index];
    }
    platform_progress(0, result.bootstrap_payload);
    platform_progress(1, result.finalize_rounds);
    platform_progress(2, configuration_or);
    platform_progress(3, result.finalize_response_opcode);
    pvpanic_trigger();
}
