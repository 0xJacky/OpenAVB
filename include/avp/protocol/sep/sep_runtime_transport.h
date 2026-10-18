#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


enum {
    SEP_RUNTIME_TRANSPORT_RECEIVE_FULL = 1u << 16,
    SEP_RUNTIME_TRANSPORT_RECEIVE_EMPTY = 1u << 17,
};

typedef enum {
    SEP_RUNTIME_TRANSPORT_RESPONSE_READY,
    SEP_RUNTIME_TRANSPORT_WAITING,
    SEP_RUNTIME_TRANSPORT_TIMED_OUT,
} SEPRuntimeTransportPoll;

typedef struct {
    uint32_t send_control_offset;
    uint32_t receive_control_offset;
    uint32_t send_slot_offset;
    uint32_t receive_slot_offset;
    uint32_t initialization_control_offset;
    uint32_t unknown_offset_20;
    uint32_t unknown_offset_24;
} SEPRuntimeTransportLayout;

_Static_assert(
    sizeof(SEPRuntimeTransportLayout) == 28,
    "SEP runtime transport layout size"
);

typedef struct {
    const void *pointer;
    const void *lower_bound;
    const void *upper_bound;
    uintptr_t context;
} SEPBoundsSpan;

typedef struct {
    volatile uint8_t *register_base;
    uintptr_t provider_auxiliary;
    SEPBoundsSpan operations;
    uintptr_t initialization_configuration;
    uintptr_t reserved_56;
    SEPBoundsSpan layout;
    uintptr_t runtime_field;
    uintptr_t reserved_104;
    uintptr_t reserved_state[4];
} SEPRuntimeTransportView;

_Static_assert(
    offsetof(SEPRuntimeTransportView, layout) == 0x40,
    "SEP runtime transport layout span offset"
);
_Static_assert(
    sizeof(SEPRuntimeTransportView) == 0x90,
    "SEP runtime transport view size"
);

typedef struct {
    uint32_t type;
    uint32_t reserved_4;
    volatile uint8_t *register_base;
    uint8_t reserved_16[88];
    uintptr_t auxiliary;
    uint8_t reserved_112[312];
} SEPRuntimeTransportProvider;

_Static_assert(
    offsetof(SEPRuntimeTransportProvider, register_base) == 0x08,
    "SEP runtime provider register base offset"
);

typedef struct {
    const SEPRuntimeTransportProvider *providers;
    size_t provider_count;
    SEPBoundsSpan layout;
} SEPRuntimeTransportProfile;
_Static_assert(
    offsetof(SEPRuntimeTransportProvider, auxiliary) == 0x68,
    "SEP runtime provider auxiliary offset"
);
_Static_assert(
    sizeof(SEPRuntimeTransportProvider) == 0x1a8,
    "SEP runtime provider size"
);

bool sep_runtime_transport_can_submit(uint32_t flags);
SEPRuntimeTransportPoll sep_runtime_transport_poll(
    uint32_t flags,
    bool timeout_elapsed
);
bool sep_runtime_transport_prepare_view(
    const SEPRuntimeTransportProfile *profile,
    uint16_t provider_type,
    SEPRuntimeTransportView *view
);
bool sep_runtime_transport_view_is_valid(
    const SEPRuntimeTransportView *view
);
const SEPRuntimeTransportLayout *sep_runtime_transport_view_layout(
    const SEPRuntimeTransportView *view
);
const SEPRuntimeTransportProfile *runtime_profile_sep_transport(void);
