#include "avp/devices/usb/usb_stack_closure_owner.h"

#include "avp/runtime/ipc/runtime_rpc.h"
#include "device/usbd.h"


enum {
    USB_REQUEST_GET_STATUS = 0,
    USB_REQUEST_CLEAR_FEATURE = 1,
    USB_REQUEST_SET_FEATURE = 3,
    USB_REQUEST_SET_ADDRESS = 5,
    USB_REQUEST_GET_DESCRIPTOR = 6,
    USB_REQUEST_GET_CONFIGURATION = 8,
    USB_REQUEST_SET_CONFIGURATION = 9,
    USB_REQUEST_GET_INTERFACE = 10,
    USB_REQUEST_SET_INTERFACE = 11,
    USB_FEATURE_DEVICE_REMOTE_WAKEUP = 1,
};

static uint16_t minimum_u16(uint16_t left, uint16_t right) {
    return left < right ? left : right;
}

static RuntimeRPCFieldSpec byte_field(void) {
    return (RuntimeRPCFieldSpec){
        .alignment = 1,
        .size = 1,
        .pointer_offset = 0,
        .type_token = AVP_RUNTIME_RPC_NO_POINTER_TOKEN,
    };
}

static RuntimeRPCPayloadSpan byte_payload(const uint8_t *value) {
    return (RuntimeRPCPayloadSpan){
        .current = value,
        .lower_bound = value,
        .upper_bound = value + 1,
        .type_token = AVP_RUNTIME_RPC_NO_POINTER_TOKEN,
    };
}

static bool send_control_data(
    USBControlRequestOwner *owner,
    const tusb_control_request_t *request,
    void *buffer,
    uint16_t length
) {
    if (!tud_control_xfer(
        0,
        request,
        buffer,
        minimum_u16(length, request->wLength)
    )) {
        return false;
    }
    owner->ep0->active_request = *request;
    owner->ep0->status_stage_active = 1;
    return true;
}

static bool send_control_status(
    USBControlRequestOwner *owner,
    const tusb_control_request_t *request
) {
    if (!tud_control_status(0, request)) {
        return false;
    }
    owner->ep0->active_request = *request;
    owner->ep0->status_stage_active = 1;
    return true;
}

static USBControlDispatchResult finish_dispatch(
    USBControlRequestOwner *owner,
    bool accepted
) {
    if (accepted) {
        ++owner->accepted_count;
        return USB_CONTROL_DISPATCH_ACCEPTED;
    }
    ++owner->stalled_count;
    owner->ep0->status_stage_active = 0;
    return USB_CONTROL_DISPATCH_STALLED;
}

void usb_stack_closure_owner_init(
    USBStackClosureOwner *owner,
    USBDFUStackLifecycle *stack,
    const USBDFUStackRegistry *registry,
    USBRPCCommandOwner *rpc,
    USBStackLocality locality,
    uint64_t stack_guard
) {
    *owner = (USBStackClosureOwner){
        .stack = stack,
        .registry = registry,
        .rpc = rpc,
        .locality = locality,
        .stack_guard = stack_guard,
    };
}

bool usb_rpc_outlined_stack_guard_compare_fp_minus_18(
    const USBStackClosureOwner *owner,
    const USBRPCFrame90 *frame
) {
    return owner->stack_guard == frame->saved_guard_fp_minus_18;
}

bool usb_rpc_outlined_stack_guard_compare_fp_minus_38(
    const USBStackClosureOwner *owner,
    const USBRPCFrame90 *frame
) {
    return owner->stack_guard == frame->saved_guard_fp_minus_38;
}

USBRPCFrameRegisters usb_rpc_outlined_frame90_epilogue(
    const USBRPCFrame90 *frame
) {
    return frame->saved;
}

void usb_rpc_outlined_frame90_prologue(
    USBRPCFrame90 *frame,
    USBRPCFrameRegisters registers
) {
    frame->saved = registers;
}

uint64_t usb_rpc_stack_guard_load_to_x8(
    const USBStackClosureOwner *owner
) {
    return owner->stack_guard;
}

void usb_rpc_outlined_stack_guard_save_fp_minus_18(
    const USBStackClosureOwner *owner,
    USBRPCFrame90 *frame
) {
    frame->saved_guard_fp_minus_18 = owner->stack_guard;
}

bool usb_rpc_outlined_stack_guard_compare_saved_x8(
    const USBStackClosureOwner *owner,
    uint64_t saved_guard
) {
    return owner->stack_guard == saved_guard;
}

int root_rpc_usb_lifecycle_notify(
    USBRPCCommandOwner *rpc,
    uint8_t phase,
    uint8_t status
) {
    if (rpc == 0 || rpc->transport == 0) {
        return USB_RPC_COMMAND_ARGUMENT_ERROR;
    }
    RuntimeRPCBuilder builder;
    runtime_rpc_builder_initialize(&builder, rpc->arena, sizeof(rpc->arena));
    if (runtime_rpc_builder_append_field(
            &builder,
            (RuntimeRPCFieldSpec){0},
            (RuntimeRPCPayloadSpan){0}
        ) == 0 ||
        runtime_rpc_builder_append_field(
            &builder,
            byte_field(),
            byte_payload(&phase)
        ) == 0 ||
        runtime_rpc_builder_append_field(
            &builder,
            byte_field(),
            byte_payload(&status)
        ) == 0 ||
        runtime_rpc_builder_finalize(&builder) == 0) {
        return USB_RPC_COMMAND_PACKET_ERROR;
    }
    uint32_t response = UINT32_MAX;
    const int transport_status = rpc->transport(
        USB_STACK_RPC_LIFECYCLE_COMMAND,
        runtime_rpc_builder_view(&builder),
        &response,
        rpc->transport_context
    );
    rpc->last_command = USB_STACK_RPC_LIFECYCLE_COMMAND;
    rpc->last_response = response;
    rpc->last_transport_status = transport_status;
    ++rpc->call_count;
    if (transport_status != USB_RPC_COMMAND_OK) {
        return USB_RPC_COMMAND_TRANSPORT_ERROR;
    }
    return response == 0 ? USB_RPC_COMMAND_OK :
        USB_RPC_COMMAND_REMOTE_ERROR;
}

int j_usb_dfu_stack_acquire(USBStackClosureOwner *owner) {
    return usb_dfu_stack_acquire(owner->stack, owner->registry);
}

int usb_dfu_device_start(USBStackClosureOwner *owner) {
    if (owner->started != 0) {
        return 0;
    }
    int status;
    if (owner->locality == USB_STACK_LOCALITY_LOCAL) {
        status = j_usb_dfu_stack_acquire(owner);
    } else {
        status = usb_rpc_device_initialize(
            owner->rpc,
            (USBRPCCommandArguments){0}
        );
        if (status == 0) {
            status = usb_rpc_commit_dfu_descriptors(owner->rpc);
        }
    }
    if (status != 0) {
        return status;
    }
    if (owner->rpc != 0) {
        status = root_rpc_usb_lifecycle_notify(owner->rpc, 1, 0);
        if (status != 0) {
            if (owner->locality == USB_STACK_LOCALITY_LOCAL) {
                (void)usb_dfu_stack_release(owner->stack);
            } else {
                (void)usb_rpc_device_deinitialize(owner->rpc);
            }
            return status;
        }
    }
    owner->started = 1;
    ++owner->start_count;
    return 0;
}

int usb_dfu_device_shutdown(USBStackClosureOwner *owner) {
    if (owner->started == 0) {
        return 0;
    }
    int notify_status = 0;
    if (owner->rpc != 0) {
        notify_status = root_rpc_usb_lifecycle_notify(owner->rpc, 0, 0);
    }
    const int lifecycle_status =
        owner->locality == USB_STACK_LOCALITY_LOCAL ?
        (usb_dfu_stack_release(owner->stack) ? 0 : -1) :
        usb_rpc_device_deinitialize(owner->rpc);
    owner->started = 0;
    ++owner->shutdown_count;
    return notify_status != 0 ? notify_status : lifecycle_status;
}

int usb_dfu_device_deinitialize(USBStackClosureOwner *owner) {
    const int status = usb_dfu_device_shutdown(owner);
    ++owner->deinitialize_count;
    return status;
}

USBControlDispatchResult usb_dispatch_control_request(
    USBControlRequestOwner *owner,
    const tusb_control_request_t *request
) {
    if (owner == 0 || owner->ep0 == 0 || request == 0 ||
        !usb_ep0_control_request_preflight_allow(request)) {
        return USB_CONTROL_DISPATCH_STALLED;
    }
    owner->ep0->active_request = *request;
    const uint8_t recipient = request->bmRequestType_bit.recipient;
    const uint8_t type = request->bmRequestType_bit.type;
    if (type != TUSB_REQ_TYPE_STANDARD) {
        uint8_t *buffer = 0;
        uint16_t length = 0;
        const bool accepted = owner->custom_dispatch != 0 &&
            owner->custom_dispatch(
                owner->callback_context,
                request,
                &buffer,
                &length
            ) &&
            (length == 0 ? send_control_status(owner, request) :
                send_control_data(owner, request, buffer, length));
        return finish_dispatch(owner, accepted);
    }

    bool accepted = false;
    switch (request->bRequest) {
        case USB_REQUEST_GET_STATUS:
            accepted = send_control_data(
                owner,
                request,
                &owner->status_word,
                sizeof(owner->status_word)
            );
            break;
        case USB_REQUEST_CLEAR_FEATURE:
        case USB_REQUEST_SET_FEATURE:
            if (recipient == TUSB_REQ_RCPT_DEVICE &&
                request->wValue == USB_FEATURE_DEVICE_REMOTE_WAKEUP) {
                owner->remote_wakeup =
                    request->bRequest == USB_REQUEST_SET_FEATURE;
                accepted = send_control_status(owner, request);
            }
            break;
        case USB_REQUEST_SET_ADDRESS:
            if (recipient == TUSB_REQ_RCPT_DEVICE &&
                request->wValue <= 0x7f && request->wLength == 0) {
                owner->address = (uint8_t)request->wValue;
                accepted = send_control_status(owner, request);
            }
            break;
        case USB_REQUEST_GET_DESCRIPTOR:
            if (owner->descriptor_lookup != 0) {
                uint16_t length = 0;
                const uint8_t *descriptor = owner->descriptor_lookup(
                    owner->callback_context,
                    (uint8_t)(request->wValue >> 8),
                    (uint8_t)request->wValue,
                    request->wIndex,
                    &length
                );
                accepted = descriptor != 0 && send_control_data(
                    owner,
                    request,
                    (void *)(uintptr_t)descriptor,
                    length
                );
            }
            break;
        case USB_REQUEST_GET_CONFIGURATION:
            owner->response_byte = owner->configuration;
            accepted = send_control_data(
                owner,
                request,
                &owner->response_byte,
                1
            );
            break;
        case USB_REQUEST_SET_CONFIGURATION:
            if (recipient == TUSB_REQ_RCPT_DEVICE &&
                request->wValue <= UINT8_MAX && request->wLength == 0) {
                owner->configuration = (uint8_t)request->wValue;
                accepted = send_control_status(owner, request);
            }
            break;
        case USB_REQUEST_GET_INTERFACE:
            if (request->wIndex < USB_STACK_CONTROL_INTERFACE_COUNT) {
                owner->response_byte =
                    owner->interface_alternate[request->wIndex];
                accepted = send_control_data(
                    owner,
                    request,
                    &owner->response_byte,
                    1
                );
            }
            break;
        case USB_REQUEST_SET_INTERFACE:
            if (request->wIndex < USB_STACK_CONTROL_INTERFACE_COUNT &&
                request->wValue <= UINT8_MAX && request->wLength == 0) {
                owner->interface_alternate[request->wIndex] =
                    (uint8_t)request->wValue;
                accepted = send_control_status(owner, request);
            }
            break;
        default:
            break;
    }
    return finish_dispatch(owner, accepted);
}

uintptr_t usb_ep0_outlined_load_qword_type_x3(void) {
    return USB_STACK_CONTROL_QWORD_TYPE;
}

USBControlSourceSpan usb_ep0_outlined_copy_saved_source_span_x24_x21(
    USBControlSourceSpan source
) {
    return source;
}
