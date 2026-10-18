#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "avp/protocol/sep/sep_transport_route_adapter.h"


typedef struct {
    SEPInterruptRouteVisit visits[SEP_INTERRUPT_ROUTE_COUNT];
    size_t count;
    size_t fail_at;
} VisitTranscript;

static int record_visit(
    void *context,
    uint16_t target,
    uint32_t route,
    uint32_t selector
)
{
    VisitTranscript *transcript = context;

    assert(transcript->count < SEP_INTERRUPT_ROUTE_COUNT);
    transcript->visits[transcript->count] = (SEPInterruptRouteVisit){
        .target = target,
        .route = route,
        .selector = selector,
    };
    ++transcript->count;
    return transcript->count == transcript->fail_at ? -1 : 0;
}

static SEPInterruptRouteSpan route_span(
    const SEPInterruptRouteRecord *records,
    size_t count
)
{
    return (SEPInterruptRouteSpan){
        .pointer = records,
        .lower_bound = records,
        .upper_bound = records + count,
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

static void test_layout_and_canonical_route_spans(void)
{
    SEPBoundsSpan layout_span = sep_runtime_transport_layout();
    SEPBoundsSpan view = sep_runtime_transport_layout_view();
    const SEPRuntimeTransportLayout *layout = layout_span.pointer;
    SEPInterruptRouteSpan empty = sep_interrupt_route_table_empty_span();
    SEPInterruptRouteSpan global = sep_interrupt_route_table_global_span();

    assert(layout_span.pointer == view.pointer);
    assert(layout_span.lower_bound == view.lower_bound);
    assert(layout_span.upper_bound == view.upper_bound);
    assert((uintptr_t)layout_span.upper_bound - (uintptr_t)layout_span.pointer ==
        sizeof(*layout));
    assert(layout->send_control_offset == 0x20);
    assert(layout->receive_control_offset == 0x1c);
    assert(layout->send_slot_offset == 0x200);
    assert(layout->receive_slot_offset == 0x100);
    assert(layout->initialization_control_offset == 0x18);
    assert(empty.pointer == empty.lower_bound);
    assert(empty.pointer == empty.upper_bound);
    assert(global.pointer == global.lower_bound);
    assert(global.upper_bound == global.pointer + 1);
}

static void test_forward_and_reverse_route_visits(void)
{
    const uint32_t packed_identifier = 0x12340002;
    const SEPInterruptRouteRecord records[] = {
        {
            .packed_identifier = 0x99990001,
            .routes = {1, 2, 3, 4, 5, 6, 7, 8},
        },
        {
            .packed_identifier = packed_identifier,
            .routes = {
                0x10,
                SEP_INTERRUPT_ROUTE_UNUSED,
                0x20,
                SEP_INTERRUPT_ROUTE_UNUSED,
                SEP_INTERRUPT_ROUTE_UNUSED,
                0x50,
                SEP_INTERRUPT_ROUTE_UNUSED,
                0x70,
            },
        },
    };
    SEPInterruptRouteSpan table = route_span(records, 2);
    VisitTranscript forward = {0};
    VisitTranscript reverse = {0};
    SEPInterruptRouteVisit direct = {0};

    assert(sep_interrupt_route_table_visit_forward(
        table,
        packed_identifier,
        record_visit,
        &forward
    ) == SEP_TRANSPORT_ROUTE_OK);
    assert(forward.count == 4);
    assert(forward.visits[0].target == 0x1234);
    assert(forward.visits[0].route == 0x10);
    assert(forward.visits[1].route == 0x20);
    assert(forward.visits[2].route == 0x50);
    assert(forward.visits[3].route == 0x70);
    for (size_t index = 0; index < forward.count; ++index) {
        assert(forward.visits[index].selector ==
            SEP_INTERRUPT_ROUTE_FORWARD_SELECTOR);
    }

    assert(sep_interrupt_route_table_visit_reverse(
        table,
        packed_identifier,
        record_visit,
        &reverse
    ) == SEP_TRANSPORT_ROUTE_OK);
    assert(reverse.count == 4);
    assert(reverse.visits[0].route == 0x70);
    assert(reverse.visits[1].route == 0x50);
    assert(reverse.visits[2].route == 0x20);
    assert(reverse.visits[3].route == 0x10);
    for (size_t index = 0; index < reverse.count; ++index) {
        assert(reverse.visits[index].selector ==
            SEP_INTERRUPT_ROUTE_REVERSE_SELECTOR);
    }

    assert(sep_interrupt_route_forward_visitor(&direct, 7, 8) == 0);
    assert(direct.target == 7 && direct.route == 8 && direct.selector == 15);
    assert(sep_interrupt_route_reverse_visitor(&direct, 9, 10) == 0);
    assert(direct.target == 9 && direct.route == 10 && direct.selector == 0);

    assert(sep_interrupt_route_table_visit(
        table,
        0xeeee0002,
        false,
        record_visit,
        &forward
    ) == SEP_TRANSPORT_ROUTE_ERROR_NOT_FOUND);
    forward = (VisitTranscript){.fail_at = 2};
    assert(sep_interrupt_route_table_visit(
        table,
        packed_identifier,
        false,
        record_visit,
        &forward
    ) == SEP_TRANSPORT_ROUTE_ERROR_CALLBACK);
    assert(forward.count == 2);
}

static void test_provider_lookup_and_properties(void)
{
    uint8_t channel_storage[0x90] = {0};
    SEPRuntimeRouteProvider providers[2] = {
        {
            .provider = {
                .type = 1,
                .register_base = (volatile uint8_t *)(uintptr_t)0x30240000,
            },
            .dma_window_size = 0x20,
            .interrupt_mask = 0,
            .interrupt_register_offset = 0x18,
            .channel_span = byte_span(channel_storage, sizeof(channel_storage)),
        },
        {
            .provider = {
                .type = 2,
                .register_base = (volatile uint8_t *)(uintptr_t)0x30250000,
            },
            .dma_window_size = 0x40,
            .interrupt_mask = 0x80,
            .interrupt_register_offset = 0x24,
            .channel_span = byte_span(channel_storage, sizeof(channel_storage)),
        },
    };
    const SEPRuntimeRouteProviderRegistry registry = {
        .providers = providers,
        .provider_count = 2,
    };
    SEPBoundsSpan provider_span;
    SEPBoundsSpan channel_span;
    uint16_t type;
    uint32_t register_offset;
    uint32_t interrupt_mask;

    assert(sep_runtime_transport_object_lookup(
        &registry,
        2,
        &provider_span
    ) == SEP_TRANSPORT_ROUTE_OK);
    assert(provider_span.pointer == &providers[1]);
    assert(provider_span.lower_bound == providers);
    assert(provider_span.upper_bound == providers + 2);
    assert(provider_span.context == 2);
    assert(sep_runtime_transport_object_require(
        &registry,
        1,
        &provider_span
    ) == SEP_TRANSPORT_ROUTE_OK);
    assert(provider_span.pointer == &providers[0]);
    assert(sep_runtime_transport_object_lookup(
        &registry,
        3,
        &provider_span
    ) == SEP_TRANSPORT_ROUTE_ERROR_NOT_FOUND);
    assert(provider_span.pointer == NULL);

    assert(sep_runtime_transport_dma_window_size_supported(&providers[0]));
    assert(sep_runtime_transport_dma_window_size_supported(&providers[1]));
    providers[1].dma_window_size = 0x30;
    assert(!sep_runtime_transport_dma_window_size_supported(&providers[1]));

    assert(sep_runtime_transport_outlined_prepare_register_address(
        &providers[0],
        &type,
        &register_offset
    ) == SEP_TRANSPORT_ROUTE_OK);
    assert(type == 1);
    assert(register_offset == 0x18);
    assert(sep_runtime_transport_interrupt_mask_value_outliner(
        &providers[0],
        &interrupt_mask
    ) == SEP_TRANSPORT_ROUTE_OK);
    assert(interrupt_mask == SEP_RUNTIME_DEFAULT_INTERRUPT_MASK);
    assert(sep_runtime_transport_interrupt_mask_value_outliner(
        &providers[1],
        &interrupt_mask
    ) == SEP_TRANSPORT_ROUTE_OK);
    assert(interrupt_mask == 0x80);
    channel_span = sep_runtime_transport_outlined_restore_channel_tuple_x20_x23(
        &providers[0]
    );
    assert(channel_span.pointer == channel_storage);
}

static void test_callback_argument_tuple(void)
{
    uint8_t channel[0x90] = {0};
    uint8_t buffer[16] = {0};
    SEPBoundsSpan channel_span = byte_span(channel, sizeof(channel));
    SEPBoundsSpan buffer_span = byte_span(buffer, sizeof(buffer));
    SEPBoundsSpan saved;
    SEPRuntimeChannelCallbackArguments arguments;

    assert(sep_runtime_transport_prepare_channel_callback_arguments(
        channel_span,
        buffer_span,
        sizeof(buffer),
        0xcafef00d,
        &arguments
    ) == SEP_TRANSPORT_ROUTE_OK);
    assert(arguments.channel_span.pointer == channel);
    assert(arguments.secondary_span.pointer == buffer);
    assert(arguments.byte_count == sizeof(buffer));
    assert(arguments.transfer_value == 0xcafef00d);

    saved = sep_runtime_transport_outlined_save_secondary_span_x22_x19(
        buffer_span
    );
    assert(memcmp(&saved, &buffer_span, sizeof(saved)) == 0);
    buffer_span.upper_bound = buffer + sizeof(buffer) - 1;
    assert(sep_runtime_transport_prepare_channel_callback_arguments(
        channel_span,
        buffer_span,
        sizeof(buffer),
        0,
        &arguments
    ) == SEP_TRANSPORT_ROUTE_ERROR_BOUNDS);
}

int main(void)
{
    test_layout_and_canonical_route_spans();
    test_forward_and_reverse_route_visits();
    test_provider_lookup_and_properties();
    test_callback_argument_tuple();
    puts("sep transport route adapter tests: PASS");
    return 0;
}
