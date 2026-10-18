#include "avp/protocol/sep/sep_runtime_transport.h"


bool sep_runtime_transport_can_submit(uint32_t flags) {
    return (flags & SEP_RUNTIME_TRANSPORT_RECEIVE_FULL) == 0;
}

SEPRuntimeTransportPoll sep_runtime_transport_poll(
    uint32_t flags,
    bool timeout_elapsed
) {
    if ((flags & SEP_RUNTIME_TRANSPORT_RECEIVE_EMPTY) == 0) {
        return SEP_RUNTIME_TRANSPORT_RESPONSE_READY;
    }
    return timeout_elapsed
        ? SEP_RUNTIME_TRANSPORT_TIMED_OUT
        : SEP_RUNTIME_TRANSPORT_WAITING;
}
