#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "avp/protocol/sep/sep_mailbox_channel_adapter.h"


typedef enum {
    EVENT_INITIALIZE,
    EVENT_READ32,
    EVENT_WRITE32,
    EVENT_WRITE_SLOT,
    EVENT_READ_SLOT,
    EVENT_DIAGNOSTIC,
} EventKind;

typedef struct {
    EventKind kind;
    uintptr_t address;
    uint64_t value;
    SEPMailboxSlot slot;
} Event;

typedef struct {
    Event events[32];
    size_t event_count;
    uint32_t send_controls[8];
    size_t send_count;
    size_t send_index;
    uint32_t receive_controls[8];
    size_t receive_count;
    size_t receive_index;
    uint64_t now;
    unsigned elapsed_calls;
    bool timeout_result;
    SEPMailboxSlot receive_slot;
} Transcript;

static void record(
    Transcript *transcript,
    EventKind kind,
    uintptr_t address,
    uint64_t value,
    const SEPMailboxSlot *slot
)
{
    Event *event;

    assert(transcript->event_count < sizeof(transcript->events) / sizeof(transcript->events[0]));
    event = &transcript->events[transcript->event_count++];
    event->kind = kind;
    event->address = address;
    event->value = value;
    if (slot != NULL) {
        event->slot = *slot;
    }
}

static int initialize_hook(
    void *context,
    SEPRuntimeMailboxChannel *channel,
    const void *lower_bound,
    const void *upper_bound
)
{
    Transcript *transcript = context;
    assert((uintptr_t)lower_bound <= (uintptr_t)channel);
    assert((uintptr_t)upper_bound >= (uintptr_t)channel + sizeof(*channel));
    record(transcript, EVENT_INITIALIZE, channel->register_base, 0, NULL);
    return 0;
}

static uint32_t next_control(
    uint32_t *values,
    size_t count,
    size_t *index
)
{
    assert(count != 0);
    if (*index + 1 < count) {
        return values[(*index)++];
    }
    return values[*index];
}

static uint32_t read32_hook(void *context, uintptr_t address)
{
    Transcript *transcript = context;
    uint32_t value;

    if (address == 0x30250020) {
        value = next_control(
            transcript->send_controls,
            transcript->send_count,
            &transcript->send_index
        );
    } else {
        assert(address == 0x3025001c);
        value = next_control(
            transcript->receive_controls,
            transcript->receive_count,
            &transcript->receive_index
        );
    }
    record(transcript, EVENT_READ32, address, value, NULL);
    return value;
}

static int write32_hook(void *context, uintptr_t address, uint32_t value)
{
    Transcript *transcript = context;
    record(transcript, EVENT_WRITE32, address, value, NULL);
    return 0;
}

static int write_slot_hook(
    void *context,
    uintptr_t address,
    const SEPMailboxSlot *slot
)
{
    Transcript *transcript = context;
    record(transcript, EVENT_WRITE_SLOT, address, 0, slot);
    return 0;
}

static int read_slot_hook(
    void *context,
    uintptr_t address,
    SEPMailboxSlot *slot
)
{
    Transcript *transcript = context;
    *slot = transcript->receive_slot;
    record(transcript, EVENT_READ_SLOT, address, 0, slot);
    return 0;
}

static uint64_t read_time_hook(void *context)
{
    Transcript *transcript = context;
    return transcript->now;
}

static bool timeout_elapsed_hook(
    void *context,
    uint64_t start,
    uint32_t timeout
)
{
    Transcript *transcript = context;
    assert(start == transcript->now);
    assert(timeout != 0 && timeout != UINT32_MAX);
    ++transcript->elapsed_calls;
    return transcript->timeout_result;
}

static void diagnostic_hook(
    void *context,
    uint64_t source_token,
    uint32_t line
)
{
    Transcript *transcript = context;
    record(transcript, EVENT_DIAGNOSTIC, line, source_token, NULL);
}

static const SEPMailboxOperations operations = {
    .initialize = initialize_hook,
    .read32 = read32_hook,
    .write32 = write32_hook,
    .write_slot = write_slot_hook,
    .read_slot = read_slot_hook,
    .read_time = read_time_hook,
    .timeout_elapsed = timeout_elapsed_hook,
    .diagnostic = diagnostic_hook,
};

static SEPRuntimeMailboxChannelSpan channel_span(
    SEPRuntimeMailboxChannel *channel
)
{
    return (SEPRuntimeMailboxChannelSpan){
        .pointer = channel,
        .lower_bound = channel,
        .upper_bound = (const uint8_t *)channel + sizeof(*channel),
    };
}

static SEPBoundsSpan object_span(void *pointer, size_t size)
{
    return (SEPBoundsSpan){
        .pointer = pointer,
        .lower_bound = pointer,
        .upper_bound = (const uint8_t *)pointer + size,
    };
}

static SEPRuntimeMailboxChannelConfiguration channel_configuration(
    const SEPRuntimeTransportLayout *layout,
    Transcript *transcript
)
{
    return (SEPRuntimeMailboxChannelConfiguration){
        .register_base = 0x30250000,
        .provider_context = 0x7002abb0,
        .operations_span = object_span((void *)&operations, sizeof(operations)),
        .layout_span = object_span((void *)layout, sizeof(*layout)),
        .configuration = {0x11223344, 0x55667788},
        .layout = layout,
        .operations = &operations,
        .operation_context = transcript,
        .clear_route_state = true,
    };
}

static void test_channel_construct_and_route_state(void)
{
    const SEPRuntimeTransportLayout layout = {
        .send_control_offset = 0x20,
        .receive_control_offset = 0x1c,
        .send_slot_offset = 0x200,
        .receive_slot_offset = 0x100,
        .initialization_control_offset = 0x18,
    };
    Transcript transcript = {0};
    SEPRuntimeMailboxChannel channel = {
        .route_state = {1, 2, 3, 4, 5},
        .preserved_route_field = 0xcafef00d,
    };
    SEPRuntimeMailboxChannelSpan span = channel_span(&channel);
    SEPRuntimeMailboxChannelConfiguration config =
        channel_configuration(&layout, &transcript);
    SEPRuntimeMailboxChannelSpan identity;

    assert(sep_runtime_transport_channel_construct(span, &config) == SEP_MAILBOX_OK);
    assert(channel.initialized);
    assert(channel.register_base == 0x30250000);
    assert(channel.provider_context == 0x7002abb0);
    assert(channel.configuration[0] == 0x11223344);
    assert(channel.configuration[1] == 0x55667788);
    assert(channel.preserved_route_field == 0xcafef00d);
    for (size_t index = 0; index < 5; ++index) {
        assert(channel.route_state[index] == 0);
    }
    assert(transcript.event_count == 2);
    assert(transcript.events[0].kind == EVENT_INITIALIZE);
    assert(transcript.events[1].kind == EVENT_WRITE32);
    assert(transcript.events[1].address == 0x30250018);
    assert(transcript.events[1].value == SEP_MAILBOX_CHANNEL_MAGIC);

    identity = sep_runtime_transport_outlined_identity_channel_span(span);
    assert(identity.pointer == span.pointer);
    assert(identity.lower_bound == span.lower_bound);
    assert(identity.upper_bound == span.upper_bound);
    assert(sep_runtime_transport_channel_clear_route_state(span) == SEP_MAILBOX_OK);

    span.upper_bound = (const uint8_t *)&channel + sizeof(channel) - 1;
    assert(sep_runtime_transport_channel_bounds_panic(span) ==
        SEP_MAILBOX_ERROR_BOUNDS);
    assert(sep_runtime_transport_channel_clear_route_state(span) ==
        SEP_MAILBOX_ERROR_BOUNDS);
}

static void test_slot_helpers(void)
{
    SEPMailboxOwnedSlot owned_slot = {
        .active_flags = 0x20,
        .control_word = 0x40,
    };
    SEPBoundsSpan owned_span = object_span(&owned_slot, sizeof(owned_slot));
    SEPMailboxSlot slot = {
        .words = {UINT64_MAX, UINT64_MAX},
    };
    SEPBoundsSpan slot_span = object_span(&slot, sizeof(slot));
    SEPBoundsSpan short_span = slot_span;
    SEPBoundsSpan restored_context;
    SEPBoundsSpan restored_slot;
    SEPMailboxSavedSpans saved = {0};
    uint32_t context_value = 7;
    SEPBoundsSpan context_span = object_span(&context_value, sizeof(context_value));

    assert(sep_mailbox_slot_mark_active(owned_span) == SEP_MAILBOX_OK);
    assert(owned_slot.active_flags == 0x21);
    assert(owned_slot.control_word == 0x80000040);
    assert(sep_mailbox_validate_slot_span(slot_span) == SEP_MAILBOX_OK);
    short_span.upper_bound = (const uint8_t *)&slot + sizeof(slot) - 1;
    assert(sep_mailbox_validate_slot_span(short_span) == SEP_MAILBOX_ERROR_BOUNDS);

    assert(sep_mailbox_outlined_save_context_and_validate_slot_span(
        &saved,
        context_span,
        slot_span,
        SEP_MAILBOX_SLOT_SIZE
    ) == SEP_MAILBOX_OK);
    assert(!sep_mailbox_outlined_save_slot_span_x23_x26_and_test_timeout(
        &saved,
        slot_span,
        8
    ));
    assert(sep_mailbox_outlined_save_slot_span_x23_x26_and_test_timeout(
        &saved,
        slot_span,
        UINT32_MAX
    ));
    assert(sep_mailbox_outlined_restore_io_spans_x22_x26(
        &saved,
        &restored_context,
        &restored_slot
    ) == SEP_MAILBOX_OK);
    assert(restored_context.pointer == &context_value);
    assert(restored_slot.pointer == &slot);

    assert(sep_clear_receive_slot(slot_span) == SEP_MAILBOX_OK);
    assert(slot.words[0] == 0 && slot.words[1] == 0);
    sep_mailbox_method_slot3_noop();
    assert(sep_mailbox_source_token() == UINT64_C(0x0bab0172d798c3be));
}

static void construct_channel(
    SEPRuntimeMailboxChannel *channel,
    Transcript *transcript,
    const SEPRuntimeTransportLayout *layout
)
{
    SEPRuntimeMailboxChannelConfiguration config =
        channel_configuration(layout, transcript);
    config.clear_route_state = false;
    assert(sep_runtime_transport_channel_construct(
        channel_span(channel),
        &config
    ) == SEP_MAILBOX_OK);
    transcript->event_count = 0;
}

static void test_send_receive_transcripts(void)
{
    const SEPRuntimeTransportLayout layout = {
        .send_control_offset = 0x20,
        .receive_control_offset = 0x1c,
        .send_slot_offset = 0x200,
        .receive_slot_offset = 0x100,
    };
    Transcript transcript = {
        .send_controls = {SEP_MAILBOX_SEND_BUSY, 0},
        .send_count = 2,
        .receive_controls = {SEP_MAILBOX_RECEIVE_EMPTY, 0},
        .receive_count = 2,
        .now = 0x12345678,
        .receive_slot = {
            .words = {UINT64_C(0x1122334455667788), UINT64_C(0xaabbccddeeff0011)},
        },
    };
    SEPRuntimeMailboxChannel channel = {0};
    SEPMailboxSlot send_slot = {
        .words = {UINT64_C(0x1020304050607080), UINT64_C(0x90a0b0c0d0e0f000)},
    };
    SEPMailboxSlot receive_slot = {0};
    SEPBoundsSpan send_span = object_span(&send_slot, sizeof(send_slot));
    SEPBoundsSpan receive_span = object_span(&receive_slot, sizeof(receive_slot));
    uint32_t receive_control;

    construct_channel(&channel, &transcript, &layout);
    assert(sep_mailbox_send_slot(
        channel_span(&channel),
        send_span,
        SEP_MAILBOX_SLOT_SIZE,
        8
    ) == SEP_MAILBOX_OK);
    assert(transcript.event_count == 3);
    assert(transcript.events[0].kind == EVENT_READ32);
    assert(transcript.events[0].value == SEP_MAILBOX_SEND_BUSY);
    assert(transcript.events[1].kind == EVENT_READ32);
    assert(transcript.events[1].value == 0);
    assert(transcript.events[2].kind == EVENT_WRITE_SLOT);
    assert(transcript.events[2].address == 0x30250200);
    assert(memcmp(&transcript.events[2].slot, &send_slot, sizeof(send_slot)) == 0);

    transcript.event_count = 0;
    assert(sep_mailbox_receive_slot(
        channel_span(&channel),
        receive_span,
        SEP_MAILBOX_SLOT_SIZE,
        8
    ) == SEP_MAILBOX_OK);
    assert(transcript.events[2].kind == EVENT_READ_SLOT);
    assert(transcript.events[2].address == 0x30250100);
    assert(receive_slot.words[0] == UINT64_C(0x1122334455667788));
    assert(receive_slot.words[1] == UINT64_C(0xeeff0011));

    transcript.receive_index = 1;
    assert(sep_runtime_transport_read_receive_control(
        channel_span(&channel),
        &receive_control
    ) == SEP_MAILBOX_OK);
    assert(receive_control == 0);
}

static void test_timeout_and_error_contracts(void)
{
    const SEPRuntimeTransportLayout layout = {
        .send_control_offset = 0x20,
        .receive_control_offset = 0x1c,
        .send_slot_offset = 0x200,
        .receive_slot_offset = 0x100,
    };
    Transcript transcript = {
        .send_controls = {SEP_MAILBOX_SEND_BUSY},
        .send_count = 1,
        .receive_controls = {SEP_MAILBOX_CONTROL_ERROR_MASK},
        .receive_count = 1,
        .now = 99,
        .timeout_result = true,
    };
    SEPRuntimeMailboxChannel channel = {0};
    SEPMailboxSlot slot = {0};
    SEPBoundsSpan slot_span = object_span(&slot, sizeof(slot));

    construct_channel(&channel, &transcript, &layout);
    assert(sep_mailbox_send_slot(
        channel_span(&channel),
        slot_span,
        SEP_MAILBOX_SLOT_SIZE,
        4
    ) == SEP_MAILBOX_ERROR_TIMEOUT);
    assert(transcript.elapsed_calls == 1);
    assert(transcript.events[1].kind == EVENT_DIAGNOSTIC);
    assert(transcript.events[1].address == 0xae);
    assert(transcript.events[1].value == sep_mailbox_source_token());

    transcript.event_count = 0;
    assert(sep_mailbox_receive_slot(
        channel_span(&channel),
        slot_span,
        SEP_MAILBOX_SLOT_SIZE,
        0
    ) == SEP_MAILBOX_ERROR_STATE);
    assert(transcript.event_count == 1);

    transcript.receive_controls[0] = SEP_MAILBOX_RECEIVE_EMPTY;
    transcript.receive_index = 0;
    transcript.event_count = 0;
    assert(sep_mailbox_receive_slot(
        channel_span(&channel),
        slot_span,
        SEP_MAILBOX_SLOT_SIZE,
        4
    ) == SEP_MAILBOX_ERROR_TIMEOUT);
    assert(transcript.events[1].kind == EVENT_DIAGNOSTIC);
    assert(transcript.events[1].address == 0x74);

    assert(sep_mailbox_outlined_save_context_and_validate_slot_span(
        NULL,
        slot_span,
        slot_span,
        SEP_MAILBOX_SLOT_SIZE
    ) == SEP_MAILBOX_ERROR_ARGUMENT);
}

int main(void)
{
    test_channel_construct_and_route_state();
    test_slot_helpers();
    test_send_receive_transcripts();
    test_timeout_and_error_contracts();
    puts("sep mailbox channel adapter tests: PASS");
    return 0;
}
