#include "avp/protocol/sep/mailbox.h"
#include "avp/platform/mmio.h"
#include "avp/runtime/exception/pvpanic.h"
#include "avp/protocol/sep/sep.h"
#include "avp/protocol/sep/sep_protocol.h"
#include "avp/protocol/sep/sep_runtime_transport.h"
#include "avp/runtime/support/time.h"


enum {
    SEP_RUNTIME_TRANSPORT_PROVIDER_TYPE = 2,
    SEP_RUNTIME_TRANSPORT_TIMEOUT_US = 2000000,
};

bool sep_execute_runtime_transport_command(
    uint8_t command_index,
    uint8_t initial_index,
    uint32_t payload,
    uint64_t extended_payload,
    SEPMailboxMessage *response
) {
    SEPCommandDescriptor descriptor;
    if (!sep_protocol_get_descriptor(command_index, &descriptor)) {
        return false;
    }

    SEPRuntimeTransportView view = {};
    if (!sep_runtime_transport_prepare_view(
            runtime_profile_sep_transport(),
            SEP_RUNTIME_TRANSPORT_PROVIDER_TYPE,
            &view
        )) {
        return false;
    }
    const SEPRuntimeTransportLayout *layout =
        sep_runtime_transport_view_layout(&view);
    if (layout == 0) {
        pvpanic_trigger();
    }
    const uintptr_t register_base = (uintptr_t)view.register_base;

    const uint32_t initial_flags = mmio_read32(
        register_base + layout->receive_control_offset
    );
    if (!sep_runtime_transport_can_submit(initial_flags)) {
        pvpanic_trigger();
    }

    const SEPMailboxMessage request = sep_protocol_request(
        descriptor,
        initial_index,
        payload,
        extended_payload
    );
    avp_mailbox_store_pair(
        register_base + layout->send_slot_offset,
        request.low,
        request.high
    );

    const uint64_t start = avp_time_counter();
    const uint64_t duration = avp_time_microseconds_to_ticks(
        SEP_RUNTIME_TRANSPORT_TIMEOUT_US
    );
    for (;;) {
        const uint32_t flags = mmio_read32(
            register_base + layout->receive_control_offset
        );
        const SEPRuntimeTransportPoll poll = sep_runtime_transport_poll(
            flags,
            avp_time_has_elapsed(start, duration)
        );
        if (poll == SEP_RUNTIME_TRANSPORT_RESPONSE_READY) {
            break;
        }
        if (poll == SEP_RUNTIME_TRANSPORT_TIMED_OUT) {
            pvpanic_trigger();
        }
    }

    avp_mailbox_load_pair(
        register_base + layout->receive_slot_offset,
        &response->low,
        &response->high
    );
    return true;
}

bool __attribute__((used, retain)) sep_read_research_pattern_word(
    uint32_t *word
) {
    SEPCommandDescriptor descriptor;
    SEPMailboxMessage response;
    if (!sep_protocol_get_descriptor(
            SEP_COMMAND_RESEARCH_PATTERN,
            &descriptor
        ) || !sep_execute_runtime_transport_command(
            SEP_COMMAND_RESEARCH_PATTERN,
            0,
            0,
            0,
            &response
        ) || sep_protocol_opcode(response) != descriptor.response) {
        return false;
    }
    *word = sep_protocol_payload(response);
    return true;
}

uint64_t sep_read_research_pattern_u64(void) {
    uint32_t low = 0;
    uint32_t high = 0;
    if (!sep_read_research_pattern_word(&low) ||
        !sep_read_research_pattern_word(&high)) {
        pvpanic_trigger();
    }
    const uint64_t value = low | ((uint64_t)high << 32);
    if (value == 0) {
        pvpanic_trigger();
    }
    return value;
}
