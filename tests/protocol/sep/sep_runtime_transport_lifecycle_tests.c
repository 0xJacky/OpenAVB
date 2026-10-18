#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "avp/protocol/sep/sep_runtime_transport_lifecycle.h"


typedef enum {
    EVENT_ROUTE,
    EVENT_READ,
    EVENT_WRITE,
    EVENT_CHANNEL,
    EVENT_LIFECYCLE,
    EVENT_SEND,
    EVENT_RECEIVE,
} EventKind;

typedef struct {
    EventKind kind;
    uintptr_t address;
    uint64_t value;
    const void *pointer;
    size_t size;
} Event;

typedef struct {
    Event events[32];
    size_t count;
    uintptr_t primary_base;
    uintptr_t secondary_base;
    uint32_t gate_value;
    uint32_t primary_interrupt_value;
    uint32_t secondary_interrupt_value;
} Transcript;

static void record(
    Transcript *transcript,
    EventKind kind,
    uintptr_t address,
    uint64_t value,
    const void *pointer,
    size_t size
)
{
    assert(transcript->count < sizeof(transcript->events) / sizeof(transcript->events[0]));
    transcript->events[transcript->count++] = (Event){
        .kind = kind,
        .address = address,
        .value = value,
        .pointer = pointer,
        .size = size,
    };
}

static int route_hook(void *context, bool install)
{
    Transcript *transcript = context;
    record(transcript, EVENT_ROUTE, 0, install, NULL, 0);
    return 0;
}

static uint32_t read32_hook(void *context, uintptr_t address)
{
    Transcript *transcript = context;
    uint32_t value;

    if (address == transcript->primary_base + 0x20) {
        value = transcript->gate_value;
    } else if (address == transcript->primary_base + 0x18) {
        value = transcript->primary_interrupt_value;
    } else {
        assert(address == transcript->secondary_base + 0x18);
        value = transcript->secondary_interrupt_value;
    }
    record(transcript, EVENT_READ, address, value, NULL, 0);
    return value;
}

static int write32_hook(void *context, uintptr_t address, uint32_t value)
{
    Transcript *transcript = context;
    record(transcript, EVENT_WRITE, address, value, NULL, 0);
    return 0;
}

static int channel_initialize_hook(
    void *context,
    SEPRuntimeTransportChannel *channel
)
{
    Transcript *transcript = context;
    assert(channel->register_base == transcript->primary_base);
    assert(channel->initialized);
    record(transcript, EVENT_CHANNEL, channel->register_base, 0, channel, 0);
    return 0;
}

static int lifecycle_hook(
    void *context,
    SEPRuntimeTransportLifecycle *transport,
    bool active
)
{
    Transcript *transcript = context;
    record(transcript, EVENT_LIFECYCLE, 0, active, transport, 0);
    return 0;
}

static int send_hook(
    void *context,
    SEPRuntimeTransportChannel *channel,
    const void *buffer,
    size_t byte_count,
    uint32_t flags
)
{
    Transcript *transcript = context;
    assert(channel->initialized);
    record(transcript, EVENT_SEND, 0, flags, buffer, byte_count);
    return 0;
}

static int receive_hook(
    void *context,
    SEPRuntimeTransportChannel *channel,
    void *buffer,
    size_t byte_count,
    uintptr_t transfer_context
)
{
    Transcript *transcript = context;
    assert(channel->initialized);
    record(
        transcript,
        EVENT_RECEIVE,
        transfer_context,
        0,
        buffer,
        byte_count
    );
    return 0;
}

static SEPRuntimeTransportLifecycleSpan full_span(
    SEPRuntimeTransportLifecycle *transport
)
{
    return (SEPRuntimeTransportLifecycleSpan){
        .pointer = transport,
        .lower_bound = transport,
        .upper_bound = (const unsigned char *)transport + sizeof(*transport),
    };
}

static SEPRuntimeTransportLifecycleConfiguration configuration(
    const SEPRuntimeTransportLayout *layout
)
{
    return (SEPRuntimeTransportLifecycleConfiguration){
        .version = SEP_RUNTIME_TRANSPORT_VERSION_2_0_0,
        .primary_register_base = 0x30250000,
        .secondary_register_base = 0x30260000,
        .dma_ipa_base = 0x81000000,
        .dma_window_size = 0x40,
        .layout = layout,
        .provider_context = 0x12340000,
        .channel_configuration = {0x11223344, 0x55667788},
        .interrupt_mask = 0,
        .interrupt_register_offset = 0x18,
        .gate_register_offset = 0x20,
        .counter_low_register_offset = 0x24,
        .counter_high_register_offset = 0x28,
        .virtual_counter = UINT64_C(0x1122334455667788),
        .initialize_channel = true,
        .invoke_lifecycle = true,
    };
}

static SEPRuntimeTransportLifecycle transport_with_operations(Transcript *transcript)
{
    return (SEPRuntimeTransportLifecycle){
        .operations = {
            .channel_initialize = channel_initialize_hook,
            .lifecycle = lifecycle_hook,
            .route = route_hook,
            .read32 = read32_hook,
            .write32 = write32_hook,
            .send = send_hook,
            .receive = receive_hook,
        },
        .operation_context = transcript,
    };
}

static void test_initialize_transcript_and_accessors(void)
{
    const SEPRuntimeTransportLayout layout = {
        .send_control_offset = 0x20,
        .receive_control_offset = 0x1c,
        .send_slot_offset = 0x200,
        .receive_slot_offset = 0x100,
        .initialization_control_offset = 0x18,
    };
    Transcript transcript = {
        .primary_base = 0x30250000,
        .secondary_base = 0x30260000,
        .gate_value = 0xa7,
        .primary_interrupt_value = 0x81,
        .secondary_interrupt_value = 0x42,
    };
    SEPRuntimeTransportLifecycle transport = transport_with_operations(&transcript);
    SEPRuntimeTransportLifecycleSpan span = full_span(&transport);
    SEPRuntimeTransportLifecycleConfiguration config = configuration(&layout);
    uintptr_t address;
    uintptr_t dma_slot;
    uint32_t mask;

    assert(sep_runtime_transport_initialize(span, &config) == 0);
    assert(transport.active);
    assert(transport.channel.layout == &layout);
    assert(transport.channel.provider_context == 0x12340000);
    assert(transport.channel.configuration[0] == 0x11223344);
    assert(transport.channel.configuration[1] == 0x55667788);
    assert(transcript.count == 12);
    assert(transcript.events[0].kind == EVENT_ROUTE);
    assert(transcript.events[0].value == 1);
    assert(transcript.events[1].kind == EVENT_READ);
    assert(transcript.events[1].address == 0x30250020);
    assert(transcript.events[2].value == (0xa7u & ~2u));
    assert(transcript.events[3].address == 0x30250024);
    assert(transcript.events[3].value == 0x55667788);
    assert(transcript.events[4].address == 0x30250028);
    assert(transcript.events[4].value == 0x11223344);
    assert(transcript.events[5].value == (0xa7u | 2u));
    assert(transcript.events[6].kind == EVENT_CHANNEL);
    assert(transcript.events[7].kind == EVENT_READ);
    assert(transcript.events[8].value == 0x91);
    assert(transcript.events[9].kind == EVENT_READ);
    assert(transcript.events[10].value == 0x52);
    assert(transcript.events[11].kind == EVENT_LIFECYCLE);
    assert(transcript.events[11].value == 1);

    assert(sep_runtime_transport_dma_ipa_slot(span, &dma_slot) == 0);
    assert(dma_slot == 0x81050000);
    assert(sep_runtime_transport_interrupt_mask_value(span, &mask) == 0);
    assert(mask == SEP_RUNTIME_TRANSPORT_DEFAULT_INTERRUPT_MASK);
    transport.interrupt_mask = 0x200;
    assert(sep_runtime_transport_interrupt_mask_value(span, &mask) == 0);
    assert(mask == 0x200);
    assert(sep_runtime_transport_primary_register_address(span, 0x44, &address) == 0);
    assert(address == 0x30250044);
    assert(sep_runtime_transport_secondary_register_address(span, 0x48, &address) == 0);
    assert(address == 0x30260048);
}

static void test_send_receive_and_teardown_transcript(void)
{
    const SEPRuntimeTransportLayout layout = {0};
    Transcript transcript = {
        .primary_base = 0x30250000,
        .secondary_base = 0x30260000,
        .gate_value = 2,
        .primary_interrupt_value = 0x31,
        .secondary_interrupt_value = 0x70,
    };
    SEPRuntimeTransportLifecycle transport = transport_with_operations(&transcript);
    SEPRuntimeTransportLifecycleSpan span = full_span(&transport);
    SEPRuntimeTransportLifecycleConfiguration config = configuration(&layout);
    uint8_t command[16] = {0x53, 0x45, 0x50};
    uint8_t response[16] = {0};
    SEPBoundsSpan command_span = {
        .pointer = command,
        .lower_bound = command,
        .upper_bound = command + sizeof(command),
    };
    SEPBoundsSpan response_span = {
        .pointer = response,
        .lower_bound = response,
        .upper_bound = response + sizeof(response),
    };

    config.interrupt_mask = 0x20;
    assert(sep_runtime_transport_initialize(span, &config) == 0);
    transcript.count = 0;

    assert(sep_runtime_transport_send(span, command_span, sizeof(command), 0xa5) == 0);
    assert(transcript.events[0].kind == EVENT_SEND);
    assert(transcript.events[0].pointer == command);
    assert(transcript.events[0].size == sizeof(command));
    assert(transcript.events[0].value == 0xa5);
    assert(sep_runtime_transport_receive(
        span,
        response_span,
        sizeof(response),
        0xcafef00d
    ) == 0);
    assert(transcript.events[1].kind == EVENT_RECEIVE);
    assert(transcript.events[1].pointer == response);
    assert(transcript.events[1].size == sizeof(response));
    assert(transcript.events[1].address == 0xcafef00d);

    assert(sep_runtime_transport_teardown(span) == 0);
    assert(transcript.events[2].kind == EVENT_LIFECYCLE);
    assert(transcript.events[2].value == 0);
    assert(transcript.events[3].kind == EVENT_READ);
    assert(transcript.events[4].kind == EVENT_WRITE);
    assert(transcript.events[4].value == 0x11);
    assert(transcript.events[5].kind == EVENT_READ);
    assert(transcript.events[6].kind == EVENT_WRITE);
    assert(transcript.events[6].value == 0x50);
    assert(transcript.events[7].kind == EVENT_ROUTE);
    assert(transcript.events[7].value == 0);
    assert(!transport.active);
    assert(!transport.channel.initialized);
    assert(sep_runtime_transport_send(span, command_span, sizeof(command), 0) ==
        SEP_RUNTIME_LIFECYCLE_ERROR_STATE);

    transport.active = true;
    transport.channel.initialized = true;
    command_span.upper_bound = command + sizeof(command) - 1;
    assert(sep_runtime_transport_send(span, command_span, sizeof(command), 0) ==
        SEP_RUNTIME_LIFECYCLE_ERROR_BOUNDS);
}

static void test_validation_contracts(void)
{
    const SEPRuntimeTransportLayout layout = {0};
    Transcript transcript = {
        .primary_base = 0x30250000,
        .secondary_base = 0x30260000,
    };
    SEPRuntimeTransportLifecycle transport = transport_with_operations(&transcript);
    SEPRuntimeTransportLifecycleSpan span = full_span(&transport);
    SEPRuntimeTransportLifecycleConfiguration config = configuration(&layout);
    SEPRuntimeTransportLifecycleSpan truncated = span;
    uint32_t mask;

    truncated.upper_bound = (const unsigned char *)&transport + sizeof(transport) - 1;
    assert(sep_runtime_transport_interrupt_mask_value(truncated, &mask) ==
        SEP_RUNTIME_LIFECYCLE_ERROR_BOUNDS);
    assert(sep_runtime_transport_interrupt_mask_bounds_panic(truncated) ==
        SEP_RUNTIME_LIFECYCLE_ERROR_BOUNDS);

    config.version = 0x0300;
    assert(sep_runtime_transport_initialize(span, &config) ==
        SEP_RUNTIME_LIFECYCLE_ERROR_VERSION);
    assert(transport.last_error == SEP_RUNTIME_LIFECYCLE_ERROR_VERSION);
    config.version = SEP_RUNTIME_TRANSPORT_VERSION_1_0_1;
    config.dma_window_size = 0x30;
    assert(sep_runtime_transport_initialize(span, &config) ==
        SEP_RUNTIME_LIFECYCLE_ERROR_DMA_WINDOW);
    config.dma_window_size = 0x20;
    config.dma_ipa_base = 0x81000001;
    assert(sep_runtime_transport_initialize(span, &config) ==
        SEP_RUNTIME_LIFECYCLE_ERROR_DMA_WINDOW);
    config.dma_ipa_base = 0x81000000;
    transport.operations.route = NULL;
    assert(sep_runtime_transport_initialize(span, &config) ==
        SEP_RUNTIME_LIFECYCLE_ERROR_CALLBACK);
}

int main(void)
{
    test_initialize_transcript_and_accessors();
    test_send_receive_and_teardown_transcript();
    test_validation_contracts();
    puts("sep runtime transport lifecycle tests: PASS");
    return 0;
}
