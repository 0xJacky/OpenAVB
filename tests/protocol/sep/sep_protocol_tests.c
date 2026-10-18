#include "avp/protocol/sep/sep_protocol.h"

#include <assert.h>
#include <stdint.h>


enum {
    TEST_RECOVERY_WINDOW_COUNT = 5,
};

typedef struct {
    bool succeeded;
    uint32_t attempted_windows;
    int32_t last_result;
} RecoveryObservation;

static int32_t response_wait_result(
    SEPCommandDescriptor descriptor,
    SEPMailboxMessage message
) {
    const SEPResponseDisposition disposition =
        sep_protocol_classify_response(descriptor, message);
    if (disposition == SEP_RESPONSE_MATCHED ||
        disposition == SEP_RESPONSE_COMPLETED) {
        return SEP_RESPONSE_WAIT_SUCCESS;
    }
    if (disposition == SEP_RESPONSE_INVALID) {
        return SEP_RESPONSE_WAIT_INVALID;
    }
    return SEP_RESPONSE_WAIT_TIMEOUT;
}

static RecoveryObservation simulate_recovery_window(
    SEPCommandDescriptor descriptor,
    SEPMailboxMessage message
) {
    RecoveryObservation observation = {
        .last_result = SEP_RESPONSE_WAIT_TIMEOUT,
    };
    for (;;) {
        observation.last_result = response_wait_result(descriptor, message);
        ++observation.attempted_windows;
        if (sep_response_wait_succeeded(observation.last_result)) {
            observation.succeeded = true;
            break;
        }
        if (!sep_response_wait_can_retry(observation.last_result)) {
            break;
        }
    }
    return observation;
}

int main(void) {
    uint8_t *previous_state = 0;
    uint32_t active_descriptors = 0;
    for (uint8_t index = 0; index < SEP_COMMAND_DESCRIPTOR_COUNT; ++index) {
        SEPCommandDescriptor descriptor;
        if (!sep_protocol_get_descriptor(index, &descriptor)) {
            continue;
        }
        assert(descriptor.state != 0);
        assert(descriptor.state == descriptor.state_lower_bound);
        assert(descriptor.state_upper_bound == descriptor.state + 1);
        assert(descriptor.state_pointer_context == 0);
        assert(sep_protocol_requires_response(descriptor));
        if (previous_state != 0) {
            assert(descriptor.state == previous_state + 1);
        }
        previous_state = descriptor.state;
        ++active_descriptors;
    }
    assert(active_descriptors == 29);

    SEPCommandDescriptor bootstrap;
    assert(sep_protocol_get_descriptor(SEP_COMMAND_BOOTSTRAP, &bootstrap));
    assert(bootstrap.command == 0x03);
    assert(bootstrap.response == 0x67);
    assert(bootstrap.correlation == 0x67);
    assert(bootstrap.timeout == 1000000);
    assert(sep_protocol_response_count(bootstrap) == 1);

    SEPCommandDescriptor configuration;
    assert(sep_protocol_get_descriptor(
        SEP_COMMAND_CONFIGURATION,
        &configuration
    ));
    assert(configuration.command == 0x04);
    assert(configuration.response == 0x68);
    assert(configuration.correlation == 0x6e);
    assert(configuration.response_index_stride == 1);
    assert(configuration.response_size == 20);
    assert(sep_protocol_response_count(configuration) == 5);

    SEPCommandDescriptor bulk;
    assert(sep_protocol_get_descriptor(26, &bulk));
    assert(bulk.command == 0x1a);
    assert(bulk.response == 0x7e);
    assert(bulk.response_index_stride == 4);
    assert(bulk.response_size == 0x40);
    assert(sep_protocol_response_count(bulk) == 16);
    assert(!sep_protocol_get_descriptor(7, &bulk));
    assert(!sep_protocol_get_descriptor(SEP_COMMAND_DESCRIPTOR_COUNT, &bulk));

    SEPCommandDescriptor finalize;
    assert(sep_protocol_get_descriptor(SEP_COMMAND_FINALIZE, &finalize));
    finalize.complete_after_send = 1;
    assert(!sep_protocol_requires_response(finalize));
    finalize.complete_after_send = 0;

    SEPCommandDescriptor preflight;
    assert(sep_protocol_get_descriptor(
        SEP_COMMAND_PREFLIGHT_PROBE,
        &preflight
    ));
    assert(preflight.command == 0x12);
    assert(preflight.response == 0x76);
    assert(preflight.timeout == 2000000);
    assert(preflight.requires_preflight_wait == 1);
    assert(sep_protocol_response_count(preflight) == 1);

    SEPMailboxMessage message = sep_protocol_request(bootstrap, 0, 0, 0);
    assert(message.low == 0x00000000000367ff);
    assert(message.high == 0);
    assert(sep_protocol_classify_response(
        bootstrap,
        (SEPMailboxMessage){0x000000a0006767ff, 0}
    ) == SEP_RESPONSE_MATCHED);
    assert(sep_protocol_payload(
        (SEPMailboxMessage){0x000000a0006767ff, 0}
    ) == 0xa0);
    assert(sep_protocol_opcode(
        (SEPMailboxMessage){0x000000a0006767ff, 0}
    ) == bootstrap.response);

    for (uint8_t index = 0;
         index < sep_protocol_response_count(configuration);
         ++index) {
        message = sep_protocol_request(configuration, index, 0, 0);
        assert(message.low == 0x0000000000046eff + ((uint64_t)index << 24));
        const SEPMailboxMessage response = {
            0xdeadbeef00686eff + ((uint64_t)index << 24),
            0,
        };
        assert(sep_protocol_classify_response(configuration, response) ==
            SEP_RESPONSE_MATCHED);
        assert(sep_protocol_payload(response) == 0xdeadbeef);
    }

    message = sep_protocol_request(finalize, 0, 0, 0);
    assert(message.low == 0x00000000000170ff);
    assert(sep_protocol_classify_response(
        finalize,
        (SEPMailboxMessage){0x00000000006570ff, 0}
    ) == SEP_RESPONSE_MATCHED);

    message = sep_protocol_request(preflight, 0, 0, 0);
    assert(message.low == 0x00000000001200ff);
    assert(sep_protocol_classify_response(
        preflight,
        (SEPMailboxMessage){0x00000000007600ff, 0}
    ) == SEP_RESPONSE_MATCHED);

    assert(sep_protocol_classify_response(
        configuration,
        (SEPMailboxMessage){0x0000000100d200ff, 0}
    ) == SEP_RESPONSE_ASYNCHRONOUS);
    assert(sep_protocol_classify_response(
        configuration,
        (SEPMailboxMessage){0x000000a0006767ff, 0}
    ) == SEP_RESPONSE_COMPLETED);
    const SEPMailboxMessage invalid_response = {
        0x0000000000ff00ff,
        0,
    };
    assert(sep_protocol_classify_response(
        configuration,
        invalid_response
    ) == SEP_RESPONSE_INVALID);

    const int32_t initial_invalid_result = response_wait_result(
        configuration,
        invalid_response
    );
    assert(initial_invalid_result == SEP_RESPONSE_WAIT_INVALID);
    assert(!sep_response_wait_succeeded(initial_invalid_result));
    assert(!sep_response_wait_can_retry(initial_invalid_result));
    assert(sep_response_wait_can_retry(SEP_RESPONSE_WAIT_TIMEOUT));

    uint32_t observed_invalid_recovery_windows = 0;
    for (uint32_t window = 0;
         window < TEST_RECOVERY_WINDOW_COUNT;
         ++window) {
        const RecoveryObservation recovery = simulate_recovery_window(
            configuration,
            invalid_response
        );
        assert(!recovery.succeeded);
        assert(recovery.attempted_windows == 1);
        assert(recovery.last_result == SEP_RESPONSE_WAIT_INVALID);
        ++observed_invalid_recovery_windows;
    }
    assert(observed_invalid_recovery_windows == TEST_RECOVERY_WINDOW_COUNT);
}
