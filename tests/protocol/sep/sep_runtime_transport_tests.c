#include "avp/protocol/sep/sep_runtime_transport.h"

#include <assert.h>


int main(void) {
    SEPRuntimeTransportView view = {};
    const SEPRuntimeTransportProfile *profile =
        runtime_profile_sep_transport();
    assert(sep_runtime_transport_prepare_view(profile, 2, &view));
    assert(!sep_runtime_transport_prepare_view(profile, 1, &view));
    assert(sep_runtime_transport_view_is_valid(&view));
    const SEPRuntimeTransportLayout *layout =
        sep_runtime_transport_view_layout(&view);
    assert(layout != 0);
    assert((uintptr_t)view.register_base == 0x30250000);
    assert(layout->send_slot_offset == 0x200);
    assert(layout->receive_slot_offset == 0x100);

    SEPRuntimeTransportView invalid = view;
    invalid.layout.upper_bound = invalid.layout.lower_bound;
    assert(!sep_runtime_transport_view_is_valid(&invalid));
    assert(sep_runtime_transport_view_layout(&invalid) == 0);

    const SEPRuntimeTransportLayout alternate_layout = {
        .send_control_offset = 0x28,
        .receive_control_offset = 0x24,
        .send_slot_offset = 0x300,
        .receive_slot_offset = 0x180,
    };
    const SEPRuntimeTransportProvider alternate_provider = {
        .type = 0x10002,
        .register_base = (volatile uint8_t *)0x12345000,
        .auxiliary = 0xabcdef,
    };
    const SEPRuntimeTransportProfile alternate_profile = {
        .providers = &alternate_provider,
        .provider_count = 1,
        .layout = {
            .pointer = &alternate_layout,
            .lower_bound = &alternate_layout,
            .upper_bound = (const uint8_t *)&alternate_layout +
                sizeof(alternate_layout),
        },
    };
    assert(sep_runtime_transport_prepare_view(
        &alternate_profile,
        2,
        &view
    ));
    assert((uintptr_t)view.register_base == 0x12345000);
    assert(view.provider_auxiliary == 0xabcdef);
    layout = sep_runtime_transport_view_layout(&view);
    assert(layout != 0);
    assert(layout->send_slot_offset == 0x300);
    assert(layout->receive_slot_offset == 0x180);

    assert(sep_runtime_transport_can_submit(0));
    assert(sep_runtime_transport_can_submit(
        SEP_RUNTIME_TRANSPORT_RECEIVE_EMPTY
    ));
    assert(!sep_runtime_transport_can_submit(
        SEP_RUNTIME_TRANSPORT_RECEIVE_FULL
    ));

    assert(sep_runtime_transport_poll(0, false) ==
        SEP_RUNTIME_TRANSPORT_RESPONSE_READY);
    assert(sep_runtime_transport_poll(
        SEP_RUNTIME_TRANSPORT_RECEIVE_EMPTY,
        false
    ) == SEP_RUNTIME_TRANSPORT_WAITING);
    assert(sep_runtime_transport_poll(
        SEP_RUNTIME_TRANSPORT_RECEIVE_EMPTY,
        true
    ) == SEP_RUNTIME_TRANSPORT_TIMED_OUT);
}
