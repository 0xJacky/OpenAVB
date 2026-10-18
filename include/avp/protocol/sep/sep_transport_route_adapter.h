#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "avp/protocol/sep/sep_runtime_transport.h"

#define SEP_TRANSPORT_ROUTE_PUBLIC __attribute__((used, retain))

enum {
    SEP_INTERRUPT_ROUTE_COUNT = 8,
    SEP_INTERRUPT_ROUTE_UNUSED = UINT32_MAX,
    SEP_INTERRUPT_ROUTE_FORWARD_SELECTOR = 15,
    SEP_INTERRUPT_ROUTE_REVERSE_SELECTOR = 0,
    SEP_RUNTIME_DEFAULT_INTERRUPT_MASK = 0x10,
};

typedef enum {
    SEP_TRANSPORT_ROUTE_OK = 0,
    SEP_TRANSPORT_ROUTE_ERROR_ARGUMENT = -1,
    SEP_TRANSPORT_ROUTE_ERROR_BOUNDS = -2,
    SEP_TRANSPORT_ROUTE_ERROR_NOT_FOUND = -3,
    SEP_TRANSPORT_ROUTE_ERROR_CALLBACK = -4,
} SEPTransportRouteStatus;

typedef struct {
    uint32_t packed_identifier;
    uint32_t routes[SEP_INTERRUPT_ROUTE_COUNT];
} SEPInterruptRouteRecord;

_Static_assert(sizeof(SEPInterruptRouteRecord) == 36, "SEP interrupt route record size");

typedef struct {
    const SEPInterruptRouteRecord *pointer;
    const SEPInterruptRouteRecord *lower_bound;
    const SEPInterruptRouteRecord *upper_bound;
} SEPInterruptRouteSpan;

typedef int (*SEPInterruptRouteVisitor)(
    void *context,
    uint16_t target,
    uint32_t route,
    uint32_t selector
);

typedef struct {
    uint16_t target;
    uint32_t route;
    uint32_t selector;
} SEPInterruptRouteVisit;

typedef struct {
    SEPRuntimeTransportProvider provider;
    uint16_t dma_window_size;
    uint16_t reserved;
    uint32_t interrupt_mask;
    uint32_t interrupt_register_offset;
    SEPBoundsSpan channel_span;
} SEPRuntimeRouteProvider;

typedef struct {
    const SEPRuntimeRouteProvider *providers;
    size_t provider_count;
} SEPRuntimeRouteProviderRegistry;

typedef struct {
    SEPBoundsSpan channel_span;
    SEPBoundsSpan secondary_span;
    size_t byte_count;
    uintptr_t transfer_value;
} SEPRuntimeChannelCallbackArguments;

SEP_TRANSPORT_ROUTE_PUBLIC SEPInterruptRouteSpan
sep_interrupt_route_table_empty_span(void);
SEP_TRANSPORT_ROUTE_PUBLIC SEPInterruptRouteSpan
sep_interrupt_route_table_global_span(void);
SEP_TRANSPORT_ROUTE_PUBLIC SEPBoundsSpan sep_runtime_transport_layout(void);
SEP_TRANSPORT_ROUTE_PUBLIC int sep_interrupt_route_table_visit_forward(
    SEPInterruptRouteSpan table,
    uint32_t packed_identifier,
    SEPInterruptRouteVisitor visitor,
    void *context
);
SEP_TRANSPORT_ROUTE_PUBLIC int sep_interrupt_route_table_visit(
    SEPInterruptRouteSpan table,
    uint32_t packed_identifier,
    bool reverse,
    SEPInterruptRouteVisitor visitor,
    void *context
);
SEP_TRANSPORT_ROUTE_PUBLIC int sep_interrupt_route_forward_visitor(
    void *context,
    uint16_t target,
    uint32_t route
);
SEP_TRANSPORT_ROUTE_PUBLIC int sep_interrupt_route_table_visit_reverse(
    SEPInterruptRouteSpan table,
    uint32_t packed_identifier,
    SEPInterruptRouteVisitor visitor,
    void *context
);
SEP_TRANSPORT_ROUTE_PUBLIC int sep_interrupt_route_reverse_visitor(
    void *context,
    uint16_t target,
    uint32_t route
);
SEP_TRANSPORT_ROUTE_PUBLIC int
sep_runtime_transport_prepare_channel_callback_arguments(
    SEPBoundsSpan channel_span,
    SEPBoundsSpan secondary_span,
    size_t byte_count,
    uintptr_t transfer_value,
    SEPRuntimeChannelCallbackArguments *arguments
);
SEP_TRANSPORT_ROUTE_PUBLIC SEPBoundsSpan
sep_runtime_transport_outlined_save_secondary_span_x22_x19(
    SEPBoundsSpan secondary_span
);
SEP_TRANSPORT_ROUTE_PUBLIC int
sep_runtime_transport_outlined_prepare_register_address(
    const SEPRuntimeRouteProvider *provider,
    uint16_t *type,
    uint32_t *register_offset
);
SEP_TRANSPORT_ROUTE_PUBLIC SEPBoundsSpan
sep_runtime_transport_outlined_restore_channel_tuple_x20_x23(
    const SEPRuntimeRouteProvider *provider
);
SEP_TRANSPORT_ROUTE_PUBLIC int
sep_runtime_transport_interrupt_mask_value_outliner(
    const SEPRuntimeRouteProvider *provider,
    uint32_t *interrupt_mask
);
SEP_TRANSPORT_ROUTE_PUBLIC bool sep_runtime_transport_dma_window_size_supported(
    const SEPRuntimeRouteProvider *provider
);
SEP_TRANSPORT_ROUTE_PUBLIC SEPBoundsSpan sep_runtime_transport_layout_view(void);
SEP_TRANSPORT_ROUTE_PUBLIC int sep_runtime_transport_object_lookup(
    const SEPRuntimeRouteProviderRegistry *registry,
    uint16_t type,
    SEPBoundsSpan *provider_span
);
SEP_TRANSPORT_ROUTE_PUBLIC int sep_runtime_transport_object_require(
    const SEPRuntimeRouteProviderRegistry *registry,
    uint16_t type,
    SEPBoundsSpan *provider_span
);
