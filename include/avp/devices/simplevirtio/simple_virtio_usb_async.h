#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


enum {
    SIMPLE_VIRTIO_USB_ASYNC_QUEUE_CAPACITY = 0x80e,
    SIMPLE_VIRTIO_USB_ASYNC_COMPLETION_SLOT_COUNT = 4,
};

typedef enum {
    SIMPLE_VIRTIO_USB_EVENT_TRANSFER = 1,
    SIMPLE_VIRTIO_USB_EVENT_RESET = 2,
    SIMPLE_VIRTIO_USB_EVENT_PAUSE = 3,
} SimpleVirtioUSBAsyncEventType;

typedef enum {
    SIMPLE_VIRTIO_USB_RESPONSE_TRANSFER = 1,
    SIMPLE_VIRTIO_USB_RESPONSE_STALL = 2,
    SIMPLE_VIRTIO_USB_RESPONSE_ABORT = 3,
    SIMPLE_VIRTIO_USB_RESPONSE_RESET_COMPLETE = 4,
    SIMPLE_VIRTIO_USB_RESPONSE_PAUSE_COMPLETE = 5,
} SimpleVirtioUSBAsyncResponseType;

typedef enum {
    SIMPLE_VIRTIO_USB_ASYNC_RUNNING = 0,
    SIMPLE_VIRTIO_USB_ASYNC_STOP_REQUESTED = 1,
    SIMPLE_VIRTIO_USB_ASYNC_WORKER_EXITED = 2,
    SIMPLE_VIRTIO_USB_ASYNC_JOINED = 3,
    SIMPLE_VIRTIO_USB_ASYNC_SHUTDOWN = 4,
} SimpleVirtioUSBAsyncState;

typedef struct {
    void *pointer;
    uintptr_t lower_bound;
    uintptr_t upper_bound;
    uintptr_t cookie;
} SimpleVirtioUSBAsyncOwnedReference;

_Static_assert(
    sizeof(SimpleVirtioUSBAsyncOwnedReference) == 0x20,
    "owned reference size"
);

typedef struct SimpleVirtioUSBAsyncCompletionRecord {
    SimpleVirtioUSBAsyncOwnedReference next;
    uint32_t length;
    uint32_t reserved_24;
    SimpleVirtioUSBAsyncOwnedReference payload;
} SimpleVirtioUSBAsyncCompletionRecord;

_Static_assert(
    sizeof(SimpleVirtioUSBAsyncCompletionRecord) == 0x48,
    "completion record size"
);
_Static_assert(
    offsetof(SimpleVirtioUSBAsyncCompletionRecord, length) == 0x20,
    "completion length offset"
);
_Static_assert(
    offsetof(SimpleVirtioUSBAsyncCompletionRecord, payload) == 0x28,
    "completion payload offset"
);

typedef struct {
    uint8_t endpoint;
    uint8_t reserved_01;
    uint16_t pending_credit;
    uint32_t reserved_04;
    SimpleVirtioUSBAsyncOwnedReference head;
    SimpleVirtioUSBAsyncOwnedReference tail_cursor;
} SimpleVirtioUSBAsyncCompletionSlot;

_Static_assert(
    sizeof(SimpleVirtioUSBAsyncCompletionSlot) == 0x48,
    "completion slot size"
);
_Static_assert(
    offsetof(SimpleVirtioUSBAsyncCompletionSlot, pending_credit) == 0x02,
    "pending credit offset"
);
_Static_assert(
    offsetof(SimpleVirtioUSBAsyncCompletionSlot, head) == 0x08,
    "completion head offset"
);
_Static_assert(
    offsetof(SimpleVirtioUSBAsyncCompletionSlot, tail_cursor) == 0x28,
    "completion tail offset"
);

typedef struct {
    uint32_t declared_length;
    uint8_t endpoint;
    uint8_t type;
    const uint8_t *inline_data;
    uint32_t inline_size;
} SimpleVirtioUSBAsyncEvent;

typedef struct {
    SimpleVirtioUSBAsyncOwnedReference (*allocate_owned)(
        void *context,
        size_t size
    );
    void (*release_owned)(
        void *context,
        SimpleVirtioUSBAsyncOwnedReference reference,
        size_t size
    );
    void (*worker_yield)(void *context);
    void (*queue0_repost)(void *context);
    void (*queue1_submit)(
        void *context,
        const uint8_t *bytes,
        uint32_t size
    );
    void (*dispatch_event)(
        void *context,
        const SimpleVirtioUSBAsyncEvent *event
    );
    void (*enter_critical)(void *context);
    void (*leave_critical)(void *context);
    void (*worker_exit)(void *context);
    void (*join_worker)(void *context);
    void (*destroy_device)(void *context);
} SimpleVirtioUSBAsyncOps;

typedef struct {
    const SimpleVirtioUSBAsyncOps *ops;
    void *ops_context;
    SimpleVirtioUSBAsyncCompletionSlot
        completion_slots[SIMPLE_VIRTIO_USB_ASYNC_COMPLETION_SLOT_COUNT];
    uint8_t queue0_event_storage[SIMPLE_VIRTIO_USB_ASYNC_QUEUE_CAPACITY];
    uint8_t queue1_response_storage[SIMPLE_VIRTIO_USB_ASYNC_QUEUE_CAPACITY];
    SimpleVirtioUSBAsyncState state;
    bool live;
} SimpleVirtioUSBAsync;

/**
 * Initializes the four-slot completion owner. Each slot is 0x48
 * bytes and owns a FIFO of separately allocated payload copies.
 */
void simple_virtio_usb_async_initialize(
    SimpleVirtioUSBAsync *usb,
    const SimpleVirtioUSBAsyncOps *ops,
    void *ops_context
);
int32_t simple_virtio_usb_async_endpoint_slot(uint8_t endpoint);

/**
 * Executes one worker iteration. The worker yields before inspecting live,
 * copies a completed queue-0 event, and reposts queue 0 before dispatch.
 *
 * Type 1 consumes one prequeued completion or creates pending credit. Type 1
 * on OUT endpoints 0x02 and 0x04 emits an empty opcode-1 completion after the
 * consumer callback. Type 2 emits opcode 4 before the device callback. Type 3
 * drains the endpoint FIFO and emits only opcode 5.
 */
bool simple_virtio_usb_async_worker_cycle(
    SimpleVirtioUSBAsync *usb,
    const uint8_t *event_bytes,
    uint32_t produced_size
);
void simple_virtio_usb_async_submit_completion(
    SimpleVirtioUSBAsync *usb,
    uint8_t endpoint,
    const void *payload,
    uint32_t payload_size
);

/**
 * Mirrors the SVC lifecycle as two explicit phases: request clears live; the
 * next worker cycle exits; finish joins, destroys the device, and releases all
 * remaining FIFO ownership exactly once.
 */
void simple_virtio_usb_async_request_shutdown(SimpleVirtioUSBAsync *usb);
void simple_virtio_usb_async_finish_shutdown(SimpleVirtioUSBAsync *usb);
uint32_t simple_virtio_usb_async_queued_completion_count(
    const SimpleVirtioUSBAsync *usb,
    uint8_t endpoint
);
