#pragma once

#include <stdbool.h>
#include <stdint.h>


typedef struct {
    uint64_t low;
    uint64_t high;
} SEPMailboxMessage;

typedef struct {
    uint32_t bootstrap_payload;
    uint32_t configuration_words[5];
    uint32_t finalize_rounds;
    uint8_t finalize_response_opcode;
} SEPBootstrapResult;

void sep_initialize(void);
void sep_send(SEPMailboxMessage message);
bool sep_execute_protocol_command(
    uint8_t command_index,
    uint8_t initial_index,
    uint32_t payload,
    uint64_t extended_payload,
    uint32_t *responses,
    uint32_t response_capacity
);
bool sep_execute_runtime_transport_command(
    uint8_t command_index,
    uint8_t initial_index,
    uint32_t payload,
    uint64_t extended_payload,
    SEPMailboxMessage *response
);
bool sep_read_research_pattern_word(uint32_t *word);
uint64_t sep_read_research_pattern_u64(void);
SEPBootstrapResult sep_bootstrap(void);
void sep_finalize_bootstrap(SEPBootstrapResult *result);
SEPBootstrapResult sep_bootstrap_with_finalize(void);
