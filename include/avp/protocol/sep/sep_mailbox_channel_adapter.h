#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "avp/protocol/sep/sep_runtime_transport.h"

#define SEP_MAILBOX_CHANNEL_PUBLIC __attribute__((used, retain))

enum {
    SEP_MAILBOX_SLOT_SIZE = 16,
    SEP_MAILBOX_SEND_BUSY = 1u << 16,
    SEP_MAILBOX_RECEIVE_EMPTY = 1u << 17,
    SEP_MAILBOX_CONTROL_ERROR_MASK = 3u << 18,
    SEP_MAILBOX_OWNED = 1u << 31,
    SEP_MAILBOX_ACTIVE = 1u,
    SEP_MAILBOX_CHANNEL_MAGIC = 0x1111,
};

typedef enum {
    SEP_MAILBOX_OK = 0,
    SEP_MAILBOX_ERROR_STATE = -1,
    SEP_MAILBOX_ERROR_TIMEOUT = -2,
    SEP_MAILBOX_ERROR_BOUNDS = -3,
    SEP_MAILBOX_ERROR_ARGUMENT = -4,
    SEP_MAILBOX_ERROR_CALLBACK = -5,
    SEP_MAILBOX_ERROR_IO = -6,
} SEPMailboxStatus;

typedef struct {
    uint64_t words[2];
} SEPMailboxSlot;

_Static_assert(sizeof(SEPMailboxSlot) == SEP_MAILBOX_SLOT_SIZE, "SEP mailbox slot size");

typedef struct {
    SEPMailboxSlot slot;
    uint32_t active_flags;
    uint32_t control_word;
} SEPMailboxOwnedSlot;

_Static_assert(offsetof(SEPMailboxOwnedSlot, active_flags) == 0x10, "active flag offset");
_Static_assert(offsetof(SEPMailboxOwnedSlot, control_word) == 0x14, "control word offset");

typedef struct SEPRuntimeMailboxChannel SEPRuntimeMailboxChannel;

typedef int (*SEPMailboxChannelInitializer)(
    void *context,
    SEPRuntimeMailboxChannel *channel,
    const void *lower_bound,
    const void *upper_bound
);
typedef uint32_t (*SEPMailboxRead32)(void *context, uintptr_t address);
typedef int (*SEPMailboxWrite32)(void *context, uintptr_t address, uint32_t value);
typedef int (*SEPMailboxWriteSlot)(
    void *context,
    uintptr_t address,
    const SEPMailboxSlot *slot
);
typedef int (*SEPMailboxReadSlot)(
    void *context,
    uintptr_t address,
    SEPMailboxSlot *slot
);
typedef uint64_t (*SEPMailboxReadTime)(void *context);
typedef bool (*SEPMailboxTimeoutElapsed)(
    void *context,
    uint64_t start,
    uint32_t timeout
);
typedef void (*SEPMailboxDiagnostic)(
    void *context,
    uint64_t source_token,
    uint32_t line
);

typedef struct {
    SEPMailboxChannelInitializer initialize;
    SEPMailboxRead32 read32;
    SEPMailboxWrite32 write32;
    SEPMailboxWriteSlot write_slot;
    SEPMailboxReadSlot read_slot;
    SEPMailboxReadTime read_time;
    SEPMailboxTimeoutElapsed timeout_elapsed;
    SEPMailboxDiagnostic diagnostic;
} SEPMailboxOperations;

struct SEPRuntimeMailboxChannel {
    uintptr_t register_base;
    uintptr_t provider_context;
    SEPBoundsSpan operations_span;
    SEPBoundsSpan layout_span;
    uint32_t configuration[2];
    uintptr_t route_state[5];
    uintptr_t preserved_route_field;
    const SEPRuntimeTransportLayout *layout;
    const SEPMailboxOperations *operations;
    void *operation_context;
    bool initialized;
};

typedef struct {
    SEPRuntimeMailboxChannel *pointer;
    const void *lower_bound;
    const void *upper_bound;
} SEPRuntimeMailboxChannelSpan;

typedef struct {
    uintptr_t register_base;
    uintptr_t provider_context;
    SEPBoundsSpan operations_span;
    SEPBoundsSpan layout_span;
    uint32_t configuration[2];
    const SEPRuntimeTransportLayout *layout;
    const SEPMailboxOperations *operations;
    void *operation_context;
    bool clear_route_state;
} SEPRuntimeMailboxChannelConfiguration;

typedef struct {
    SEPBoundsSpan context_span;
    SEPBoundsSpan slot_span;
    uint32_t slot_size;
    uint32_t timeout;
} SEPMailboxSavedSpans;

SEP_MAILBOX_CHANNEL_PUBLIC int sep_runtime_transport_read_receive_control(
    SEPRuntimeMailboxChannelSpan channel,
    uint32_t *control
);
SEP_MAILBOX_CHANNEL_PUBLIC int sep_clear_receive_slot(SEPBoundsSpan slot_span);
SEP_MAILBOX_CHANNEL_PUBLIC int sep_runtime_transport_channel_construct(
    SEPRuntimeMailboxChannelSpan channel,
    const SEPRuntimeMailboxChannelConfiguration *configuration
);
SEP_MAILBOX_CHANNEL_PUBLIC SEPRuntimeMailboxChannelSpan
sep_runtime_transport_outlined_identity_channel_span(
    SEPRuntimeMailboxChannelSpan channel
);
SEP_MAILBOX_CHANNEL_PUBLIC int sep_mailbox_slot_mark_active(
    SEPBoundsSpan owned_slot_span
);
SEP_MAILBOX_CHANNEL_PUBLIC int sep_mailbox_send_slot(
    SEPRuntimeMailboxChannelSpan channel,
    SEPBoundsSpan slot_span,
    uint32_t slot_size,
    uint32_t timeout
);
SEP_MAILBOX_CHANNEL_PUBLIC int sep_mailbox_receive_slot(
    SEPRuntimeMailboxChannelSpan channel,
    SEPBoundsSpan slot_span,
    uint32_t slot_size,
    uint32_t timeout
);
SEP_MAILBOX_CHANNEL_PUBLIC void sep_mailbox_method_slot3_noop(void);
SEP_MAILBOX_CHANNEL_PUBLIC int sep_mailbox_validate_slot_span(
    SEPBoundsSpan slot_span
);
SEP_MAILBOX_CHANNEL_PUBLIC uint64_t sep_mailbox_source_token(void);
SEP_MAILBOX_CHANNEL_PUBLIC int sep_mailbox_outlined_restore_io_spans_x22_x26(
    const SEPMailboxSavedSpans *saved,
    SEPBoundsSpan *context_span,
    SEPBoundsSpan *slot_span
);
SEP_MAILBOX_CHANNEL_PUBLIC void sep_mailbox_log_timeout_source_line(
    const SEPRuntimeMailboxChannel *channel,
    uint32_t line
);
SEP_MAILBOX_CHANNEL_PUBLIC bool
sep_mailbox_outlined_save_slot_span_x23_x26_and_test_timeout(
    SEPMailboxSavedSpans *saved,
    SEPBoundsSpan slot_span,
    uint32_t timeout
);
SEP_MAILBOX_CHANNEL_PUBLIC int
sep_mailbox_outlined_save_context_and_validate_slot_span(
    SEPMailboxSavedSpans *saved,
    SEPBoundsSpan context_span,
    SEPBoundsSpan slot_span,
    uint32_t slot_size
);
SEP_MAILBOX_CHANNEL_PUBLIC int sep_runtime_transport_channel_clear_route_state(
    SEPRuntimeMailboxChannelSpan channel
);
SEP_MAILBOX_CHANNEL_PUBLIC int sep_runtime_transport_channel_bounds_panic(
    SEPRuntimeMailboxChannelSpan channel
);
