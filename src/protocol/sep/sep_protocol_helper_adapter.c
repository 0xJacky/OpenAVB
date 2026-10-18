#include "avp/protocol/sep/sep_protocol_helper_adapter.h"


static bool byte_span_contains(SEPBoundsSpan span, size_t size)
{
    uintptr_t pointer = (uintptr_t)span.pointer;
    uintptr_t lower = (uintptr_t)span.lower_bound;
    uintptr_t upper = (uintptr_t)span.upper_bound;

    return span.pointer != NULL && lower <= pointer && pointer < upper &&
        size <= upper - pointer;
}

int sep_validate_configuration_byte_span(
    SEPBoundsSpan configuration,
    size_t required_size
)
{
    return required_size == SEP_PROTOCOL_CONFIGURATION_SIZE &&
        byte_span_contains(configuration, required_size)
        ? SEP_PROTOCOL_HELPER_OK
        : SEP_PROTOCOL_HELPER_ERROR_BOUNDS;
}

void sep_clear_protocol_arguments(SEPProtocolArguments *arguments)
{
    arguments->initial_index = 0;
    arguments->payload = 0;
    arguments->extended_payload = 0;
    arguments->responses = NULL;
    arguments->response_capacity = 0;
}

int sep_read_pattern_outlined_save_output_span(
    SEPResearchPatternOutput *output,
    SEPBoundsSpan output_span
)
{
    if (output == NULL || !byte_span_contains(output_span, sizeof(uint32_t))) {
        return SEP_PROTOCOL_HELPER_ERROR_BOUNDS;
    }
    output->pointer = (uint32_t *)output_span.pointer;
    output->lower_bound = (uint32_t *)output_span.lower_bound;
    output->upper_bound = (uint32_t *)output_span.upper_bound;
    output->scratch = 0;
    return SEP_PROTOCOL_HELPER_OK;
}

int sep_read_pattern_store_payload_success(
    SEPResearchPatternOutput *output,
    uint32_t payload
)
{
    if (output == NULL || output->pointer == NULL ||
        output->lower_bound > output->pointer ||
        output->pointer >= output->upper_bound) {
        return SEP_PROTOCOL_HELPER_ERROR_BOUNDS;
    }
    *output->pointer = payload;
    return SEP_PROTOCOL_HELPER_OK;
}

int sep_build_protocol_message(
    uint8_t command_index,
    uint8_t index,
    uint32_t payload,
    uint64_t extended_payload,
    SEPMailboxMessage *message
)
{
    SEPCommandDescriptor descriptor;

    if (message == NULL) {
        return SEP_PROTOCOL_HELPER_ERROR_ARGUMENT;
    }
    if (!sep_protocol_get_descriptor(command_index, &descriptor)) {
        return SEP_PROTOCOL_HELPER_ERROR_COMMAND;
    }
    *message = sep_protocol_request(
        descriptor,
        index,
        payload,
        extended_payload
    );
    return SEP_PROTOCOL_HELPER_OK;
}

static void record_last_message(
    SEPProtocolHelperSession *session,
    SEPMailboxMessage message
)
{
    session->last_message = message;
    session->has_last_message = true;
}

int32_t sep_wait_protocol_response(
    SEPProtocolHelperSession *session,
    uint32_t timeout,
    uint8_t command_index
)
{
    SEPCommandDescriptor descriptor;

    if (session == NULL || session->receive == NULL ||
        !sep_protocol_get_descriptor(command_index, &descriptor)) {
        return SEP_RESPONSE_WAIT_INVALID;
    }
    for (;;) {
        SEPMailboxMessage message;
        int32_t receive_status = session->receive(
            session->context,
            timeout,
            &message
        );
        SEPResponseDisposition disposition;

        if (!sep_response_wait_succeeded(receive_status)) {
            return receive_status;
        }
        record_last_message(session, message);
        disposition = sep_protocol_classify_response(descriptor, message);
        if (disposition == SEP_RESPONSE_ASYNCHRONOUS) {
            ++session->asynchronous_response_count;
            continue;
        }
        if (disposition == SEP_RESPONSE_INVALID) {
            return SEP_RESPONSE_WAIT_INVALID;
        }
        return SEP_RESPONSE_WAIT_SUCCESS;
    }
}

int sep_protocol_last_payload(
    const SEPProtocolHelperSession *session,
    uint32_t *payload
)
{
    if (session == NULL || payload == NULL || !session->has_last_message) {
        return SEP_PROTOCOL_HELPER_ERROR_RESPONSE;
    }
    *payload = sep_protocol_payload(session->last_message);
    return SEP_PROTOCOL_HELPER_OK;
}

int sep_protocol_last_opcode(
    const SEPProtocolHelperSession *session,
    uint8_t *opcode
)
{
    if (session == NULL || opcode == NULL || !session->has_last_message) {
        return SEP_PROTOCOL_HELPER_ERROR_RESPONSE;
    }
    *opcode = sep_protocol_opcode(session->last_message);
    return SEP_PROTOCOL_HELPER_OK;
}

int sep_protocol_expected_response_opcode(
    uint8_t command_index,
    uint8_t *opcode
)
{
    SEPCommandDescriptor descriptor;

    if (opcode == NULL) {
        return SEP_PROTOCOL_HELPER_ERROR_ARGUMENT;
    }
    if (!sep_protocol_get_descriptor(command_index, &descriptor)) {
        return SEP_PROTOCOL_HELPER_ERROR_COMMAND;
    }
    *opcode = descriptor.response;
    return SEP_PROTOCOL_HELPER_OK;
}

int sep_bootstrap_configuration_exchange(
    SEPProtocolHelperSession *session,
    SEPBootstrapResult *result
)
{
    const uint32_t configuration_word_count =
        sizeof(result->configuration_words) / sizeof(result->configuration_words[0]);
    uint32_t configuration_or = 0;
    uint32_t index;
    SEPMailboxMessage last_message;

    if (session == NULL || session->execute == NULL || result == NULL) {
        return SEP_PROTOCOL_HELPER_ERROR_ARGUMENT;
    }
    result->bootstrap_payload = 0;
    for (index = 0; index < configuration_word_count; ++index) {
        result->configuration_words[index] = 0;
    }
    if (session->execute(
            session->context,
            SEP_COMMAND_BOOTSTRAP,
            0,
            0,
            0,
            &result->bootstrap_payload,
            1,
            &last_message
        ) != 0 || result->bootstrap_payload != SEP_PROTOCOL_BOOTSTRAP_PAYLOAD) {
        return SEP_PROTOCOL_HELPER_ERROR_RESPONSE;
    }
    record_last_message(session, last_message);
    if (session->execute(
            session->context,
            SEP_COMMAND_CONFIGURATION,
            0,
            0,
            0,
            result->configuration_words,
            configuration_word_count,
            &last_message
        ) != 0) {
        return SEP_PROTOCOL_HELPER_ERROR_COMMAND;
    }
    record_last_message(session, last_message);
    for (index = 0; index < configuration_word_count; ++index) {
        configuration_or |= result->configuration_words[index];
    }
    return configuration_or == 0
        ? SEP_PROTOCOL_HELPER_ERROR_EMPTY_CONFIGURATION
        : SEP_PROTOCOL_HELPER_OK;
}

int sep_finalize_eight_round_handshake(
    SEPProtocolHelperSession *session,
    SEPBootstrapResult *result
)
{
    uint8_t expected_opcode;
    uint32_t round;

    if (session == NULL || session->execute == NULL || result == NULL) {
        return SEP_PROTOCOL_HELPER_ERROR_ARGUMENT;
    }
    if (sep_protocol_expected_response_opcode(
            SEP_COMMAND_FINALIZE,
            &expected_opcode
        ) != SEP_PROTOCOL_HELPER_OK) {
        return SEP_PROTOCOL_HELPER_ERROR_COMMAND;
    }
    result->finalize_rounds = 0;
    for (round = 0; round < SEP_PROTOCOL_FINALIZE_ROUNDS; ++round) {
        SEPMailboxMessage last_message;

        if (session->execute(
                session->context,
                SEP_COMMAND_FINALIZE,
                0,
                0,
                0,
                NULL,
                0,
                &last_message
            ) != 0 || sep_protocol_opcode(last_message) != expected_opcode) {
            return SEP_PROTOCOL_HELPER_ERROR_RESPONSE;
        }
        record_last_message(session, last_message);
        result->finalize_rounds = round + 1;
    }
    result->finalize_response_opcode = expected_opcode;
    return SEP_PROTOCOL_HELPER_OK;
}
