#pragma once

#include "avp/protocol/sep/sep_protocol_helper_adapter.h"
#include "avp/devices/usb/usb_dfu_stack_lifecycle_adapter.h"
#include "avp/devices/usb/usb_ep0_state_adapter.h"

#include <stdbool.h>
#include <stdint.h>


#define BOOT_DEVICE_CLOSURE_PUBLIC __attribute__((used, retain))

enum {
    BOOT_DEVICE_DFU_IMAGE_TYPE_IBSS = 0x69627373,
    BOOT_DEVICE_FINAL_PROGRESS = 2,
    BOOT_DEVICE_POLICY_FLAG18 = 0x00040000,
    BOOT_DEVICE_POLICY_FLAG19 = 0x00080000,
    BOOT_DEVICE_MODE2_ENTER = 0x3c505144,
    BOOT_DEVICE_MODE2_EXIT = 0x3e505144,
};

typedef void (*BootDeviceShutdown)(void *context);
typedef void (*BootDeviceProgress)(void *context, uint32_t code);
typedef void (*BootDeviceModeTransition)(
    void *context,
    uint32_t marker,
    bool active
);
typedef bool (*BootDeviceAcceptPolicyFlags)(
    void *context,
    uint32_t flags
);

typedef struct {
    void *context;
    BootDeviceShutdown shutdown;
    BootDeviceProgress progress;
    BootDeviceModeTransition mode_transition;
    BootDeviceAcceptPolicyFlags accept_policy_flags;
} BootDeviceClosureOperations;

typedef struct {
    USBDFUStackLifecycle *usb_stack;
    SEPProtocolHelperSession *sep_session;
    SEPBootstrapResult *sep_result;
    BootDeviceClosureOperations operations;
    uint32_t boot_options;
    uint32_t progress_words[2];
    int8_t mode3_state;
    uint8_t usb_release_observed;
    uint8_t platform_shutdown_observed;
    uint8_t sep_finalize_observed;
} BootDeviceClosureOwner;

BOOT_DEVICE_CLOSURE_PUBLIC void boot_device_closure_owner_init(
    BootDeviceClosureOwner *owner,
    USBDFUStackLifecycle *usb_stack,
    SEPProtocolHelperSession *sep_session,
    SEPBootstrapResult *sep_result,
    BootDeviceClosureOperations operations,
    uint32_t boot_options
);

BOOT_DEVICE_CLOSURE_PUBLIC bool boot_dispatch_validate_dfu_download_span(
    USBDeviceByteSpan span
);
BOOT_DEVICE_CLOSURE_PUBLIC int platform_set_dfu_mode_noop(bool active);
BOOT_DEVICE_CLOSURE_PUBLIC int j_platform_set_dfu_mode_noop(bool active);
BOOT_DEVICE_CLOSURE_PUBLIC bool platform_claim_force_dfu(
    BootDeviceClosureOwner *owner
);
BOOT_DEVICE_CLOSURE_PUBLIC uint32_t platform_boot_dfu_image_type_ibss(void);
BOOT_DEVICE_CLOSURE_PUBLIC int boot_platform_prepare_sep_shutdown_noop(void);
BOOT_DEVICE_CLOSURE_PUBLIC int boot_finalize_usb_and_platform_services(
    BootDeviceClosureOwner *owner
);
BOOT_DEVICE_CLOSURE_PUBLIC int boot_finalize_sep_and_platform_state(
    BootDeviceClosureOwner *owner,
    uint32_t boot_mode
);
