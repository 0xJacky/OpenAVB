#include "avp/runtime/boot/boot_policy.h"

#include <assert.h>
#include <string.h>


int main(void) {
    AVPBootContext initialized;
    memset(&initialized, 0xff, sizeof(initialized));
    boot_context_initialize(&initialized, 7);
    assert(initialized.status == 0);
    assert(initialized.configuration == 0);
    assert(initialized.board_id == 0);
    assert(initialized.security_epoch == 7);
    assert(initialized.reserved[0] == 0);
    assert(initialized.reserved[1] == 0);
    assert(initialized.reserved[2] == 0);
    assert(boot_context_populate_platform(&initialized, 2, 9) == 0);
    assert(initialized.status == 1);
    assert(initialized.configuration == 2);
    assert(initialized.board_id == 9);
    assert(boot_context_populate_platform(&initialized, 1, 3) == 0);
    assert(initialized.configuration == 2);
    assert(initialized.board_id == 9);
    boot_dispatch_preflight_hook_noop();

    AVPBootPolicy policy = boot_policy_decode(1, 1);
    assert(policy.configuration == 1);
    assert(policy.options == 1);
    assert(policy.selection == AVP_BOOT_SELECTION_FORCE_DFU);

    policy = boot_policy_decode(1, 0x10101);
    assert(policy.options == 0x10101);
    assert(policy.selection == AVP_BOOT_SELECTION_FORCE_DFU);

    policy = boot_policy_decode(1, 0x10100);
    assert(policy.selection == AVP_BOOT_SELECTION_PLATFORM);

    const AVPBootContext context = boot_context_create(1, 0x90, 1);
    const uint8_t expected_context[] = {
        0x01, 0x01, 0x90, 0x01,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
    };
    assert(sizeof(context) == sizeof(expected_context));
    assert(memcmp(&context, expected_context, sizeof(context)) == 0);

    AVPBootContext nonce_context = context;
    boot_context_mark_boot_nonce_valid(&nonce_context);
    assert(nonce_context.status == 3);

    AVPBootSourceCandidate candidate;
    assert(boot_source_next_candidate(&context, 0, &candidate));
    assert(candidate.attempt == 0);
    assert(candidate.source == AVP_BOOT_SOURCE_VIRTUAL_FIRMWARE);
    assert(candidate.selector == 0);
    assert(candidate.instance == 0);

    assert(boot_source_next_candidate(&context, 1, &candidate));
    assert(candidate.source == AVP_BOOT_SOURCE_DFU);
    assert(boot_source_next_candidate(&context, -1, &candidate));
    assert(candidate.source == AVP_BOOT_SOURCE_DFU);

    AVPBootContext invalid = boot_context_create(0, 0x90, 1);
    assert(!boot_source_next_candidate(&invalid, 0, &candidate));
    invalid.configuration = 3;
    assert(!boot_source_next_candidate(&invalid, 0, &candidate));
}
