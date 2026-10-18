#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "avp/protocol/sep/sep_protocol.h"
#include "avp/protocol/sep/sep_runtime_transport.h"

#define SEP_PROTOCOL_HELPER_PUBLIC __attribute__((used, retain))

enum {
    SEP_PROTOCOL_CONFIGURATION_SIZE = 20,
    SEP_PROTOCOL_FINALIZE_ROUNDS = 8,
    SEP_PROTOCOL_BOOTSTRAP_PAYLOAD = 0xa0,
};

typedef enum {
    SEP_PROTOCOL_HELPER_OK = 0,
    SEP_PROTOCOL_HELPER_ERROR_ARGUMENT = -1,
    SEP_PROTOCOL_HELPER_ERROR_BOUNDS = -2,
    SEP_PROTOCOL_HELPER_ERROR_COMMAND = -3,
    SEP_PROTOCOL_HELPER_ERROR_RESPONSE = -4,
    SEP_PROTOCOL_HELPER_ERROR_EMPTY_CONFIGURATION = -5,
} SEPProtocolHelperStatus;

typedef struct SEPProtocolHelperSession SEPProtocolHelperSession;

typedef int (*SEPProtocolExecuteCallback)(
    void *context,
    uint8_t command_index,
    uint8_t initial_index,
    uint32_t payload,
    uint64_t extended_payload,
    uint32_t *responses,
    uint32_t response_capacity,
    SEPMailboxMessage *last_message
);
typedef int32_t (*SEPProtocolReceiveCallback)(
    void *context,
    uint32_t timeout,
    SEPMailboxMessage *message
);

struct SEPProtocolHelperSession {
    SEPProtocolExecuteCallback execute;
    SEPProtocolReceiveCallback receive;
    void *context;
    SEPMailboxMessage last_message;
    bool has_last_message;
    uint32_t asynchronous_response_count;
};

typedef struct {
    uint8_t initial_index;
    uint32_t payload;
    uint64_t extended_payload;
    uint32_t *responses;
    uint32_t response_capacity;
} SEPProtocolArguments;

typedef struct {
    uint32_t *pointer;
    uint32_t *lower_bound;
    uint32_t *upper_bound;
    uint32_t scratch;
} SEPResearchPatternOutput;

SEP_PROTOCOL_HELPER_PUBLIC int sep_bootstrap_configuration_exchange(
    SEPProtocolHelperSession *session,
    SEPBootstrapResult *result
);
SEP_PROTOCOL_HELPER_PUBLIC int sep_finalize_eight_round_handshake(
    SEPProtocolHelperSession *session,
    SEPBootstrapResult *result
);
SEP_PROTOCOL_HELPER_PUBLIC int sep_validate_configuration_byte_span(
    SEPBoundsSpan configuration,
    size_t required_size
);
SEP_PROTOCOL_HELPER_PUBLIC void sep_clear_protocol_arguments(
    SEPProtocolArguments *arguments
);
SEP_PROTOCOL_HELPER_PUBLIC int sep_read_pattern_outlined_save_output_span(
    SEPResearchPatternOutput *output,
    SEPBoundsSpan output_span
);
SEP_PROTOCOL_HELPER_PUBLIC int sep_read_pattern_store_payload_success(
    SEPResearchPatternOutput *output,
    uint32_t payload
);
SEP_PROTOCOL_HELPER_PUBLIC int sep_build_protocol_message(
    uint8_t command_index,
    uint8_t index,
    uint32_t payload,
    uint64_t extended_payload,
    SEPMailboxMessage *message
);
SEP_PROTOCOL_HELPER_PUBLIC int32_t sep_wait_protocol_response(
    SEPProtocolHelperSession *session,
    uint32_t timeout,
    uint8_t command_index
);
SEP_PROTOCOL_HELPER_PUBLIC int sep_protocol_last_payload(
    const SEPProtocolHelperSession *session,
    uint32_t *payload
);
SEP_PROTOCOL_HELPER_PUBLIC int sep_protocol_last_opcode(
    const SEPProtocolHelperSession *session,
    uint8_t *opcode
);
SEP_PROTOCOL_HELPER_PUBLIC int sep_protocol_expected_response_opcode(
    uint8_t command_index,
    uint8_t *opcode
);
