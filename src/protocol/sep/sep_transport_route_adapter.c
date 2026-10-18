#include "avp/protocol/sep/sep_transport_route_adapter.h"


static const SEPRuntimeTransportLayout runtime_transport_layout = {
    .send_control_offset = 0x20,
    .receive_control_offset = 0x1c,
    .send_slot_offset = 0x200,
    .receive_slot_offset = 0x100,
    .initialization_control_offset = 0x18,
    .unknown_offset_20 = 0,
    .unknown_offset_24 = 0,
};

static const SEPInterruptRouteRecord default_route_table[] = {
    {
        .packed_identifier = 0,
        .routes = {
            SEP_INTERRUPT_ROUTE_UNUSED,
            SEP_INTERRUPT_ROUTE_UNUSED,
            SEP_INTERRUPT_ROUTE_UNUSED,
            SEP_INTERRUPT_ROUTE_UNUSED,
            SEP_INTERRUPT_ROUTE_UNUSED,
            SEP_INTERRUPT_ROUTE_UNUSED,
            SEP_INTERRUPT_ROUTE_UNUSED,
            SEP_INTERRUPT_ROUTE_UNUSED,
        },
    },
};

static bool byte_span_contains(SEPBoundsSpan span, size_t size)
{
    uintptr_t pointer = (uintptr_t)span.pointer;
    uintptr_t lower = (uintptr_t)span.lower_bound;
    uintptr_t upper = (uintptr_t)span.upper_bound;

    return span.pointer != NULL && lower <= pointer && pointer < upper &&
        size <= upper - pointer;
}

static bool route_span_is_valid(SEPInterruptRouteSpan span)
{
    return span.pointer != NULL && span.lower_bound <= span.pointer &&
        span.pointer <= span.upper_bound;
}

SEPInterruptRouteSpan sep_interrupt_route_table_empty_span(void)
{
    return (SEPInterruptRouteSpan){
        .pointer = default_route_table,
        .lower_bound = default_route_table,
        .upper_bound = default_route_table,
    };
}

SEPInterruptRouteSpan sep_interrupt_route_table_global_span(void)
{
    return (SEPInterruptRouteSpan){
        .pointer = default_route_table,
        .lower_bound = default_route_table,
        .upper_bound = default_route_table + 1,
    };
}

SEPBoundsSpan sep_runtime_transport_layout(void)
{
    return (SEPBoundsSpan){
        .pointer = &runtime_transport_layout,
        .lower_bound = &runtime_transport_layout,
        .upper_bound = (const uint8_t *)&runtime_transport_layout +
            sizeof(runtime_transport_layout),
        .context = 0,
    };
}

SEPBoundsSpan sep_runtime_transport_layout_view(void)
{
    return sep_runtime_transport_layout();
}

static const SEPInterruptRouteRecord *find_record(
    SEPInterruptRouteSpan table,
    uint32_t packed_identifier
)
{
    const SEPInterruptRouteRecord *record;

    for (record = table.pointer; record < table.upper_bound; ++record) {
        if (record->packed_identifier == packed_identifier) {
            return record;
        }
    }
    return NULL;
}

int sep_interrupt_route_table_visit(
    SEPInterruptRouteSpan table,
    uint32_t packed_identifier,
    bool reverse,
    SEPInterruptRouteVisitor visitor,
    void *context
)
{
    const SEPInterruptRouteRecord *record;
    uint16_t target = (uint16_t)(packed_identifier >> 16);
    size_t step;

    if (!route_span_is_valid(table)) {
        return SEP_TRANSPORT_ROUTE_ERROR_BOUNDS;
    }
    if (visitor == NULL) {
        return SEP_TRANSPORT_ROUTE_ERROR_ARGUMENT;
    }
    record = find_record(table, packed_identifier);
    if (record == NULL) {
        return SEP_TRANSPORT_ROUTE_ERROR_NOT_FOUND;
    }
    for (step = 0; step < SEP_INTERRUPT_ROUTE_COUNT; ++step) {
        size_t index = reverse
            ? SEP_INTERRUPT_ROUTE_COUNT - 1 - step
            : step;
        uint32_t route = record->routes[index];
        uint32_t selector = reverse
            ? SEP_INTERRUPT_ROUTE_REVERSE_SELECTOR
            : SEP_INTERRUPT_ROUTE_FORWARD_SELECTOR;

        if (route != SEP_INTERRUPT_ROUTE_UNUSED && visitor(
                context,
                target,
                route,
                selector
            ) != 0) {
            return SEP_TRANSPORT_ROUTE_ERROR_CALLBACK;
        }
    }
    return SEP_TRANSPORT_ROUTE_OK;
}

int sep_interrupt_route_table_visit_forward(
    SEPInterruptRouteSpan table,
    uint32_t packed_identifier,
    SEPInterruptRouteVisitor visitor,
    void *context
)
{
    return sep_interrupt_route_table_visit(
        table,
        packed_identifier,
        false,
        visitor,
        context
    );
}

int sep_interrupt_route_table_visit_reverse(
    SEPInterruptRouteSpan table,
    uint32_t packed_identifier,
    SEPInterruptRouteVisitor visitor,
    void *context
)
{
    return sep_interrupt_route_table_visit(
        table,
        packed_identifier,
        true,
        visitor,
        context
    );
}

int sep_interrupt_route_forward_visitor(
    void *context,
    uint16_t target,
    uint32_t route
)
{
    SEPInterruptRouteVisit *visit = context;

    if (visit == NULL) {
        return SEP_TRANSPORT_ROUTE_ERROR_ARGUMENT;
    }
    visit->target = target;
    visit->route = route;
    visit->selector = SEP_INTERRUPT_ROUTE_FORWARD_SELECTOR;
    return SEP_TRANSPORT_ROUTE_OK;
}

int sep_interrupt_route_reverse_visitor(
    void *context,
    uint16_t target,
    uint32_t route
)
{
    SEPInterruptRouteVisit *visit = context;

    if (visit == NULL) {
        return SEP_TRANSPORT_ROUTE_ERROR_ARGUMENT;
    }
    visit->target = target;
    visit->route = route;
    visit->selector = SEP_INTERRUPT_ROUTE_REVERSE_SELECTOR;
    return SEP_TRANSPORT_ROUTE_OK;
}

int sep_runtime_transport_prepare_channel_callback_arguments(
    SEPBoundsSpan channel_span,
    SEPBoundsSpan secondary_span,
    size_t byte_count,
    uintptr_t transfer_value,
    SEPRuntimeChannelCallbackArguments *arguments
)
{
    if (arguments == NULL || !byte_span_contains(channel_span, 1) ||
        !byte_span_contains(secondary_span, byte_count)) {
        return SEP_TRANSPORT_ROUTE_ERROR_BOUNDS;
    }
    arguments->channel_span = channel_span;
    arguments->secondary_span = secondary_span;
    arguments->byte_count = byte_count;
    arguments->transfer_value = transfer_value;
    return SEP_TRANSPORT_ROUTE_OK;
}

SEPBoundsSpan sep_runtime_transport_outlined_save_secondary_span_x22_x19(
    SEPBoundsSpan secondary_span
)
{
    return secondary_span;
}

int sep_runtime_transport_outlined_prepare_register_address(
    const SEPRuntimeRouteProvider *provider,
    uint16_t *type,
    uint32_t *register_offset
)
{
    if (provider == NULL || type == NULL || register_offset == NULL) {
        return SEP_TRANSPORT_ROUTE_ERROR_ARGUMENT;
    }
    *type = (uint16_t)provider->provider.type;
    *register_offset = provider->interrupt_register_offset;
    return SEP_TRANSPORT_ROUTE_OK;
}

SEPBoundsSpan sep_runtime_transport_outlined_restore_channel_tuple_x20_x23(
    const SEPRuntimeRouteProvider *provider
)
{
    if (provider == NULL) {
        return (SEPBoundsSpan){0};
    }
    return provider->channel_span;
}

int sep_runtime_transport_interrupt_mask_value_outliner(
    const SEPRuntimeRouteProvider *provider,
    uint32_t *interrupt_mask
)
{
    if (provider == NULL || interrupt_mask == NULL) {
        return SEP_TRANSPORT_ROUTE_ERROR_ARGUMENT;
    }
    *interrupt_mask = provider->interrupt_mask == 0
        ? SEP_RUNTIME_DEFAULT_INTERRUPT_MASK
        : provider->interrupt_mask;
    return SEP_TRANSPORT_ROUTE_OK;
}

bool sep_runtime_transport_dma_window_size_supported(
    const SEPRuntimeRouteProvider *provider
)
{
    return provider != NULL &&
        (provider->dma_window_size == 0x20 || provider->dma_window_size == 0x40);
}

int sep_runtime_transport_object_lookup(
    const SEPRuntimeRouteProviderRegistry *registry,
    uint16_t type,
    SEPBoundsSpan *provider_span
)
{
    size_t index;

    if (registry == NULL || provider_span == NULL ||
        (registry->provider_count != 0 && registry->providers == NULL)) {
        return SEP_TRANSPORT_ROUTE_ERROR_ARGUMENT;
    }
    for (index = 0; index < registry->provider_count; ++index) {
        const SEPRuntimeRouteProvider *provider = &registry->providers[index];

        if ((uint16_t)provider->provider.type == type) {
            *provider_span = (SEPBoundsSpan){
                .pointer = provider,
                .lower_bound = registry->providers,
                .upper_bound = registry->providers + registry->provider_count,
                .context = type,
            };
            return SEP_TRANSPORT_ROUTE_OK;
        }
    }
    *provider_span = (SEPBoundsSpan){0};
    return SEP_TRANSPORT_ROUTE_ERROR_NOT_FOUND;
}

int sep_runtime_transport_object_require(
    const SEPRuntimeRouteProviderRegistry *registry,
    uint16_t type,
    SEPBoundsSpan *provider_span
)
{
    return sep_runtime_transport_object_lookup(registry, type, provider_span);
}
