#include "avp/runtime/boot/boot_policy.h"


enum {
    AVP_BOOT_OPTION_FORCE_DFU = 1,
    AVP_BOOT_CONTEXT_INITIALIZED = 1,
    AVP_BOOT_CONTEXT_BOOT_NONCE_VALID = 2,
    /* iBoot reads this bit of the published status as "Remote boot": the
     * image it was handed came over USB rather than from local storage. */
    AVP_BOOT_CONTEXT_REMOTE_BOOT = 0x20,
};

AVPBootPolicy boot_policy_decode(uint32_t configuration, uint32_t options) {
    return (AVPBootPolicy){
        .configuration = configuration,
        .options = options,
        .selection = (options & AVP_BOOT_OPTION_FORCE_DFU) != 0
            ? AVP_BOOT_SELECTION_FORCE_DFU
            : AVP_BOOT_SELECTION_PLATFORM,
    };
}

AVPBootContext boot_context_create(
    uint8_t configuration,
    uint8_t board_id,
    uint8_t security_epoch
) {
    AVPBootContext context;
    boot_context_initialize(&context, security_epoch);
    (void)boot_context_populate_platform(&context, configuration, board_id);
    return context;
}

void boot_context_initialize(
    AVPBootContext *context,
    uint8_t security_epoch
) {
    *context = (AVPBootContext){
        .security_epoch = security_epoch,
    };
}

int boot_context_populate_platform(
    AVPBootContext *context,
    uint8_t configuration,
    uint8_t board_id
) {
    if ((context->status & AVP_BOOT_CONTEXT_INITIALIZED) == 0) {
        context->configuration = configuration;
        context->board_id = board_id;
        context->status |= AVP_BOOT_CONTEXT_INITIALIZED;
    }
    return 0;
}

void boot_dispatch_preflight_hook_noop(void) {
}

void boot_context_mark_boot_nonce_valid(AVPBootContext *context) {
    context->status |= AVP_BOOT_CONTEXT_BOOT_NONCE_VALID;
}

void boot_context_mark_remote_boot(AVPBootContext *context) {
    context->status |= AVP_BOOT_CONTEXT_REMOTE_BOOT;
}

bool boot_source_next_candidate(
    const AVPBootContext *context,
    int32_t attempt,
    AVPBootSourceCandidate *candidate
) {
    if (context->configuration < 1 || context->configuration > 2) {
        return false;
    }
    *candidate = (AVPBootSourceCandidate){
        .attempt = attempt,
        .source = attempt == 0
            ? AVP_BOOT_SOURCE_VIRTUAL_FIRMWARE
            : AVP_BOOT_SOURCE_DFU,
        .selector = 0,
        .instance = 0,
    };
    return true;
}
