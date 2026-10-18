#include "avp/protocol/sep/sep_runtime_transport_lifecycle.h"


static int set_error(SEPRuntimeTransportLifecycle *transport, int error)
{
    if (transport != NULL) {
        transport->last_error = error;
    }
    return error;
}

static bool span_contains_transport(SEPRuntimeTransportLifecycleSpan span)
{
    uintptr_t pointer = (uintptr_t)span.pointer;
    uintptr_t lower = (uintptr_t)span.lower_bound;
    uintptr_t upper = (uintptr_t)span.upper_bound;

    return span.pointer != NULL && lower <= pointer && pointer < upper &&
        sizeof(*span.pointer) <= upper - pointer;
}

static bool span_contains_buffer(SEPBoundsSpan span, size_t byte_count)
{
    uintptr_t pointer = (uintptr_t)span.pointer;
    uintptr_t lower = (uintptr_t)span.lower_bound;
    uintptr_t upper = (uintptr_t)span.upper_bound;

    return span.pointer != NULL && byte_count != 0 && lower <= pointer &&
        pointer < upper && byte_count <= upper - pointer;
}

static int require_transport(
    SEPRuntimeTransportLifecycleSpan span,
    SEPRuntimeTransportLifecycle **transport
)
{
    if (transport == NULL) {
        return SEP_RUNTIME_LIFECYCLE_ERROR_ARGUMENT;
    }
    if (!span_contains_transport(span)) {
        return sep_runtime_transport_interrupt_mask_bounds_panic(span);
    }
    *transport = span.pointer;
    return SEP_RUNTIME_LIFECYCLE_OK;
}

static bool version_is_supported(uint16_t version)
{
    return version == SEP_RUNTIME_TRANSPORT_VERSION_1_0_1 ||
        version == SEP_RUNTIME_TRANSPORT_VERSION_2_0_0;
}

static bool dma_window_is_supported(uint16_t size)
{
    return size == 0 || size == 0x20 || size == 0x40;
}

static int write_register(
    SEPRuntimeTransportLifecycle *transport,
    uintptr_t address,
    uint32_t value
)
{
    if (transport->operations.write32 == NULL) {
        return set_error(transport, SEP_RUNTIME_LIFECYCLE_ERROR_CALLBACK);
    }
    if (transport->operations.write32(
            transport->operation_context,
            address,
            value
        ) != 0) {
        return set_error(transport, SEP_RUNTIME_LIFECYCLE_ERROR_IO);
    }
    return SEP_RUNTIME_LIFECYCLE_OK;
}

static int write_primary_register(
    SEPRuntimeTransportLifecycleSpan span,
    SEPRuntimeTransportLifecycle *transport,
    uint32_t offset,
    uint32_t value
)
{
    uintptr_t address;
    int status = sep_runtime_transport_primary_register_address(span, offset, &address);

    if (status != SEP_RUNTIME_LIFECYCLE_OK) {
        return status;
    }
    return write_register(transport, address, value);
}

static int apply_interrupt_mask(
    SEPRuntimeTransportLifecycle *transport,
    uintptr_t address,
    uint32_t mask,
    bool enable
)
{
    uint32_t value = transport->operations.read32(transport->operation_context, address);

    value = enable ? value | mask : value & ~mask;
    return write_register(transport, address, value);
}

static int update_interrupt_mask(
    SEPRuntimeTransportLifecycleSpan span,
    bool enable
)
{
    SEPRuntimeTransportLifecycle *transport;
    uintptr_t address;
    uint32_t mask;
    int status = require_transport(span, &transport);

    if (status != SEP_RUNTIME_LIFECYCLE_OK) {
        return status;
    }
    if (transport->operations.read32 == NULL) {
        return set_error(transport, SEP_RUNTIME_LIFECYCLE_ERROR_CALLBACK);
    }
    status = sep_runtime_transport_interrupt_mask_value(span, &mask);
    if (status != SEP_RUNTIME_LIFECYCLE_OK) {
        return status;
    }
    status = sep_runtime_transport_primary_register_address(
        span,
        transport->interrupt_register_offset,
        &address
    );
    if (status != SEP_RUNTIME_LIFECYCLE_OK) {
        return status;
    }
    status = apply_interrupt_mask(transport, address, mask, enable);
    if (status != SEP_RUNTIME_LIFECYCLE_OK) {
        return status;
    }
    if (transport->secondary_register_base == 0) {
        return SEP_RUNTIME_LIFECYCLE_OK;
    }
    status = sep_runtime_transport_secondary_register_address(
        span,
        transport->interrupt_register_offset,
        &address
    );
    if (status != SEP_RUNTIME_LIFECYCLE_OK) {
        return status;
    }
    return apply_interrupt_mask(transport, address, mask, enable);
}

int sep_runtime_transport_interrupt_mask_bounds_panic(
    SEPRuntimeTransportLifecycleSpan span
)
{
    (void)span;
    return SEP_RUNTIME_LIFECYCLE_ERROR_BOUNDS;
}

int sep_runtime_transport_dma_ipa_slot(
    SEPRuntimeTransportLifecycleSpan span,
    uintptr_t *slot
)
{
    SEPRuntimeTransportLifecycle *transport;
    int status = require_transport(span, &transport);

    if (status != SEP_RUNTIME_LIFECYCLE_OK) {
        return status;
    }
    if (slot == NULL) {
        return set_error(transport, SEP_RUNTIME_LIFECYCLE_ERROR_ARGUMENT);
    }
    *slot = transport->dma_ipa_base + SEP_RUNTIME_TRANSPORT_DMA_SLOT_OFFSET;
    return set_error(transport, SEP_RUNTIME_LIFECYCLE_OK);
}

int sep_runtime_transport_primary_register_address(
    SEPRuntimeTransportLifecycleSpan span,
    uint32_t offset,
    uintptr_t *address
)
{
    SEPRuntimeTransportLifecycle *transport;
    int status = require_transport(span, &transport);

    if (status != SEP_RUNTIME_LIFECYCLE_OK) {
        return status;
    }
    if (address == NULL) {
        return set_error(transport, SEP_RUNTIME_LIFECYCLE_ERROR_ARGUMENT);
    }
    *address = transport->primary_register_base + offset;
    return set_error(transport, SEP_RUNTIME_LIFECYCLE_OK);
}

int sep_runtime_transport_secondary_register_address(
    SEPRuntimeTransportLifecycleSpan span,
    uint32_t offset,
    uintptr_t *address
)
{
    SEPRuntimeTransportLifecycle *transport;
    int status = require_transport(span, &transport);

    if (status != SEP_RUNTIME_LIFECYCLE_OK) {
        return status;
    }
    if (address == NULL || transport->secondary_register_base == 0) {
        return set_error(transport, SEP_RUNTIME_LIFECYCLE_ERROR_ARGUMENT);
    }
    *address = transport->secondary_register_base + offset;
    return set_error(transport, SEP_RUNTIME_LIFECYCLE_OK);
}

int sep_runtime_transport_interrupt_mask_value(
    SEPRuntimeTransportLifecycleSpan span,
    uint32_t *mask
)
{
    SEPRuntimeTransportLifecycle *transport;
    int status = require_transport(span, &transport);

    if (status != SEP_RUNTIME_LIFECYCLE_OK) {
        return status;
    }
    if (mask == NULL) {
        return set_error(transport, SEP_RUNTIME_LIFECYCLE_ERROR_ARGUMENT);
    }
    *mask = transport->interrupt_mask == 0
        ? SEP_RUNTIME_TRANSPORT_DEFAULT_INTERRUPT_MASK
        : transport->interrupt_mask;
    return set_error(transport, SEP_RUNTIME_LIFECYCLE_OK);
}

int sep_runtime_transport_channel_initialize(
    SEPRuntimeTransportLifecycleSpan span,
    bool route_state
)
{
    SEPRuntimeTransportLifecycle *transport;
    SEPRuntimeTransportChannel *channel;
    int status = require_transport(span, &transport);

    if (status != SEP_RUNTIME_LIFECYCLE_OK) {
        return status;
    }
    channel = &transport->channel;
    channel->register_base = transport->primary_register_base;
    channel->layout = transport->layout;
    channel->provider_context = transport->provider_context;
    channel->configuration[0] = transport->channel_configuration[0];
    channel->configuration[1] = transport->channel_configuration[1];
    channel->route_state = route_state;
    channel->initialized = true;
    if (transport->operations.channel_initialize != NULL &&
        transport->operations.channel_initialize(
            transport->operation_context,
            channel
        ) != 0) {
        channel->initialized = false;
        return set_error(transport, SEP_RUNTIME_LIFECYCLE_ERROR_CALLBACK);
    }
    return set_error(transport, SEP_RUNTIME_LIFECYCLE_OK);
}

int sep_runtime_transport_invoke_lifecycle_hook(
    SEPRuntimeTransportLifecycleSpan span,
    bool active
)
{
    SEPRuntimeTransportLifecycle *transport;
    int status = require_transport(span, &transport);

    if (status != SEP_RUNTIME_LIFECYCLE_OK) {
        return status;
    }
    if (transport->operations.lifecycle == NULL) {
        return set_error(transport, SEP_RUNTIME_LIFECYCLE_ERROR_CALLBACK);
    }
    if (transport->operations.lifecycle(
            transport->operation_context,
            transport,
            active
        ) != 0) {
        return set_error(transport, SEP_RUNTIME_LIFECYCLE_ERROR_CALLBACK);
    }
    return set_error(transport, SEP_RUNTIME_LIFECYCLE_OK);
}

int sep_runtime_transport_send(
    SEPRuntimeTransportLifecycleSpan span,
    SEPBoundsSpan buffer,
    size_t byte_count,
    uint32_t flags
)
{
    SEPRuntimeTransportLifecycle *transport;
    int status = require_transport(span, &transport);

    if (status != SEP_RUNTIME_LIFECYCLE_OK) {
        return status;
    }
    if (!transport->active || !transport->channel.initialized) {
        return set_error(transport, SEP_RUNTIME_LIFECYCLE_ERROR_STATE);
    }
    if (!span_contains_buffer(buffer, byte_count)) {
        return set_error(transport, SEP_RUNTIME_LIFECYCLE_ERROR_BOUNDS);
    }
    if (transport->operations.send == NULL) {
        return set_error(transport, SEP_RUNTIME_LIFECYCLE_ERROR_CALLBACK);
    }
    status = transport->operations.send(
        transport->operation_context,
        &transport->channel,
        buffer.pointer,
        byte_count,
        flags
    );
    return set_error(transport, status);
}

int sep_runtime_transport_receive(
    SEPRuntimeTransportLifecycleSpan span,
    SEPBoundsSpan buffer,
    size_t byte_count,
    uintptr_t transfer_context
)
{
    SEPRuntimeTransportLifecycle *transport;
    int status = require_transport(span, &transport);

    if (status != SEP_RUNTIME_LIFECYCLE_OK) {
        return status;
    }
    if (!transport->active || !transport->channel.initialized) {
        return set_error(transport, SEP_RUNTIME_LIFECYCLE_ERROR_STATE);
    }
    if (!span_contains_buffer(buffer, byte_count)) {
        return set_error(transport, SEP_RUNTIME_LIFECYCLE_ERROR_BOUNDS);
    }
    if (transport->operations.receive == NULL) {
        return set_error(transport, SEP_RUNTIME_LIFECYCLE_ERROR_CALLBACK);
    }
    status = transport->operations.receive(
        transport->operation_context,
        &transport->channel,
        (void *)buffer.pointer,
        byte_count,
        transfer_context
    );
    return set_error(transport, status);
}

int sep_runtime_transport_initialize(
    SEPRuntimeTransportLifecycleSpan span,
    const SEPRuntimeTransportLifecycleConfiguration *configuration
)
{
    SEPRuntimeTransportLifecycle *transport;
    uintptr_t address;
    uint32_t gate;
    int status = require_transport(span, &transport);

    if (status != SEP_RUNTIME_LIFECYCLE_OK) {
        return status;
    }
    if (configuration == NULL || configuration->primary_register_base == 0 ||
        configuration->layout == NULL) {
        return set_error(transport, SEP_RUNTIME_LIFECYCLE_ERROR_ARGUMENT);
    }
    if (!version_is_supported(configuration->version)) {
        return set_error(transport, SEP_RUNTIME_LIFECYCLE_ERROR_VERSION);
    }
    if (!dma_window_is_supported(configuration->dma_window_size)) {
        return set_error(transport, SEP_RUNTIME_LIFECYCLE_ERROR_DMA_WINDOW);
    }
    if (configuration->dma_ipa_base != 0 &&
        (configuration->dma_ipa_base & 0x7ffu) != 0) {
        return set_error(transport, SEP_RUNTIME_LIFECYCLE_ERROR_DMA_WINDOW);
    }

    transport->version = configuration->version;
    transport->primary_register_base = configuration->primary_register_base;
    transport->secondary_register_base = configuration->secondary_register_base;
    transport->dma_ipa_base = configuration->dma_ipa_base;
    transport->dma_window_size = configuration->dma_window_size;
    transport->layout = configuration->layout;
    transport->provider_context = configuration->provider_context;
    transport->channel_configuration[0] = configuration->channel_configuration[0];
    transport->channel_configuration[1] = configuration->channel_configuration[1];
    transport->interrupt_mask = configuration->interrupt_mask;
    transport->interrupt_register_offset = configuration->interrupt_register_offset;
    transport->gate_register_offset = configuration->gate_register_offset;
    transport->counter_low_register_offset = configuration->counter_low_register_offset;
    transport->counter_high_register_offset = configuration->counter_high_register_offset;
    transport->virtual_counter = configuration->virtual_counter;
    transport->initialize_channel = configuration->initialize_channel;
    transport->invoke_lifecycle = configuration->invoke_lifecycle;

    if (transport->operations.route == NULL ||
        transport->operations.read32 == NULL ||
        transport->operations.write32 == NULL) {
        return set_error(transport, SEP_RUNTIME_LIFECYCLE_ERROR_CALLBACK);
    }
    if (transport->operations.route(transport->operation_context, true) != 0) {
        return set_error(transport, SEP_RUNTIME_LIFECYCLE_ERROR_CALLBACK);
    }
    status = sep_runtime_transport_primary_register_address(
        span,
        transport->gate_register_offset,
        &address
    );
    if (status != SEP_RUNTIME_LIFECYCLE_OK) {
        return status;
    }
    gate = transport->operations.read32(transport->operation_context, address);
    status = write_register(transport, address, gate & ~2u);
    if (status != SEP_RUNTIME_LIFECYCLE_OK) {
        return status;
    }
    status = write_primary_register(
        span,
        transport,
        transport->counter_low_register_offset,
        (uint32_t)transport->virtual_counter
    );
    if (status != SEP_RUNTIME_LIFECYCLE_OK) {
        return status;
    }
    status = write_primary_register(
        span,
        transport,
        transport->counter_high_register_offset,
        (uint32_t)(transport->virtual_counter >> 32)
    );
    if (status != SEP_RUNTIME_LIFECYCLE_OK) {
        return status;
    }
    status = write_primary_register(span, transport, transport->gate_register_offset, gate | 2u);
    if (status != SEP_RUNTIME_LIFECYCLE_OK) {
        return status;
    }
    if (transport->initialize_channel) {
        status = sep_runtime_transport_channel_initialize(span, false);
        if (status != SEP_RUNTIME_LIFECYCLE_OK) {
            return status;
        }
    }
    status = update_interrupt_mask(span, true);
    if (status != SEP_RUNTIME_LIFECYCLE_OK) {
        return status;
    }
    if (transport->invoke_lifecycle) {
        status = sep_runtime_transport_invoke_lifecycle_hook(span, true);
        if (status != SEP_RUNTIME_LIFECYCLE_OK) {
            return status;
        }
    }
    transport->active = true;
    return set_error(transport, SEP_RUNTIME_LIFECYCLE_OK);
}

int sep_runtime_transport_teardown(SEPRuntimeTransportLifecycleSpan span)
{
    SEPRuntimeTransportLifecycle *transport;
    int status = require_transport(span, &transport);

    if (status != SEP_RUNTIME_LIFECYCLE_OK) {
        return status;
    }
    if (transport->primary_register_base == 0) {
        return set_error(transport, SEP_RUNTIME_LIFECYCLE_ERROR_STATE);
    }
    if (transport->invoke_lifecycle) {
        status = sep_runtime_transport_invoke_lifecycle_hook(span, false);
        if (status != SEP_RUNTIME_LIFECYCLE_OK) {
            return status;
        }
    }
    status = update_interrupt_mask(span, false);
    if (status != SEP_RUNTIME_LIFECYCLE_OK) {
        return status;
    }
    if (transport->operations.route == NULL ||
        transport->operations.route(transport->operation_context, false) != 0) {
        return set_error(transport, SEP_RUNTIME_LIFECYCLE_ERROR_CALLBACK);
    }
    transport->active = false;
    transport->channel.initialized = false;
    return set_error(transport, SEP_RUNTIME_LIFECYCLE_OK);
}
