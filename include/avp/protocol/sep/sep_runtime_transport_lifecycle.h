#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "avp/protocol/sep/sep_runtime_transport.h"

enum {
    SEP_RUNTIME_TRANSPORT_VERSION_1_0_1 = 0x0101,
    SEP_RUNTIME_TRANSPORT_VERSION_2_0_0 = 0x0200,
    SEP_RUNTIME_TRANSPORT_DEFAULT_INTERRUPT_MASK = 0x10,
    SEP_RUNTIME_TRANSPORT_DMA_SLOT_OFFSET = 0x50000,
};

typedef enum {
    SEP_RUNTIME_LIFECYCLE_OK = 0,
    SEP_RUNTIME_LIFECYCLE_ERROR_ARGUMENT = -1,
    SEP_RUNTIME_LIFECYCLE_ERROR_BOUNDS = -2,
    SEP_RUNTIME_LIFECYCLE_ERROR_VERSION = -3,
    SEP_RUNTIME_LIFECYCLE_ERROR_DMA_WINDOW = -4,
    SEP_RUNTIME_LIFECYCLE_ERROR_STATE = -5,
    SEP_RUNTIME_LIFECYCLE_ERROR_CALLBACK = -6,
    SEP_RUNTIME_LIFECYCLE_ERROR_IO = -7,
} SEPRuntimeTransportLifecycleStatus;

typedef struct SEPRuntimeTransportLifecycle SEPRuntimeTransportLifecycle;

typedef struct {
    SEPRuntimeTransportLifecycle *pointer;
    const void *lower_bound;
    const void *upper_bound;
} SEPRuntimeTransportLifecycleSpan;

typedef struct {
    uintptr_t register_base;
    const SEPRuntimeTransportLayout *layout;
    uintptr_t provider_context;
    uint32_t configuration[2];
    bool route_state;
    bool initialized;
} SEPRuntimeTransportChannel;

typedef int (*SEPRuntimeTransportChannelInitialize)(
    void *context,
    SEPRuntimeTransportChannel *channel
);
typedef int (*SEPRuntimeTransportLifecycleHook)(
    void *context,
    SEPRuntimeTransportLifecycle *transport,
    bool active
);
typedef int (*SEPRuntimeTransportRouteHook)(void *context, bool install);
typedef uint32_t (*SEPRuntimeTransportRead32)(void *context, uintptr_t address);
typedef int (*SEPRuntimeTransportWrite32)(
    void *context,
    uintptr_t address,
    uint32_t value
);
typedef int (*SEPRuntimeTransportSendHook)(
    void *context,
    SEPRuntimeTransportChannel *channel,
    const void *buffer,
    size_t byte_count,
    uint32_t flags
);
typedef int (*SEPRuntimeTransportReceiveHook)(
    void *context,
    SEPRuntimeTransportChannel *channel,
    void *buffer,
    size_t byte_count,
    uintptr_t transfer_context
);

typedef struct {
    SEPRuntimeTransportChannelInitialize channel_initialize;
    SEPRuntimeTransportLifecycleHook lifecycle;
    SEPRuntimeTransportRouteHook route;
    SEPRuntimeTransportRead32 read32;
    SEPRuntimeTransportWrite32 write32;
    SEPRuntimeTransportSendHook send;
    SEPRuntimeTransportReceiveHook receive;
} SEPRuntimeTransportLifecycleOperations;

typedef struct {
    uint16_t version;
    uintptr_t primary_register_base;
    uintptr_t secondary_register_base;
    uintptr_t dma_ipa_base;
    uint16_t dma_window_size;
    const SEPRuntimeTransportLayout *layout;
    uintptr_t provider_context;
    uint32_t channel_configuration[2];
    uint32_t interrupt_mask;
    uint32_t interrupt_register_offset;
    uint32_t gate_register_offset;
    uint32_t counter_low_register_offset;
    uint32_t counter_high_register_offset;
    uint64_t virtual_counter;
    bool initialize_channel;
    bool invoke_lifecycle;
} SEPRuntimeTransportLifecycleConfiguration;

struct SEPRuntimeTransportLifecycle {
    uint16_t version;
    uintptr_t primary_register_base;
    uintptr_t secondary_register_base;
    uintptr_t dma_ipa_base;
    uint16_t dma_window_size;
    const SEPRuntimeTransportLayout *layout;
    uintptr_t provider_context;
    uint32_t channel_configuration[2];
    uint32_t interrupt_mask;
    uint32_t interrupt_register_offset;
    uint32_t gate_register_offset;
    uint32_t counter_low_register_offset;
    uint32_t counter_high_register_offset;
    uint64_t virtual_counter;
    bool initialize_channel;
    bool invoke_lifecycle;
    bool active;
    int last_error;
    SEPRuntimeTransportChannel channel;
    SEPRuntimeTransportLifecycleOperations operations;
    void *operation_context;
};

#define SEP_RUNTIME_LIFECYCLE_PUBLIC __attribute__((used, retain))

SEP_RUNTIME_LIFECYCLE_PUBLIC int sep_runtime_transport_initialize(
    SEPRuntimeTransportLifecycleSpan span,
    const SEPRuntimeTransportLifecycleConfiguration *configuration
);
SEP_RUNTIME_LIFECYCLE_PUBLIC int sep_runtime_transport_dma_ipa_slot(
    SEPRuntimeTransportLifecycleSpan span,
    uintptr_t *slot
);
SEP_RUNTIME_LIFECYCLE_PUBLIC int sep_runtime_transport_channel_initialize(
    SEPRuntimeTransportLifecycleSpan span,
    bool route_state
);
SEP_RUNTIME_LIFECYCLE_PUBLIC int sep_runtime_transport_interrupt_mask_value(
    SEPRuntimeTransportLifecycleSpan span,
    uint32_t *mask
);
SEP_RUNTIME_LIFECYCLE_PUBLIC int
sep_runtime_transport_interrupt_mask_bounds_panic(
    SEPRuntimeTransportLifecycleSpan span
);
SEP_RUNTIME_LIFECYCLE_PUBLIC int
sep_runtime_transport_primary_register_address(
    SEPRuntimeTransportLifecycleSpan span,
    uint32_t offset,
    uintptr_t *address
);
SEP_RUNTIME_LIFECYCLE_PUBLIC int
sep_runtime_transport_secondary_register_address(
    SEPRuntimeTransportLifecycleSpan span,
    uint32_t offset,
    uintptr_t *address
);
SEP_RUNTIME_LIFECYCLE_PUBLIC int sep_runtime_transport_invoke_lifecycle_hook(
    SEPRuntimeTransportLifecycleSpan span,
    bool active
);
SEP_RUNTIME_LIFECYCLE_PUBLIC int sep_runtime_transport_receive(
    SEPRuntimeTransportLifecycleSpan span,
    SEPBoundsSpan buffer,
    size_t byte_count,
    uintptr_t transfer_context
);
SEP_RUNTIME_LIFECYCLE_PUBLIC int sep_runtime_transport_send(
    SEPRuntimeTransportLifecycleSpan span,
    SEPBoundsSpan buffer,
    size_t byte_count,
    uint32_t flags
);
SEP_RUNTIME_LIFECYCLE_PUBLIC int sep_runtime_transport_teardown(
    SEPRuntimeTransportLifecycleSpan span
);
