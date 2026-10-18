#include "avp/devices/usb/usb_rpc_command_owner.h"

#include <assert.h>
#include <stdio.h>


typedef struct {
    uint32_t commands[32];
    uint8_t field_counts[32];
    uint64_t first_argument[32];
    uint32_t responses[0x40];
    uint32_t call_count;
    int status;
} RPCTranscript;

static int transport(
    uint32_t command,
    RuntimeRPCPacketView request,
    uint32_t *response,
    void *context
) {
    RPCTranscript *transcript = context;
    const uint32_t call = transcript->call_count++;
    transcript->commands[call] = command;
    uint8_t fields = 0;
    const RuntimeRPCFieldHeader *field = runtime_rpc_field_find(request, 0);
    while (field != 0) {
        ++fields;
        if (fields == 2) {
            const uint8_t *payload = runtime_rpc_field_payload_const(field);
            const size_t size = runtime_rpc_field_payload_size(field);
            uint64_t value = 0;
            for (size_t index = 0; index < size; ++index) {
                value |= (uint64_t)payload[index] << (index * 8);
            }
            transcript->first_argument[call] = value;
        }
        field = runtime_rpc_field_next(request, field);
    }
    transcript->field_counts[call] = fields;
    *response = transcript->responses[command];
    return transcript->status;
}

static USBRPCCommandOwner owner(RPCTranscript *transcript) {
    return (USBRPCCommandOwner){
        .transport = transport,
        .transport_context = transcript,
    };
}

static void test_lifecycle_command_ids(void) {
    RPCTranscript transcript = {0};
    USBRPCCommandOwner command_owner = owner(&transcript);
    assert(usb_rpc_device_deinitialize(&command_owner) == 0);
    assert(usb_rpc_finalize_device_callbacks(&command_owner) == 0);
    assert(usb_rpc_release_transport_mapping(&command_owner) == 0);
    assert(usb_rpc_commit_dfu_descriptors(&command_owner) == 0);
    assert(usb_rpc_device_dispatch_slot3(&command_owner) == 0);
    assert(usb_rpc_device_dispatch_slot1(&command_owner) == 0);
    assert(usb_rpc_dfu_deinitialize(&command_owner) == 0);
    assert(usb_rpc_simplevirtio_initialize(&command_owner) == 0);
    const uint32_t expected[] = {2, 5, 10, 11, 20, 21, 27, 61};
    assert(transcript.call_count == sizeof(expected) / sizeof(expected[0]));
    for (size_t index = 0; index < transcript.call_count; ++index) {
        assert(transcript.commands[index] == expected[index]);
        assert(transcript.field_counts[index] == 1);
    }
}

static void test_single_byte_commands(void) {
    RPCTranscript transcript = {0};
    USBRPCCommandOwner command_owner = owner(&transcript);
    assert(usb_rpc_command_0x11_set_byte(&command_owner, 0x51) == 0);
    assert(usb_rpc_command_0x1a_set_byte(&command_owner, 0x62) == 0);
    assert(usb_rpc_command_0x1f_set_byte(&command_owner, 0x73) == 0);
    const uint32_t expected_commands[] = {0x11, 0x1a, 0x1f};
    const uint32_t expected_values[] = {0x51, 0x62, 0x73};
    for (size_t index = 0; index < 3; ++index) {
        assert(transcript.commands[index] == expected_commands[index]);
        assert(transcript.field_counts[index] == 2);
        assert(transcript.first_argument[index] == expected_values[index]);
    }
}

static void test_result_width_contracts(void) {
    RPCTranscript transcript = {0};
    transcript.responses[4] = 0x11223344;
    transcript.responses[0x12] = 0x22334455;
    transcript.responses[0x13] = 0x33445566;
    transcript.responses[0x17] = 0x44556677;
    transcript.responses[0x1c] = 0x123456ab;
    transcript.responses[0x1d] = 0x89abcdef;
    transcript.responses[0x20] = 0x55667788;
    USBRPCCommandOwner command_owner = owner(&transcript);
    uint32_t value32;
    uint16_t value16;
    uint8_t value8;
    assert(usb_rpc_build_configuration_descriptors(
        &command_owner, &value32) == 0 && value32 == 0x11223344);
    assert(usb_rpc_device_dispatch_slot0(
        &command_owner, &value32) == 0 && value32 == 0x22334455);
    assert(usb_rpc_device_dispatch_slot2(
        &command_owner, &value32) == 0 && value32 == 0x33445566);
    assert(usb_rpc_dfu_initialize(
        &command_owner, &value32) == 0 && value32 == 0x44556677);
    assert(usb_rpc_dfu_download_is_complete(
        &command_owner, &value8) == 0 && value8 == 0xab);
    assert(usb_rpc_dfu_download_event_handle(
        &command_owner, 0x3456, &value16) == 0 && value16 == 0xcdef);
    assert(transcript.first_argument[5] == 0x3456);
    assert(usb_rpc_dfu_download_payload_length(
        &command_owner, &value32) == 0 && value32 == 0x55667788);
}

static void test_multi_field_and_failure_contracts(void) {
    RPCTranscript transcript = {0};
    transcript.responses[3] = 0;
    transcript.responses[9] = 0x99;
    USBRPCCommandOwner command_owner = owner(&transcript);
    uint32_t first = 0x11223344;
    uint64_t second = UINT64_C(0xaabbccddeeff0011);
    USBRPCCommandArguments arguments = {
        .fields = {
            usb_rpc_command_payload(
                &first,
                sizeof(first),
                AVP_RUNTIME_RPC_NO_POINTER_TOKEN
            ),
            usb_rpc_command_payload(
                &second,
                sizeof(second),
                AVP_RUNTIME_RPC_NO_POINTER_TOKEN
            ),
        },
        .field_count = 2,
    };
    assert(usb_rpc_device_initialize(&command_owner, arguments) == 0);
    assert(transcript.commands[0] == 3);
    assert(transcript.field_counts[0] == 3);
    assert(transcript.first_argument[0] == first);
    uint32_t mapping_id;
    assert(usb_rpc_register_transport_mapping(
        &command_owner, arguments, &mapping_id) == 0);
    assert(mapping_id == 0x99);

    transcript.status = 7;
    assert(usb_rpc_device_deinitialize(&command_owner) ==
        USB_RPC_COMMAND_TRANSPORT_ERROR);
    assert(command_owner.last_command == 2);
    assert(command_owner.last_transport_status == 7);
    assert(command_owner.call_count == 3);

    transcript.status = 0;
    transcript.responses[2] = 1;
    assert(usb_rpc_device_deinitialize(&command_owner) ==
        USB_RPC_COMMAND_REMOTE_ERROR);
    assert(command_owner.last_response == 1);

    arguments.field_count = USB_RPC_COMMAND_MAX_ARGUMENTS + 1;
    assert(usb_rpc_device_initialize(&command_owner, arguments) ==
        USB_RPC_COMMAND_ARGUMENT_ERROR);
}

int main(void) {
    test_lifecycle_command_ids();
    test_single_byte_commands();
    test_result_width_contracts();
    test_multi_field_and_failure_contracts();
    puts("usb_rpc_command_owner_tests: ok");
    return 0;
}
