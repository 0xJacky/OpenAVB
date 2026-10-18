#pragma once

#include "avp/protocol/sep/sep.h"

#include <stdbool.h>
#include <stdint.h>


typedef struct {
    uint8_t command;
    uint8_t response;
    uint8_t correlation;
    uint8_t response_index_stride;
    uint32_t timeout;
    uint32_t response_size;
    uint32_t requires_preflight_wait;
    uint8_t *state;
    uint8_t *state_lower_bound;
    uint8_t *state_upper_bound;
    uintptr_t state_pointer_context;
    uint64_t complete_after_send;
} SEPCommandDescriptor;

_Static_assert(sizeof(SEPCommandDescriptor) == 56, "SEP descriptor size");

typedef enum {
    SEP_RESPONSE_MATCHED,
    SEP_RESPONSE_COMPLETED,
    SEP_RESPONSE_ASYNCHRONOUS,
    SEP_RESPONSE_INVALID,
} SEPResponseDisposition;

enum {
    SEP_RESPONSE_WAIT_TIMEOUT = -2,
    SEP_RESPONSE_WAIT_SUCCESS = 0,
    SEP_RESPONSE_WAIT_INVALID = 0xff,
};

/**
 * Protocol failures use positive diagnostic values, so success requires the
 * exact zero returned by a matched or synchronously completed response.
 */
static inline bool sep_response_wait_succeeded(int32_t result) {
    return result == SEP_RESPONSE_WAIT_SUCCESS;
}

static inline bool sep_response_wait_can_retry(int32_t result) {
    return result == SEP_RESPONSE_WAIT_TIMEOUT;
}

enum {
    SEP_COMMAND_DESCRIPTOR_COUNT = 43,
    SEP_COMMAND_FINALIZE = 0,
    SEP_COMMAND_BOOTSTRAP = 2,
    SEP_COMMAND_CONFIGURATION = 3,
    SEP_COMMAND_RESEARCH_PATTERN = 6,
    SEP_COMMAND_PREFLIGHT_PROBE = 20,
};

bool sep_protocol_get_descriptor(
    uint8_t command_index,
    SEPCommandDescriptor *descriptor
);
uint32_t sep_protocol_response_count(SEPCommandDescriptor descriptor);
bool sep_protocol_requires_response(SEPCommandDescriptor descriptor);

SEPMailboxMessage sep_protocol_request(
    SEPCommandDescriptor descriptor,
    uint8_t index,
    uint32_t payload,
    uint64_t extended_payload
);

SEPResponseDisposition sep_protocol_classify_response(
    SEPCommandDescriptor descriptor,
    SEPMailboxMessage message
);

uint32_t sep_protocol_payload(SEPMailboxMessage message);
uint8_t sep_protocol_opcode(SEPMailboxMessage message);
