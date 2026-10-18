#include "avp/devices/usb/usb_rpc_command_owner.h"


static RuntimeRPCFieldSpec field_spec(RuntimeRPCPayloadSpan payload) {
    const size_t size = (size_t)(
        (const uint8_t *)payload.upper_bound -
        (const uint8_t *)payload.current
    );
    size_t alignment = 1;
    while (alignment < size && alignment < sizeof(uint64_t)) {
        alignment <<= 1;
    }
    return (RuntimeRPCFieldSpec){
        .alignment = alignment,
        .size = size,
        .pointer_offset = 0,
        .type_token = payload.type_token,
    };
}

RuntimeRPCPayloadSpan usb_rpc_command_payload(
    const void *value,
    size_t size,
    uintptr_t type_token
) {
    return (RuntimeRPCPayloadSpan){
        .current = value,
        .lower_bound = value,
        .upper_bound = (const uint8_t *)value + size,
        .type_token = type_token,
    };
}

static bool payload_valid(RuntimeRPCPayloadSpan payload) {
    return payload.current != 0 && payload.lower_bound != 0 &&
        payload.upper_bound != 0 &&
        (uintptr_t)payload.lower_bound <= (uintptr_t)payload.current &&
        (uintptr_t)payload.current < (uintptr_t)payload.upper_bound;
}

static int command_call(
    USBRPCCommandOwner *owner,
    uint32_t command,
    USBRPCCommandArguments arguments,
    uint32_t *response
) {
    if (owner == 0 || owner->transport == 0 ||
        arguments.field_count > USB_RPC_COMMAND_MAX_ARGUMENTS) {
        return USB_RPC_COMMAND_ARGUMENT_ERROR;
    }
    RuntimeRPCBuilder builder;
    runtime_rpc_builder_initialize(&builder, owner->arena, sizeof(owner->arena));
    if (runtime_rpc_builder_append_field(
            &builder,
            (RuntimeRPCFieldSpec){0},
            (RuntimeRPCPayloadSpan){0}
        ) == 0) {
        return USB_RPC_COMMAND_PACKET_ERROR;
    }
    for (uint8_t index = 0; index < arguments.field_count; ++index) {
        const RuntimeRPCPayloadSpan payload = arguments.fields[index];
        if (!payload_valid(payload) || runtime_rpc_builder_append_field(
                &builder,
                field_spec(payload),
                payload
            ) == 0) {
            return USB_RPC_COMMAND_PACKET_ERROR;
        }
    }
    if (runtime_rpc_builder_finalize(&builder) == 0) {
        return USB_RPC_COMMAND_PACKET_ERROR;
    }
    uint32_t remote_response = 0;
    const int status = owner->transport(
        command,
        runtime_rpc_builder_view(&builder),
        &remote_response,
        owner->transport_context
    );
    owner->last_command = command;
    owner->last_response = remote_response;
    owner->last_transport_status = status;
    ++owner->call_count;
    if (status != USB_RPC_COMMAND_OK) {
        return USB_RPC_COMMAND_TRANSPORT_ERROR;
    }
    if (response != 0) {
        *response = remote_response;
    }
    return USB_RPC_COMMAND_OK;
}

static int command_expect_zero(
    USBRPCCommandOwner *owner,
    uint32_t command,
    USBRPCCommandArguments arguments
) {
    uint32_t response;
    const int status = command_call(owner, command, arguments, &response);
    if (status != USB_RPC_COMMAND_OK) {
        return status;
    }
    return response == 0 ? USB_RPC_COMMAND_OK : USB_RPC_COMMAND_REMOTE_ERROR;
}

static int command_without_arguments(
    USBRPCCommandOwner *owner,
    uint32_t command
) {
    return command_expect_zero(owner, command, (USBRPCCommandArguments){0});
}

static int command_with_result(
    USBRPCCommandOwner *owner,
    uint32_t command,
    uint32_t *result
) {
    if (result == 0) {
        return USB_RPC_COMMAND_ARGUMENT_ERROR;
    }
    return command_call(owner, command, (USBRPCCommandArguments){0}, result);
}

static int command_with_byte(
    USBRPCCommandOwner *owner,
    uint32_t command,
    uint8_t value
) {
    const USBRPCCommandArguments arguments = {
        .fields = {
            usb_rpc_command_payload(
                &value,
                sizeof(value),
                AVP_RUNTIME_RPC_NO_POINTER_TOKEN
            ),
        },
        .field_count = 1,
    };
    return command_expect_zero(owner, command, arguments);
}

int usb_rpc_device_deinitialize(USBRPCCommandOwner *owner) {
    return command_without_arguments(owner, USB_RPC_COMMAND_DEVICE_DEINITIALIZE);
}

int usb_rpc_device_initialize(
    USBRPCCommandOwner *owner,
    USBRPCCommandArguments arguments
) {
    return command_expect_zero(
        owner,
        USB_RPC_COMMAND_DEVICE_INITIALIZE,
        arguments
    );
}

int usb_rpc_build_configuration_descriptors(
    USBRPCCommandOwner *owner,
    uint32_t *descriptor_length
) {
    return command_with_result(
        owner,
        USB_RPC_COMMAND_BUILD_CONFIGURATION,
        descriptor_length
    );
}

int usb_rpc_finalize_device_callbacks(USBRPCCommandOwner *owner) {
    return command_without_arguments(owner, USB_RPC_COMMAND_FINALIZE_CALLBACKS);
}

int usb_rpc_register_transport_mapping(
    USBRPCCommandOwner *owner,
    USBRPCCommandArguments arguments,
    uint32_t *mapping_id
) {
    if (mapping_id == 0) {
        return USB_RPC_COMMAND_ARGUMENT_ERROR;
    }
    return command_call(
        owner,
        USB_RPC_COMMAND_REGISTER_MAPPING,
        arguments,
        mapping_id
    );
}

int usb_rpc_release_transport_mapping(USBRPCCommandOwner *owner) {
    return command_without_arguments(owner, USB_RPC_COMMAND_RELEASE_MAPPING);
}

int usb_rpc_commit_dfu_descriptors(USBRPCCommandOwner *owner) {
    return command_without_arguments(
        owner,
        USB_RPC_COMMAND_COMMIT_DFU_DESCRIPTORS
    );
}

int usb_rpc_command_0x11_set_byte(
    USBRPCCommandOwner *owner,
    uint8_t value
) {
    return command_with_byte(owner, USB_RPC_COMMAND_SET_BYTE_0X11, value);
}

int usb_rpc_device_dispatch_slot0(
    USBRPCCommandOwner *owner,
    uint32_t *result
) {
    return command_with_result(owner, USB_RPC_COMMAND_DISPATCH_SLOT0, result);
}

int usb_rpc_device_dispatch_slot2(
    USBRPCCommandOwner *owner,
    uint32_t *result
) {
    return command_with_result(owner, USB_RPC_COMMAND_DISPATCH_SLOT2, result);
}

int usb_rpc_device_dispatch_slot3(USBRPCCommandOwner *owner) {
    return command_without_arguments(owner, USB_RPC_COMMAND_DISPATCH_SLOT3);
}

int usb_rpc_device_dispatch_slot1(USBRPCCommandOwner *owner) {
    return command_without_arguments(owner, USB_RPC_COMMAND_DISPATCH_SLOT1);
}

int usb_rpc_dfu_initialize(
    USBRPCCommandOwner *owner,
    uint32_t *result
) {
    return command_with_result(owner, USB_RPC_COMMAND_DFU_INITIALIZE, result);
}

int usb_rpc_command_0x1a_set_byte(
    USBRPCCommandOwner *owner,
    uint8_t value
) {
    return command_with_byte(owner, USB_RPC_COMMAND_SET_BYTE_0X1A, value);
}

int usb_rpc_dfu_deinitialize(USBRPCCommandOwner *owner) {
    return command_without_arguments(owner, USB_RPC_COMMAND_DFU_DEINITIALIZE);
}

int usb_rpc_dfu_download_is_complete(
    USBRPCCommandOwner *owner,
    uint8_t *complete
) {
    if (complete == 0) {
        return USB_RPC_COMMAND_ARGUMENT_ERROR;
    }
    uint32_t result;
    const int status = command_with_result(
        owner,
        USB_RPC_COMMAND_DFU_DOWNLOAD_COMPLETE,
        &result
    );
    if (status == USB_RPC_COMMAND_OK) {
        *complete = (uint8_t)result;
    }
    return status;
}

int usb_rpc_dfu_download_event_handle(
    USBRPCCommandOwner *owner,
    uint16_t event,
    uint16_t *result
) {
    if (result == 0) {
        return USB_RPC_COMMAND_ARGUMENT_ERROR;
    }
    const USBRPCCommandArguments arguments = {
        .fields = {
            usb_rpc_command_payload(
                &event,
                sizeof(event),
                AVP_RUNTIME_RPC_NO_POINTER_TOKEN
            ),
        },
        .field_count = 1,
    };
    uint32_t response;
    const int status = command_call(
        owner,
        USB_RPC_COMMAND_DFU_DOWNLOAD_EVENT,
        arguments,
        &response
    );
    if (status == USB_RPC_COMMAND_OK) {
        *result = (uint16_t)response;
    }
    return status;
}

int usb_rpc_command_0x1f_set_byte(
    USBRPCCommandOwner *owner,
    uint8_t value
) {
    return command_with_byte(owner, USB_RPC_COMMAND_SET_BYTE_0X1F, value);
}

int usb_rpc_dfu_download_payload_length(
    USBRPCCommandOwner *owner,
    uint32_t *length
) {
    return command_with_result(
        owner,
        USB_RPC_COMMAND_DFU_PAYLOAD_LENGTH,
        length
    );
}

int usb_rpc_simplevirtio_initialize(USBRPCCommandOwner *owner) {
    return command_without_arguments(
        owner,
        USB_RPC_COMMAND_SIMPLEVIRTIO_INITIALIZE
    );
}
