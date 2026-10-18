#include "avp/protocol/sep/sep_protocol.h"


enum {
    SEP_MESSAGE_TAG = 0xff,
    SEP_ASYNC_OPCODE_MINIMUM = 0xc8,
    SEP_INVALID_OPCODE = 0xff,
    SEP_ACTIVE_COMMAND_COUNT = 29,
};

static uint8_t descriptor_states[SEP_ACTIVE_COMMAND_COUNT];

/* The four state words preserve Apple's bounds-safe one-byte pointer shape. */
static const SEPCommandDescriptor descriptors[SEP_COMMAND_DESCRIPTOR_COUNT] = {
    [0] = {0x01, 0x65, 0x70, 0, 1000000, 0, 0, &descriptor_states[0], &descriptor_states[0], &descriptor_states[1], 0, 0},
    [1] = {0x02, 0x66, 0x00, 0, 1000000, 0, 0, &descriptor_states[1], &descriptor_states[1], &descriptor_states[2], 0, 0},
    [2] = {0x03, 0x67, 0x67, 0, 1000000, 0, 0, &descriptor_states[2], &descriptor_states[2], &descriptor_states[3], 0, 0},
    [3] = {0x04, 0x68, 0x6e, 1, 1000000, 20, 0, &descriptor_states[3], &descriptor_states[3], &descriptor_states[4], 0, 0},
    [4] = {0x0a, 0x6e, 0x00, 0, 1000000, 0, 0, &descriptor_states[4], &descriptor_states[4], &descriptor_states[5], 0, 0},
    [5] = {0x0f, 0x73, 0x00, 0, 2000000, 0, 0, &descriptor_states[5], &descriptor_states[5], &descriptor_states[6], 0, 0},
    [6] = {0x10, 0x74, 0x00, 0, 2000000, 0, 0, &descriptor_states[6], &descriptor_states[6], &descriptor_states[7], 0, 0},
    [17] = {0x05, 0x69, 0x00, 0, 5000000, 0, 0, &descriptor_states[7], &descriptor_states[7], &descriptor_states[8], 0, 0},
    [18] = {0x06, 0x6a, 0x00, 0, 5000000, 0, 0, &descriptor_states[8], &descriptor_states[8], &descriptor_states[9], 0, 0},
    [19] = {0x14, 0x78, 0x00, 0, 2000000, 0, 0, &descriptor_states[9], &descriptor_states[9], &descriptor_states[10], 0, 0},
    [20] = {0x12, 0x76, 0x00, 0, 2000000, 0, 1, &descriptor_states[10], &descriptor_states[10], &descriptor_states[11], 0, 0},
    [21] = {0x13, 0x77, 0x00, 0, 2000000, 0, 0, &descriptor_states[11], &descriptor_states[11], &descriptor_states[12], 0, 0},
    [22] = {0x15, 0x79, 0x00, 0, 2000000, 0, 0, &descriptor_states[12], &descriptor_states[12], &descriptor_states[13], 0, 0},
    [23] = {0x20, 0x84, 0x00, 0, 10000000, 0, 1, &descriptor_states[13], &descriptor_states[13], &descriptor_states[14], 0, 0},
    [24] = {0x16, 0x7a, 0x00, 0, 2000000, 0, 0, &descriptor_states[14], &descriptor_states[14], &descriptor_states[15], 0, 0},
    [25] = {0x19, 0x7d, 0x00, 0, 10000000, 0, 1, &descriptor_states[15], &descriptor_states[15], &descriptor_states[16], 0, 0},
    [26] = {0x1a, 0x7e, 0x00, 4, 2000000, 0x40, 0, &descriptor_states[16], &descriptor_states[16], &descriptor_states[17], 0, 0},
    [27] = {0x1b, 0x7f, 0x00, 0, 2000000, 0, 0, &descriptor_states[17], &descriptor_states[17], &descriptor_states[18], 0, 0},
    [28] = {0x11, 0x75, 0x00, 0, 2000000, 0, 0, &descriptor_states[18], &descriptor_states[18], &descriptor_states[19], 0, 0},
    [29] = {0x17, 0x7b, 0x00, 0, 5000000, 0, 1, &descriptor_states[19], &descriptor_states[19], &descriptor_states[20], 0, 0},
    [30] = {0x18, 0x7c, 0x00, 0, 5000000, 0, 1, &descriptor_states[20], &descriptor_states[20], &descriptor_states[21], 0, 0},
    [31] = {0x1c, 0x80, 0x00, 0, 10000000, 0, 1, &descriptor_states[21], &descriptor_states[21], &descriptor_states[22], 0, 0},
    [32] = {0x1d, 0x81, 0x00, 4, 2000000, 0x90, 0, &descriptor_states[22], &descriptor_states[22], &descriptor_states[23], 0, 0},
    [33] = {0x1e, 0x82, 0x00, 0, 2000000, 0, 0, &descriptor_states[23], &descriptor_states[23], &descriptor_states[24], 0, 0},
    [34] = {0x1f, 0x83, 0x00, 4, 2000000, 0x20, 0, &descriptor_states[24], &descriptor_states[24], &descriptor_states[25], 0, 0},
    [35] = {0x23, 0x87, 0x00, 0, 2000000, 0, 0, &descriptor_states[25], &descriptor_states[25], &descriptor_states[26], 0, 0},
    [40] = {0x30, 0x94, 0x00, 0, 2000000, 0, 0, &descriptor_states[26], &descriptor_states[26], &descriptor_states[27], 0, 0},
    [41] = {0x31, 0x95, 0x00, 0, 2000000, 0, 0, &descriptor_states[27], &descriptor_states[27], &descriptor_states[28], 0, 0},
    [42] = {0x2e, 0x92, 0x00, 0, 2000000, 0, 0, &descriptor_states[28], &descriptor_states[28], &descriptor_states[29], 0, 0},
};

static uint8_t message_byte(SEPMailboxMessage message, uint32_t index) {
    return (uint8_t)(message.low >> (index * 8));
}

bool sep_protocol_get_descriptor(
    uint8_t command_index,
    SEPCommandDescriptor *descriptor
) {
    if (command_index >= SEP_COMMAND_DESCRIPTOR_COUNT ||
        descriptors[command_index].command == 0) {
        return false;
    }
    *descriptor = descriptors[command_index];
    return true;
}

uint32_t sep_protocol_response_count(SEPCommandDescriptor descriptor) {
    return descriptor.response_size == 0
        ? 1
        : descriptor.response_size / sizeof(uint32_t);
}

bool sep_protocol_requires_response(SEPCommandDescriptor descriptor) {
    return descriptor.complete_after_send == 0;
}

SEPMailboxMessage sep_protocol_request(
    SEPCommandDescriptor descriptor,
    uint8_t index,
    uint32_t payload,
    uint64_t extended_payload
) {
    const uint64_t low = SEP_MESSAGE_TAG |
        (uint64_t)descriptor.correlation << 8 |
        (uint64_t)descriptor.command << 16 |
        (uint64_t)index << 24 |
        (uint64_t)payload << 32;
    return (SEPMailboxMessage){low, extended_payload};
}

SEPResponseDisposition sep_protocol_classify_response(
    SEPCommandDescriptor descriptor,
    SEPMailboxMessage message
) {
    const uint8_t opcode = sep_protocol_opcode(message);
    if (opcode == descriptor.response) {
        return SEP_RESPONSE_MATCHED;
    }
    if (opcode < SEP_ASYNC_OPCODE_MINIMUM) {
        return SEP_RESPONSE_COMPLETED;
    }
    if (opcode == SEP_INVALID_OPCODE) {
        return SEP_RESPONSE_INVALID;
    }
    return SEP_RESPONSE_ASYNCHRONOUS;
}

uint32_t sep_protocol_payload(SEPMailboxMessage message) {
    return (uint32_t)(message.low >> 32);
}

uint8_t sep_protocol_opcode(SEPMailboxMessage message) {
    return message_byte(message, 2);
}
