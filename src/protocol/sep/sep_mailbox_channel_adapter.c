#include "avp/protocol/sep/sep_mailbox_channel_adapter.h"


static bool span_contains(const SEPBoundsSpan *span, size_t size)
{
    uintptr_t pointer;
    uintptr_t lower;
    uintptr_t upper;

    if (span == NULL) {
        return false;
    }
    pointer = (uintptr_t)span->pointer;
    lower = (uintptr_t)span->lower_bound;
    upper = (uintptr_t)span->upper_bound;
    return span->pointer != NULL && lower <= pointer && pointer < upper &&
        size <= upper - pointer;
}

static bool channel_span_contains(SEPRuntimeMailboxChannelSpan span)
{
    SEPBoundsSpan bounds = {
        .pointer = span.pointer,
        .lower_bound = span.lower_bound,
        .upper_bound = span.upper_bound,
    };

    return span_contains(&bounds, sizeof(*span.pointer));
}

static int require_channel(
    SEPRuntimeMailboxChannelSpan span,
    SEPRuntimeMailboxChannel **channel
)
{
    if (channel == NULL) {
        return SEP_MAILBOX_ERROR_ARGUMENT;
    }
    if (!channel_span_contains(span)) {
        return sep_runtime_transport_channel_bounds_panic(span);
    }
    *channel = span.pointer;
    return SEP_MAILBOX_OK;
}

int sep_runtime_transport_channel_bounds_panic(
    SEPRuntimeMailboxChannelSpan channel
)
{
    (void)channel;
    return SEP_MAILBOX_ERROR_BOUNDS;
}

int sep_runtime_transport_channel_clear_route_state(
    SEPRuntimeMailboxChannelSpan span
)
{
    SEPRuntimeMailboxChannel *channel;
    size_t index;
    int status = require_channel(span, &channel);

    if (status != SEP_MAILBOX_OK) {
        return status;
    }
    for (index = 0; index < 5; ++index) {
        channel->route_state[index] = 0;
    }
    return SEP_MAILBOX_OK;
}

int sep_runtime_transport_channel_construct(
    SEPRuntimeMailboxChannelSpan span,
    const SEPRuntimeMailboxChannelConfiguration *configuration
)
{
    SEPRuntimeMailboxChannel *channel;
    uintptr_t register_address;
    int status = require_channel(span, &channel);

    if (status != SEP_MAILBOX_OK) {
        return status;
    }
    if (configuration == NULL || configuration->register_base == 0 ||
        configuration->layout == NULL || configuration->operations == NULL ||
        configuration->operations->initialize == NULL) {
        return SEP_MAILBOX_ERROR_ARGUMENT;
    }
    if (!span_contains(
            &configuration->operations_span,
            sizeof(SEPMailboxOperations)
        ) ||
        !span_contains(
            &configuration->layout_span,
            sizeof(SEPRuntimeTransportLayout)
        )) {
        return SEP_MAILBOX_ERROR_BOUNDS;
    }

    channel->register_base = configuration->register_base;
    channel->provider_context = configuration->provider_context;
    channel->operations_span = configuration->operations_span;
    channel->layout_span = configuration->layout_span;
    channel->configuration[0] = configuration->configuration[0];
    channel->configuration[1] = configuration->configuration[1];
    channel->layout = configuration->layout;
    channel->operations = configuration->operations;
    channel->operation_context = configuration->operation_context;
    channel->initialized = true;

    status = channel->operations->initialize(
        channel->operation_context,
        channel,
        span.lower_bound,
        span.upper_bound
    );
    if (status != 0) {
        channel->initialized = false;
        return SEP_MAILBOX_ERROR_CALLBACK;
    }
    if (configuration->clear_route_state) {
        status = sep_runtime_transport_channel_clear_route_state(span);
        if (status != SEP_MAILBOX_OK) {
            return status;
        }
    }
    if (channel->layout->initialization_control_offset == 0) {
        return SEP_MAILBOX_OK;
    }
    if (channel->operations->write32 == NULL) {
        return SEP_MAILBOX_ERROR_CALLBACK;
    }
    register_address = channel->register_base +
        channel->layout->initialization_control_offset;
    if (channel->operations->write32(
            channel->operation_context,
            register_address,
            SEP_MAILBOX_CHANNEL_MAGIC
        ) != 0) {
        return SEP_MAILBOX_ERROR_IO;
    }
    return SEP_MAILBOX_OK;
}

SEPRuntimeMailboxChannelSpan sep_runtime_transport_outlined_identity_channel_span(
    SEPRuntimeMailboxChannelSpan channel
)
{
    return channel;
}

int sep_mailbox_validate_slot_span(SEPBoundsSpan slot_span)
{
    return span_contains(&slot_span, SEP_MAILBOX_SLOT_SIZE)
        ? SEP_MAILBOX_OK
        : SEP_MAILBOX_ERROR_BOUNDS;
}

int sep_mailbox_slot_mark_active(SEPBoundsSpan owned_slot_span)
{
    SEPMailboxOwnedSlot *owned_slot;

    if (!span_contains(&owned_slot_span, sizeof(SEPMailboxOwnedSlot))) {
        return SEP_MAILBOX_ERROR_BOUNDS;
    }
    owned_slot = (SEPMailboxOwnedSlot *)owned_slot_span.pointer;
    owned_slot->active_flags |= SEP_MAILBOX_ACTIVE;
    owned_slot->control_word |= SEP_MAILBOX_OWNED;
    return SEP_MAILBOX_OK;
}

uint64_t sep_mailbox_source_token(void)
{
    return UINT64_C(0x0bab0172d798c3be);
}

void sep_mailbox_log_timeout_source_line(
    const SEPRuntimeMailboxChannel *channel,
    uint32_t line
)
{
    if (channel != NULL && channel->operations != NULL &&
        channel->operations->diagnostic != NULL) {
        channel->operations->diagnostic(
            channel->operation_context,
            sep_mailbox_source_token(),
            line
        );
    }
}

int sep_mailbox_outlined_save_context_and_validate_slot_span(
    SEPMailboxSavedSpans *saved,
    SEPBoundsSpan context_span,
    SEPBoundsSpan slot_span,
    uint32_t slot_size
)
{
    if (saved == NULL || slot_size != SEP_MAILBOX_SLOT_SIZE) {
        return SEP_MAILBOX_ERROR_ARGUMENT;
    }
    if (sep_mailbox_validate_slot_span(slot_span) != SEP_MAILBOX_OK) {
        return SEP_MAILBOX_ERROR_BOUNDS;
    }
    saved->context_span = context_span;
    saved->slot_span = slot_span;
    saved->slot_size = slot_size;
    return SEP_MAILBOX_OK;
}

bool sep_mailbox_outlined_save_slot_span_x23_x26_and_test_timeout(
    SEPMailboxSavedSpans *saved,
    SEPBoundsSpan slot_span,
    uint32_t timeout
)
{
    if (saved == NULL) {
        return false;
    }
    saved->slot_span = slot_span;
    saved->timeout = timeout;
    return timeout == UINT32_MAX;
}

int sep_mailbox_outlined_restore_io_spans_x22_x26(
    const SEPMailboxSavedSpans *saved,
    SEPBoundsSpan *context_span,
    SEPBoundsSpan *slot_span
)
{
    if (saved == NULL || context_span == NULL || slot_span == NULL) {
        return SEP_MAILBOX_ERROR_ARGUMENT;
    }
    *context_span = saved->context_span;
    *slot_span = saved->slot_span;
    return SEP_MAILBOX_OK;
}

void sep_mailbox_method_slot3_noop(void)
{
}

int sep_runtime_transport_read_receive_control(
    SEPRuntimeMailboxChannelSpan span,
    uint32_t *control
)
{
    SEPRuntimeMailboxChannel *channel;
    int status = require_channel(span, &channel);

    if (status != SEP_MAILBOX_OK) {
        return status;
    }
    if (control == NULL || channel->layout == NULL) {
        return SEP_MAILBOX_ERROR_ARGUMENT;
    }
    if (channel->operations == NULL || channel->operations->read32 == NULL) {
        return SEP_MAILBOX_ERROR_CALLBACK;
    }
    *control = channel->operations->read32(
        channel->operation_context,
        channel->register_base + channel->layout->receive_control_offset
    );
    return SEP_MAILBOX_OK;
}

int sep_clear_receive_slot(SEPBoundsSpan slot_span)
{
    SEPMailboxSlot *slot;

    if (sep_mailbox_validate_slot_span(slot_span) != SEP_MAILBOX_OK) {
        return SEP_MAILBOX_ERROR_BOUNDS;
    }
    slot = (SEPMailboxSlot *)slot_span.pointer;
    slot->words[0] = 0;
    slot->words[1] = 0;
    return SEP_MAILBOX_OK;
}

static int validate_io(
    SEPRuntimeMailboxChannelSpan span,
    SEPBoundsSpan slot_span,
    uint32_t slot_size,
    SEPRuntimeMailboxChannel **channel
)
{
    int status = require_channel(span, channel);

    if (status != SEP_MAILBOX_OK) {
        return status;
    }
    if (!(*channel)->initialized || (*channel)->layout == NULL ||
        (*channel)->operations == NULL) {
        return SEP_MAILBOX_ERROR_STATE;
    }
    if (slot_size != SEP_MAILBOX_SLOT_SIZE) {
        return SEP_MAILBOX_ERROR_ARGUMENT;
    }
    return sep_mailbox_validate_slot_span(slot_span);
}

static int wait_until_ready(
    SEPRuntimeMailboxChannel *channel,
    uint32_t control_offset,
    uint32_t busy_mask,
    uint32_t timeout,
    uint32_t timeout_line
)
{
    const SEPMailboxOperations *operations = channel->operations;
    uint64_t start = 0;
    uint32_t control;

    if (operations->read32 == NULL) {
        return SEP_MAILBOX_ERROR_CALLBACK;
    }
    if (timeout != 0 && timeout != UINT32_MAX) {
        if (operations->read_time == NULL || operations->timeout_elapsed == NULL) {
            return SEP_MAILBOX_ERROR_CALLBACK;
        }
        start = operations->read_time(channel->operation_context);
    }
    for (;;) {
        control = operations->read32(
            channel->operation_context,
            channel->register_base + control_offset
        );
        if ((control & SEP_MAILBOX_CONTROL_ERROR_MASK) != 0) {
            return SEP_MAILBOX_ERROR_STATE;
        }
        if ((control & busy_mask) == 0) {
            return SEP_MAILBOX_OK;
        }
        if (timeout == 0) {
            return SEP_MAILBOX_ERROR_STATE;
        }
        if (timeout != UINT32_MAX && operations->timeout_elapsed(
                channel->operation_context,
                start,
                timeout
            )) {
            sep_mailbox_log_timeout_source_line(channel, timeout_line);
            return SEP_MAILBOX_ERROR_TIMEOUT;
        }
    }
}

int sep_mailbox_send_slot(
    SEPRuntimeMailboxChannelSpan span,
    SEPBoundsSpan slot_span,
    uint32_t slot_size,
    uint32_t timeout
)
{
    SEPRuntimeMailboxChannel *channel;
    int status = validate_io(span, slot_span, slot_size, &channel);

    if (status != SEP_MAILBOX_OK) {
        return status;
    }
    status = wait_until_ready(
        channel,
        channel->layout->send_control_offset,
        SEP_MAILBOX_SEND_BUSY,
        timeout,
        0xae
    );
    if (status != SEP_MAILBOX_OK) {
        return status;
    }
    if (channel->operations->write_slot == NULL) {
        return SEP_MAILBOX_ERROR_CALLBACK;
    }
    if (channel->operations->write_slot(
            channel->operation_context,
            channel->register_base + channel->layout->send_slot_offset,
            (const SEPMailboxSlot *)slot_span.pointer
        ) != 0) {
        return SEP_MAILBOX_ERROR_IO;
    }
    return SEP_MAILBOX_OK;
}

int sep_mailbox_receive_slot(
    SEPRuntimeMailboxChannelSpan span,
    SEPBoundsSpan slot_span,
    uint32_t slot_size,
    uint32_t timeout
)
{
    SEPRuntimeMailboxChannel *channel;
    SEPMailboxSlot *slot;
    int status = validate_io(span, slot_span, slot_size, &channel);

    if (status != SEP_MAILBOX_OK) {
        return status;
    }
    status = wait_until_ready(
        channel,
        channel->layout->receive_control_offset,
        SEP_MAILBOX_RECEIVE_EMPTY,
        timeout,
        0x74
    );
    if (status != SEP_MAILBOX_OK) {
        return status;
    }
    if (channel->operations->read_slot == NULL) {
        return SEP_MAILBOX_ERROR_CALLBACK;
    }
    slot = (SEPMailboxSlot *)slot_span.pointer;
    if (channel->operations->read_slot(
            channel->operation_context,
            channel->register_base + channel->layout->receive_slot_offset,
            slot
        ) != 0) {
        return SEP_MAILBOX_ERROR_IO;
    }
    slot->words[1] &= UINT64_C(0xffffffff);
    return SEP_MAILBOX_OK;
}
