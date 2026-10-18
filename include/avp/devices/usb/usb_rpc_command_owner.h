#pragma once

#include "avp/runtime/ipc/runtime_rpc.h"

#include <stddef.h>
#include <stdint.h>


#define USB_RPC_COMMAND_OWNER_PUBLIC __attribute__((used, retain))

enum {
    USB_RPC_COMMAND_DEVICE_DEINITIALIZE = 0x02,
    USB_RPC_COMMAND_DEVICE_INITIALIZE = 0x03,
    USB_RPC_COMMAND_BUILD_CONFIGURATION = 0x04,
    USB_RPC_COMMAND_FINALIZE_CALLBACKS = 0x05,
    USB_RPC_COMMAND_REGISTER_MAPPING = 0x09,
    USB_RPC_COMMAND_RELEASE_MAPPING = 0x0a,
    USB_RPC_COMMAND_COMMIT_DFU_DESCRIPTORS = 0x0b,
    USB_RPC_COMMAND_SET_BYTE_0X11 = 0x11,
    USB_RPC_COMMAND_DISPATCH_SLOT0 = 0x12,
    USB_RPC_COMMAND_DISPATCH_SLOT2 = 0x13,
    USB_RPC_COMMAND_DISPATCH_SLOT3 = 0x14,
    USB_RPC_COMMAND_DISPATCH_SLOT1 = 0x15,
    USB_RPC_COMMAND_DFU_INITIALIZE = 0x17,
    USB_RPC_COMMAND_SET_BYTE_0X1A = 0x1a,
    USB_RPC_COMMAND_DFU_DEINITIALIZE = 0x1b,
    USB_RPC_COMMAND_DFU_DOWNLOAD_COMPLETE = 0x1c,
    USB_RPC_COMMAND_DFU_DOWNLOAD_EVENT = 0x1d,
    USB_RPC_COMMAND_SET_BYTE_0X1F = 0x1f,
    USB_RPC_COMMAND_DFU_PAYLOAD_LENGTH = 0x20,
    USB_RPC_COMMAND_SIMPLEVIRTIO_INITIALIZE = 0x3d,
    USB_RPC_COMMAND_OK = 0,
    USB_RPC_COMMAND_TRANSPORT_ERROR = -1,
    USB_RPC_COMMAND_PACKET_ERROR = -2,
    USB_RPC_COMMAND_ARGUMENT_ERROR = -3,
    USB_RPC_COMMAND_REMOTE_ERROR = -4,
    USB_RPC_COMMAND_ARENA_WORDS = 512,
    USB_RPC_COMMAND_MAX_ARGUMENTS = 8,
};

typedef int (*USBRPCCommandTransport)(
    uint32_t command,
    RuntimeRPCPacketView request,
    uint32_t *response,
    void *context
);

typedef struct {
    RuntimeRPCPayloadSpan fields[USB_RPC_COMMAND_MAX_ARGUMENTS];
    uint8_t field_count;
} USBRPCCommandArguments;

typedef struct {
    uint64_t arena[USB_RPC_COMMAND_ARENA_WORDS];
    USBRPCCommandTransport transport;
    void *transport_context;
    uint32_t last_command;
    uint32_t last_response;
    int last_transport_status;
    uint32_t call_count;
} USBRPCCommandOwner;

USB_RPC_COMMAND_OWNER_PUBLIC int usb_rpc_device_deinitialize(
    USBRPCCommandOwner *owner
);
USB_RPC_COMMAND_OWNER_PUBLIC int usb_rpc_device_initialize(
    USBRPCCommandOwner *owner,
    USBRPCCommandArguments arguments
);
USB_RPC_COMMAND_OWNER_PUBLIC int usb_rpc_build_configuration_descriptors(
    USBRPCCommandOwner *owner,
    uint32_t *descriptor_length
);
USB_RPC_COMMAND_OWNER_PUBLIC int usb_rpc_finalize_device_callbacks(
    USBRPCCommandOwner *owner
);
USB_RPC_COMMAND_OWNER_PUBLIC int usb_rpc_register_transport_mapping(
    USBRPCCommandOwner *owner,
    USBRPCCommandArguments arguments,
    uint32_t *mapping_id
);
USB_RPC_COMMAND_OWNER_PUBLIC int usb_rpc_release_transport_mapping(
    USBRPCCommandOwner *owner
);
USB_RPC_COMMAND_OWNER_PUBLIC int usb_rpc_commit_dfu_descriptors(
    USBRPCCommandOwner *owner
);
USB_RPC_COMMAND_OWNER_PUBLIC int usb_rpc_command_0x11_set_byte(
    USBRPCCommandOwner *owner,
    uint8_t value
);
USB_RPC_COMMAND_OWNER_PUBLIC int usb_rpc_device_dispatch_slot0(
    USBRPCCommandOwner *owner,
    uint32_t *result
);
USB_RPC_COMMAND_OWNER_PUBLIC int usb_rpc_device_dispatch_slot2(
    USBRPCCommandOwner *owner,
    uint32_t *result
);
USB_RPC_COMMAND_OWNER_PUBLIC int usb_rpc_device_dispatch_slot3(
    USBRPCCommandOwner *owner
);
USB_RPC_COMMAND_OWNER_PUBLIC int usb_rpc_device_dispatch_slot1(
    USBRPCCommandOwner *owner
);
USB_RPC_COMMAND_OWNER_PUBLIC int usb_rpc_dfu_initialize(
    USBRPCCommandOwner *owner,
    uint32_t *result
);
USB_RPC_COMMAND_OWNER_PUBLIC int usb_rpc_command_0x1a_set_byte(
    USBRPCCommandOwner *owner,
    uint8_t value
);
USB_RPC_COMMAND_OWNER_PUBLIC int usb_rpc_dfu_deinitialize(
    USBRPCCommandOwner *owner
);
USB_RPC_COMMAND_OWNER_PUBLIC int usb_rpc_dfu_download_is_complete(
    USBRPCCommandOwner *owner,
    uint8_t *complete
);
USB_RPC_COMMAND_OWNER_PUBLIC int usb_rpc_dfu_download_event_handle(
    USBRPCCommandOwner *owner,
    uint16_t event,
    uint16_t *result
);
USB_RPC_COMMAND_OWNER_PUBLIC int usb_rpc_command_0x1f_set_byte(
    USBRPCCommandOwner *owner,
    uint8_t value
);
USB_RPC_COMMAND_OWNER_PUBLIC int usb_rpc_dfu_download_payload_length(
    USBRPCCommandOwner *owner,
    uint32_t *length
);
USB_RPC_COMMAND_OWNER_PUBLIC int usb_rpc_simplevirtio_initialize(
    USBRPCCommandOwner *owner
);

RuntimeRPCPayloadSpan usb_rpc_command_payload(
    const void *value,
    size_t size,
    uintptr_t type_token
);
