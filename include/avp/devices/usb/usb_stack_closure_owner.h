#pragma once

#include "avp/devices/usb/usb_dfu_stack_lifecycle_adapter.h"
#include "avp/devices/usb/usb_ep0_state_adapter.h"
#include "avp/devices/usb/usb_rpc_command_owner.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


#define USB_STACK_CLOSURE_PUBLIC __attribute__((used, retain))

enum {
    USB_STACK_CONTROL_INTERFACE_COUNT = 10,
    USB_STACK_CONTROL_QWORD_TYPE = 8,
    USB_STACK_RPC_LIFECYCLE_COMMAND = 0x13,
};

typedef enum {
    USB_STACK_LOCALITY_LOCAL = 0,
    USB_STACK_LOCALITY_REMOTE = 1,
} USBStackLocality;

typedef enum {
    USB_CONTROL_DISPATCH_STALLED = 0,
    USB_CONTROL_DISPATCH_ACCEPTED = 1,
} USBControlDispatchResult;

typedef struct {
    uint64_t x19;
    uint64_t x20;
    uint64_t x21;
    uint64_t x22;
    uint64_t x23;
    uint64_t x24;
} USBRPCFrameRegisters;

typedef struct {
    uint8_t reserved[0x50];
    USBRPCFrameRegisters saved;
    uint64_t saved_guard_fp_minus_38;
    uint64_t saved_guard_fp_minus_18;
} USBRPCFrame90;

typedef struct {
    const uint8_t *current;
    uintptr_t lower_bound;
    uintptr_t upper_bound;
    uintptr_t type;
} USBControlSourceSpan;

typedef const uint8_t *(*USBControlDescriptorLookup)(
    void *context,
    uint8_t descriptor_type,
    uint8_t descriptor_index,
    uint16_t language_id,
    uint16_t *length
);

typedef bool (*USBControlCustomDispatch)(
    void *context,
    const tusb_control_request_t *request,
    uint8_t **buffer,
    uint16_t *length
);

typedef struct {
    USBEP0State *ep0;
    USBControlDescriptorLookup descriptor_lookup;
    USBControlCustomDispatch custom_dispatch;
    void *callback_context;
    uint16_t status_word;
    uint8_t configuration;
    uint8_t address;
    uint8_t remote_wakeup;
    uint8_t interface_alternate[USB_STACK_CONTROL_INTERFACE_COUNT];
    uint8_t response_byte;
    uint32_t accepted_count;
    uint32_t stalled_count;
} USBControlRequestOwner;

typedef struct {
    USBDFUStackLifecycle *stack;
    const USBDFUStackRegistry *registry;
    USBRPCCommandOwner *rpc;
    USBStackLocality locality;
    uint64_t stack_guard;
    uint32_t start_count;
    uint32_t shutdown_count;
    uint32_t deinitialize_count;
    uint8_t started;
} USBStackClosureOwner;

_Static_assert(offsetof(USBRPCFrame90, saved) == 0x50,
    "frame90 register offset");
_Static_assert(offsetof(USBRPCFrame90, saved_guard_fp_minus_18) == 0x88,
    "frame90 guard offset");

USB_STACK_CLOSURE_PUBLIC void usb_stack_closure_owner_init(
    USBStackClosureOwner *owner,
    USBDFUStackLifecycle *stack,
    const USBDFUStackRegistry *registry,
    USBRPCCommandOwner *rpc,
    USBStackLocality locality,
    uint64_t stack_guard
);

USB_STACK_CLOSURE_PUBLIC bool
usb_rpc_outlined_stack_guard_compare_fp_minus_18(
    const USBStackClosureOwner *owner,
    const USBRPCFrame90 *frame
);

USB_STACK_CLOSURE_PUBLIC bool
usb_rpc_outlined_stack_guard_compare_fp_minus_38(
    const USBStackClosureOwner *owner,
    const USBRPCFrame90 *frame
);

USB_STACK_CLOSURE_PUBLIC USBRPCFrameRegisters
usb_rpc_outlined_frame90_epilogue(const USBRPCFrame90 *frame);

USB_STACK_CLOSURE_PUBLIC void usb_rpc_outlined_frame90_prologue(
    USBRPCFrame90 *frame,
    USBRPCFrameRegisters registers
);

USB_STACK_CLOSURE_PUBLIC uint64_t usb_rpc_stack_guard_load_to_x8(
    const USBStackClosureOwner *owner
);

USB_STACK_CLOSURE_PUBLIC void
usb_rpc_outlined_stack_guard_save_fp_minus_18(
    const USBStackClosureOwner *owner,
    USBRPCFrame90 *frame
);

USB_STACK_CLOSURE_PUBLIC bool
usb_rpc_outlined_stack_guard_compare_saved_x8(
    const USBStackClosureOwner *owner,
    uint64_t saved_guard
);

USB_STACK_CLOSURE_PUBLIC int j_usb_dfu_stack_acquire(
    USBStackClosureOwner *owner
);

USB_STACK_CLOSURE_PUBLIC int usb_dfu_device_start(
    USBStackClosureOwner *owner
);

USB_STACK_CLOSURE_PUBLIC int usb_dfu_device_shutdown(
    USBStackClosureOwner *owner
);

USB_STACK_CLOSURE_PUBLIC int usb_dfu_device_deinitialize(
    USBStackClosureOwner *owner
);

USB_STACK_CLOSURE_PUBLIC USBControlDispatchResult
usb_dispatch_control_request(
    USBControlRequestOwner *owner,
    const tusb_control_request_t *request
);

USB_STACK_CLOSURE_PUBLIC uintptr_t usb_ep0_outlined_load_qword_type_x3(void);

USB_STACK_CLOSURE_PUBLIC USBControlSourceSpan
usb_ep0_outlined_copy_saved_source_span_x24_x21(
    USBControlSourceSpan source
);

USB_STACK_CLOSURE_PUBLIC int root_rpc_usb_lifecycle_notify(
    USBRPCCommandOwner *rpc,
    uint8_t phase,
    uint8_t status
);
