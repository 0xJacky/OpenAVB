#include <assert.h>
#include <stdio.h>

#include "avp/protocol/sep/sep_protocol_helper_adapter.h"


typedef struct {
    uint8_t commands[16];
    uint32_t capacities[16];
    size_t execute_count;
    SEPMailboxMessage receive_messages[8];
    int32_t receive_statuses[8];
    size_t receive_count;
    size_t receive_index;
    uint32_t observed_timeout;
    bool bad_bootstrap_payload;
    bool empty_configuration;
    size_t bad_finalize_round;
} ProtocolTranscript;

static SEPMailboxMessage message_with_opcode_payload(
    uint8_t opcode,
    uint32_t payload
)
{
    return (SEPMailboxMessage){
        .low = UINT64_C(0xff) | (uint64_t)opcode << 16 |
            (uint64_t)payload << 32,
        .high = 0,
    };
}

static int execute_callback(
    void *context,
    uint8_t command_index,
    uint8_t initial_index,
    uint32_t payload,
    uint64_t extended_payload,
    uint32_t *responses,
    uint32_t response_capacity,
    SEPMailboxMessage *last_message
)
{
    ProtocolTranscript *transcript = context;
    size_t call = transcript->execute_count++;

    assert(call < 16);
    assert(initial_index == 0);
    assert(payload == 0);
    assert(extended_payload == 0);
    transcript->commands[call] = command_index;
    transcript->capacities[call] = response_capacity;
    if (command_index == SEP_COMMAND_BOOTSTRAP) {
        assert(responses != NULL && response_capacity == 1);
        responses[0] = transcript->bad_bootstrap_payload ? 0xa1 : 0xa0;
        *last_message = message_with_opcode_payload(0x67, responses[0]);
        return 0;
    }
    if (command_index == SEP_COMMAND_CONFIGURATION) {
        assert(responses != NULL && response_capacity == 5);
        for (uint32_t index = 0; index < 5; ++index) {
            responses[index] = transcript->empty_configuration ? 0 : index + 1;
        }
        *last_message = message_with_opcode_payload(0x68, responses[4]);
        return 0;
    }
    assert(command_index == SEP_COMMAND_FINALIZE);
    assert(responses == NULL && response_capacity == 0);
    *last_message = message_with_opcode_payload(
        transcript->bad_finalize_round == call + 1 ? 0x66 : 0x65,
        0
    );
    return 0;
}

static int32_t receive_callback(
    void *context,
    uint32_t timeout,
    SEPMailboxMessage *message
)
{
    ProtocolTranscript *transcript = context;
    size_t index = transcript->receive_index++;

    assert(index < transcript->receive_count);
    transcript->observed_timeout = timeout;
    if (transcript->receive_statuses[index] == SEP_RESPONSE_WAIT_SUCCESS) {
        *message = transcript->receive_messages[index];
    }
    return transcript->receive_statuses[index];
}

static SEPProtocolHelperSession session_for(ProtocolTranscript *transcript)
{
    return (SEPProtocolHelperSession){
        .execute = execute_callback,
        .receive = receive_callback,
        .context = transcript,
    };
}

static SEPBoundsSpan byte_span(void *pointer, size_t size)
{
    return (SEPBoundsSpan){
        .pointer = pointer,
        .lower_bound = pointer,
        .upper_bound = (const uint8_t *)pointer + size,
        .context = 0,
    };
}

static void test_message_build_and_descriptor_accessors(void)
{
    SEPMailboxMessage message;
    uint8_t opcode;

    assert(sep_build_protocol_message(
        SEP_COMMAND_BOOTSTRAP,
        7,
        0xaabbccdd,
        UINT64_C(0x1122334455667788),
        &message
    ) == SEP_PROTOCOL_HELPER_OK);
    assert((uint8_t)message.low == 0xff);
    assert((uint8_t)(message.low >> 8) == 0x67);
    assert((uint8_t)(message.low >> 16) == 0x03);
    assert((uint8_t)(message.low >> 24) == 7);
    assert((uint32_t)(message.low >> 32) == 0xaabbccdd);
    assert(message.high == UINT64_C(0x1122334455667788));

    assert(sep_protocol_expected_response_opcode(
        SEP_COMMAND_BOOTSTRAP,
        &opcode
    ) == SEP_PROTOCOL_HELPER_OK);
    assert(opcode == 0x67);
    assert(sep_protocol_expected_response_opcode(7, &opcode) ==
        SEP_PROTOCOL_HELPER_ERROR_COMMAND);
    assert(sep_build_protocol_message(7, 0, 0, 0, &message) ==
        SEP_PROTOCOL_HELPER_ERROR_COMMAND);
}

static void test_wait_response_state_machine(void)
{
    ProtocolTranscript transcript = {
        .receive_messages = {
            {UINT64_C(0x00c800ff), 0},
            {
                UINT64_C(0x12345678006700ff),
                0,
            },
        },
        .receive_statuses = {
            SEP_RESPONSE_WAIT_SUCCESS,
            SEP_RESPONSE_WAIT_SUCCESS,
        },
        .receive_count = 2,
    };
    SEPProtocolHelperSession session = session_for(&transcript);
    uint32_t payload;
    uint8_t opcode;

    assert(sep_wait_protocol_response(
        &session,
        1000000,
        SEP_COMMAND_BOOTSTRAP
    ) == SEP_RESPONSE_WAIT_SUCCESS);
    assert(transcript.observed_timeout == 1000000);
    assert(session.asynchronous_response_count == 1);
    assert(sep_protocol_last_opcode(&session, &opcode) == 0);
    assert(opcode == 0x67);
    assert(sep_protocol_last_payload(&session, &payload) == 0);
    assert(payload == 0x12345678);

    transcript = (ProtocolTranscript){
        .receive_messages = {{UINT64_C(0x00ff00ff), 0}},
        .receive_statuses = {SEP_RESPONSE_WAIT_SUCCESS},
        .receive_count = 1,
    };
    session = session_for(&transcript);
    assert(sep_wait_protocol_response(
        &session,
        1,
        SEP_COMMAND_BOOTSTRAP
    ) == SEP_RESPONSE_WAIT_INVALID);

    transcript = (ProtocolTranscript){
        .receive_statuses = {SEP_RESPONSE_WAIT_TIMEOUT},
        .receive_count = 1,
    };
    session = session_for(&transcript);
    assert(sep_wait_protocol_response(
        &session,
        1,
        SEP_COMMAND_BOOTSTRAP
    ) == SEP_RESPONSE_WAIT_TIMEOUT);

    transcript = (ProtocolTranscript){
        .receive_messages = {{UINT64_C(0x001000ff), 0}},
        .receive_statuses = {SEP_RESPONSE_WAIT_SUCCESS},
        .receive_count = 1,
    };
    session = session_for(&transcript);
    assert(sep_wait_protocol_response(
        &session,
        1,
        SEP_COMMAND_BOOTSTRAP
    ) == SEP_RESPONSE_WAIT_SUCCESS);
}

static void test_bootstrap_and_finalize(void)
{
    ProtocolTranscript transcript = {0};
    SEPProtocolHelperSession session = session_for(&transcript);
    SEPBootstrapResult result = {0};

    assert(sep_bootstrap_configuration_exchange(&session, &result) ==
        SEP_PROTOCOL_HELPER_OK);
    assert(transcript.execute_count == 2);
    assert(transcript.commands[0] == SEP_COMMAND_BOOTSTRAP);
    assert(transcript.capacities[0] == 1);
    assert(transcript.commands[1] == SEP_COMMAND_CONFIGURATION);
    assert(transcript.capacities[1] == 5);
    assert(result.bootstrap_payload == SEP_PROTOCOL_BOOTSTRAP_PAYLOAD);
    assert(result.configuration_words[4] == 5);

    transcript.execute_count = 0;
    assert(sep_finalize_eight_round_handshake(&session, &result) ==
        SEP_PROTOCOL_HELPER_OK);
    assert(transcript.execute_count == SEP_PROTOCOL_FINALIZE_ROUNDS);
    assert(result.finalize_rounds == SEP_PROTOCOL_FINALIZE_ROUNDS);
    assert(result.finalize_response_opcode == 0x65);
    for (size_t index = 0; index < transcript.execute_count; ++index) {
        assert(transcript.commands[index] == SEP_COMMAND_FINALIZE);
    }

    transcript = (ProtocolTranscript){.bad_bootstrap_payload = true};
    session = session_for(&transcript);
    assert(sep_bootstrap_configuration_exchange(&session, &result) ==
        SEP_PROTOCOL_HELPER_ERROR_RESPONSE);
    transcript = (ProtocolTranscript){.empty_configuration = true};
    session = session_for(&transcript);
    assert(sep_bootstrap_configuration_exchange(&session, &result) ==
        SEP_PROTOCOL_HELPER_ERROR_EMPTY_CONFIGURATION);
    transcript = (ProtocolTranscript){.bad_finalize_round = 3};
    session = session_for(&transcript);
    assert(sep_finalize_eight_round_handshake(&session, &result) ==
        SEP_PROTOCOL_HELPER_ERROR_RESPONSE);
    assert(result.finalize_rounds == 2);
}

static void test_bounds_and_output_helpers(void)
{
    uint8_t configuration[SEP_PROTOCOL_CONFIGURATION_SIZE] = {0};
    uint32_t output_word = 0;
    SEPBoundsSpan configuration_span = byte_span(
        configuration,
        sizeof(configuration)
    );
    SEPBoundsSpan output_span = byte_span(&output_word, sizeof(output_word));
    SEPResearchPatternOutput output;
    SEPProtocolArguments arguments = {
        .initial_index = 9,
        .payload = 0x1234,
        .extended_payload = 0x5678,
        .responses = &output_word,
        .response_capacity = 1,
    };

    assert(sep_validate_configuration_byte_span(
        configuration_span,
        SEP_PROTOCOL_CONFIGURATION_SIZE
    ) == SEP_PROTOCOL_HELPER_OK);
    configuration_span.upper_bound = configuration + sizeof(configuration) - 1;
    assert(sep_validate_configuration_byte_span(
        configuration_span,
        SEP_PROTOCOL_CONFIGURATION_SIZE
    ) == SEP_PROTOCOL_HELPER_ERROR_BOUNDS);

    assert(sep_read_pattern_outlined_save_output_span(
        &output,
        output_span
    ) == SEP_PROTOCOL_HELPER_OK);
    assert(output.scratch == 0);
    assert(sep_read_pattern_store_payload_success(&output, 0xfeedface) ==
        SEP_PROTOCOL_HELPER_OK);
    assert(output_word == 0xfeedface);

    sep_clear_protocol_arguments(&arguments);
    assert(arguments.initial_index == 0);
    assert(arguments.payload == 0);
    assert(arguments.extended_payload == 0);
    assert(arguments.responses == NULL);
    assert(arguments.response_capacity == 0);
}

int main(void)
{
    test_message_build_and_descriptor_accessors();
    test_wait_response_state_machine();
    test_bootstrap_and_finalize();
    test_bounds_and_output_helpers();
    puts("sep protocol helper adapter tests: PASS");
    return 0;
}
