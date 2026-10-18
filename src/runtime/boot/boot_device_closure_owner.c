#include "avp/runtime/boot/boot_device_closure_owner.h"

#include "avp/runtime/memory/runtime_memory.h"


void boot_device_closure_owner_init(
    BootDeviceClosureOwner *owner,
    USBDFUStackLifecycle *usb_stack,
    SEPProtocolHelperSession *sep_session,
    SEPBootstrapResult *sep_result,
    BootDeviceClosureOperations operations,
    uint32_t boot_options
) {
    *owner = (BootDeviceClosureOwner){
        .usb_stack = usb_stack,
        .sep_session = sep_session,
        .sep_result = sep_result,
        .operations = operations,
        .boot_options = boot_options,
    };
}

bool boot_dispatch_validate_dfu_download_span(USBDeviceByteSpan span) {
    return usb_device_validate_byte_span(span);
}

int platform_set_dfu_mode_noop(bool active) {
    (void)active;
    return 0;
}

int j_platform_set_dfu_mode_noop(bool active) {
    return platform_set_dfu_mode_noop(active);
}

bool platform_claim_force_dfu(BootDeviceClosureOwner *owner) {
    const bool claimed = (owner->boot_options & 1u) != 0;
    owner->boot_options &= ~1u;
    return claimed;
}

uint32_t platform_boot_dfu_image_type_ibss(void) {
    return BOOT_DEVICE_DFU_IMAGE_TYPE_IBSS;
}

int boot_platform_prepare_sep_shutdown_noop(void) {
    return 0;
}

int boot_finalize_usb_and_platform_services(BootDeviceClosureOwner *owner) {
    owner->operations.mode_transition(
        owner->operations.context,
        BOOT_DEVICE_FINAL_PROGRESS,
        false
    );
    owner->usb_release_observed =
        usb_dfu_stack_release(owner->usb_stack) ? 1 : 0;
    owner->operations.shutdown(owner->operations.context);
    owner->platform_shutdown_observed = 1;
    owner->operations.progress(
        owner->operations.context,
        BOOT_DEVICE_FINAL_PROGRESS
    );
    return 0;
}

int boot_finalize_sep_and_platform_state(
    BootDeviceClosureOwner *owner,
    uint32_t boot_mode
) {
    if (boot_mode == 2) {
        owner->operations.mode_transition(
            owner->operations.context,
            BOOT_DEVICE_MODE2_ENTER,
            false
        );
        (void)boot_platform_prepare_sep_shutdown_noop();
        owner->operations.mode_transition(
            owner->operations.context,
            BOOT_DEVICE_MODE2_EXIT,
            false
        );
    } else if (boot_mode == 0) {
        (void)boot_platform_prepare_sep_shutdown_noop();
    } else {
        if (boot_mode == 3 && owner->mode3_state < 0) {
            owner->mode3_state &= 0x7f;
        }
        runtime_firebloom_panic();
    }

    (void)owner->operations.accept_policy_flags(
        owner->operations.context,
        BOOT_DEVICE_POLICY_FLAG18
    );
    if (!owner->operations.accept_policy_flags(
            owner->operations.context,
            BOOT_DEVICE_POLICY_FLAG19
        )) {
        owner->progress_words[0] = 0;
        owner->progress_words[1] = 0;
    }

    const int status = sep_finalize_eight_round_handshake(
        owner->sep_session,
        owner->sep_result
    );
    if (status != SEP_PROTOCOL_HELPER_OK) {
        runtime_firebloom_panic();
    }
    owner->sep_finalize_observed = 1;
    return 0;
}
