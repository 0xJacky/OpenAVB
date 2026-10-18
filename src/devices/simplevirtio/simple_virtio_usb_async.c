#include "avp/devices/simplevirtio/simple_virtio_usb_async.h"
#include "avp/runtime/support/recovered_symbol.h"


#define ASYNC_REQUIRE(condition) \
    do { \
        if (!(condition)) { \
            __builtin_trap(); \
        } \
    } while (0)

static const uint8_t completion_endpoints[] = {
    0x81,
    0x02,
    0x85,
    0x04,
};

static void copy_bytes(void *destination, const void *source, size_t size) {
    uint8_t *output = destination;
    const uint8_t *input = source;
    for (size_t index = 0; index < size; ++index) {
        output[index] = input[index];
    }
}

static SimpleVirtioUSBAsyncOwnedReference borrowed_reference(
    void *pointer,
    size_t size,
    uintptr_t cookie
) {
    return (SimpleVirtioUSBAsyncOwnedReference){
        .pointer = pointer,
        .lower_bound = (uintptr_t)pointer,
        .upper_bound = (uintptr_t)pointer + size,
        .cookie = cookie,
    };
}

static void require_reference(
    SimpleVirtioUSBAsyncOwnedReference reference,
    size_t size
) {
    const uintptr_t pointer = (uintptr_t)reference.pointer;
    ASYNC_REQUIRE(reference.pointer != 0);
    ASYNC_REQUIRE(reference.lower_bound <= pointer);
    ASYNC_REQUIRE(pointer <= reference.upper_bound);
    ASYNC_REQUIRE(size <= reference.upper_bound - pointer);
}

static SimpleVirtioUSBAsyncCompletionSlot *completion_slot(
    SimpleVirtioUSBAsync *usb,
    uint8_t endpoint
) {
    const int32_t index = simple_virtio_usb_async_endpoint_slot(endpoint);
    return index < 0 ? 0 : &usb->completion_slots[index];
}

static const SimpleVirtioUSBAsyncCompletionSlot *const_completion_slot(
    const SimpleVirtioUSBAsync *usb,
    uint8_t endpoint
) {
    const int32_t index = simple_virtio_usb_async_endpoint_slot(endpoint);
    return index < 0 ? 0 : &usb->completion_slots[index];
}

static void reset_slot_tail(SimpleVirtioUSBAsyncCompletionSlot *slot) {
    slot->tail_cursor = borrowed_reference(
        &slot->head,
        sizeof(slot->head),
        (uintptr_t)slot
    );
}

static void initialize_slot(
    SimpleVirtioUSBAsyncCompletionSlot *slot,
    uint8_t endpoint
) {
    *slot = (SimpleVirtioUSBAsyncCompletionSlot){
        .endpoint = endpoint,
    };
    reset_slot_tail(slot);
}

static void submit_response(
    SimpleVirtioUSBAsync *usb,
    uint8_t operation,
    uint8_t endpoint,
    const void *payload,
    uint32_t payload_size
) {
    ASYNC_REQUIRE(payload_size <= SIMPLE_VIRTIO_USB_ASYNC_QUEUE_CAPACITY - 2);
    ASYNC_REQUIRE(payload_size == 0 || payload != 0);
    usb->queue1_response_storage[0] = operation;
    usb->queue1_response_storage[1] = endpoint == 0x80 ? 0 : endpoint;
    copy_bytes(&usb->queue1_response_storage[2], payload, payload_size);
    usb->ops->queue1_submit(
        usb->ops_context,
        usb->queue1_response_storage,
        payload_size + 2
    );
}

static void enqueue_completion(
    SimpleVirtioUSBAsync *usb,
    SimpleVirtioUSBAsyncCompletionSlot *slot,
    const void *payload,
    uint32_t payload_size
) {
    SimpleVirtioUSBAsyncOwnedReference record_reference =
        usb->ops->allocate_owned(
            usb->ops_context,
            sizeof(SimpleVirtioUSBAsyncCompletionRecord)
        );
    require_reference(
        record_reference,
        sizeof(SimpleVirtioUSBAsyncCompletionRecord)
    );
    SimpleVirtioUSBAsyncCompletionRecord *record = record_reference.pointer;
    *record = (SimpleVirtioUSBAsyncCompletionRecord){
        .length = payload_size,
    };
    if (payload_size != 0) {
        record->payload = usb->ops->allocate_owned(
            usb->ops_context,
            payload_size
        );
        require_reference(record->payload, payload_size);
        copy_bytes(record->payload.pointer, payload, payload_size);
    }

    require_reference(slot->tail_cursor, sizeof(record->next));
    SimpleVirtioUSBAsyncOwnedReference *tail = slot->tail_cursor.pointer;
    ASYNC_REQUIRE(tail->pointer == 0);
    *tail = record_reference;
    slot->tail_cursor = borrowed_reference(
        &record->next,
        sizeof(record->next),
        record_reference.cookie
    );
}

static void release_record(
    SimpleVirtioUSBAsync *usb,
    SimpleVirtioUSBAsyncOwnedReference record_reference
) {
    require_reference(
        record_reference,
        sizeof(SimpleVirtioUSBAsyncCompletionRecord)
    );
    SimpleVirtioUSBAsyncCompletionRecord *record = record_reference.pointer;
    if (record->length != 0) {
        require_reference(record->payload, record->length);
        SimpleVirtioUSBAsyncOwnedReference payload = record->payload;
        record->payload = (SimpleVirtioUSBAsyncOwnedReference){0};
        usb->ops->release_owned(
            usb->ops_context,
            payload,
            record->length
        );
    }
    usb->ops->release_owned(
        usb->ops_context,
        record_reference,
        sizeof(*record)
    );
}

static SimpleVirtioUSBAsyncOwnedReference dequeue_completion(
    SimpleVirtioUSBAsyncCompletionSlot *slot
) {
    SimpleVirtioUSBAsyncOwnedReference record_reference = slot->head;
    if (record_reference.pointer == 0) {
        return record_reference;
    }
    require_reference(
        record_reference,
        sizeof(SimpleVirtioUSBAsyncCompletionRecord)
    );
    SimpleVirtioUSBAsyncCompletionRecord *record = record_reference.pointer;
    slot->head = record->next;
    record->next = (SimpleVirtioUSBAsyncOwnedReference){0};
    if (slot->head.pointer == 0) {
        reset_slot_tail(slot);
    }
    return record_reference;
}

static void submit_and_release_completion(
    SimpleVirtioUSBAsync *usb,
    SimpleVirtioUSBAsyncOwnedReference record_reference,
    uint8_t endpoint
) {
    SimpleVirtioUSBAsyncCompletionRecord *record = record_reference.pointer;
    require_reference(
        record_reference,
        sizeof(SimpleVirtioUSBAsyncCompletionRecord)
    );
    if (record->length != 0) {
        require_reference(record->payload, record->length);
    }
    submit_response(
        usb,
        SIMPLE_VIRTIO_USB_RESPONSE_TRANSFER,
        endpoint,
        record->payload.pointer,
        record->length
    );
    release_record(usb, record_reference);
}

static void drain_slot(
    SimpleVirtioUSBAsync *usb,
    SimpleVirtioUSBAsyncCompletionSlot *slot
) {
    slot->pending_credit = 0;
    for (;;) {
        SimpleVirtioUSBAsyncOwnedReference record =
            dequeue_completion(slot);
        if (record.pointer == 0) {
            return;
        }
        release_record(usb, record);
    }
}

static uint32_t read_u32_le(const uint8_t *bytes) {
    return (uint32_t)bytes[0] |
        (uint32_t)bytes[1] << 8 |
        (uint32_t)bytes[2] << 16 |
        (uint32_t)bytes[3] << 24;
}

static void dispatch_transfer_event(
    SimpleVirtioUSBAsync *usb,
    const SimpleVirtioUSBAsyncEvent *event
) {
    SimpleVirtioUSBAsyncCompletionSlot *slot =
        completion_slot(usb, event->endpoint);
    if (slot != 0) {
        SimpleVirtioUSBAsyncOwnedReference record =
            dequeue_completion(slot);
        if (record.pointer != 0) {
            submit_and_release_completion(usb, record, event->endpoint);
            return;
        }
        ASYNC_REQUIRE(slot->pending_credit != UINT16_MAX);
        ++slot->pending_credit;
    }

    usb->ops->dispatch_event(usb->ops_context, event);

    /* OUT endpoints complete after their consumer observes the copied event.
     * The pause branch has its own opcode-5 completion path. */
    if (usb->live &&
        (event->endpoint == 0x02 || event->endpoint == 0x04)) {
        submit_response(
            usb,
            SIMPLE_VIRTIO_USB_RESPONSE_TRANSFER,
            event->endpoint,
            0,
            0
        );
    }
}

static void dispatch_event(
    SimpleVirtioUSBAsync *usb,
    const SimpleVirtioUSBAsyncEvent *event
) {
    switch (event->type) {
        case SIMPLE_VIRTIO_USB_EVENT_TRANSFER:
            dispatch_transfer_event(usb, event);
            return;
        case SIMPLE_VIRTIO_USB_EVENT_RESET:
            submit_response(
                usb,
                SIMPLE_VIRTIO_USB_RESPONSE_RESET_COMPLETE,
                event->endpoint,
                0,
                0
            );
            usb->ops->dispatch_event(usb->ops_context, event);
            return;
        case SIMPLE_VIRTIO_USB_EVENT_PAUSE: {
            SimpleVirtioUSBAsyncCompletionSlot *slot =
                completion_slot(usb, event->endpoint);
            if (slot != 0) {
                drain_slot(usb, slot);
            }
            submit_response(
                usb,
                SIMPLE_VIRTIO_USB_RESPONSE_PAUSE_COMPLETE,
                event->endpoint,
                0,
                0
            );
            return;
        }
    }
    __builtin_trap();
}

AVP_RECOVERED_ENTRY int32_t simple_virtio_usb_async_endpoint_slot(
    uint8_t endpoint
) {
    for (uint32_t index = 0;
         index < SIMPLE_VIRTIO_USB_ASYNC_COMPLETION_SLOT_COUNT;
         ++index) {
        if (completion_endpoints[index] == endpoint) {
            return (int32_t)index;
        }
    }
    return -1;
}

AVP_RECOVERED_ENTRY void simple_virtio_usb_async_initialize(
    SimpleVirtioUSBAsync *usb,
    const SimpleVirtioUSBAsyncOps *ops,
    void *ops_context
) {
    ASYNC_REQUIRE(usb != 0);
    ASYNC_REQUIRE(ops != 0);
    ASYNC_REQUIRE(ops->allocate_owned != 0);
    ASYNC_REQUIRE(ops->release_owned != 0);
    ASYNC_REQUIRE(ops->worker_yield != 0);
    ASYNC_REQUIRE(ops->queue0_repost != 0);
    ASYNC_REQUIRE(ops->queue1_submit != 0);
    ASYNC_REQUIRE(ops->dispatch_event != 0);
    ASYNC_REQUIRE(ops->enter_critical != 0);
    ASYNC_REQUIRE(ops->leave_critical != 0);
    ASYNC_REQUIRE(ops->worker_exit != 0);
    ASYNC_REQUIRE(ops->join_worker != 0);
    ASYNC_REQUIRE(ops->destroy_device != 0);

    *usb = (SimpleVirtioUSBAsync){
        .ops = ops,
        .ops_context = ops_context,
        .state = SIMPLE_VIRTIO_USB_ASYNC_RUNNING,
        .live = true,
    };
    for (uint32_t index = 0;
         index < SIMPLE_VIRTIO_USB_ASYNC_COMPLETION_SLOT_COUNT;
         ++index) {
        initialize_slot(
            &usb->completion_slots[index],
            completion_endpoints[index]
        );
    }
}

AVP_RECOVERED_ENTRY bool simple_virtio_usb_async_worker_cycle(
    SimpleVirtioUSBAsync *usb,
    const uint8_t *event_bytes,
    uint32_t produced_size
) {
    ASYNC_REQUIRE(usb != 0);
    ASYNC_REQUIRE(
        usb->state == SIMPLE_VIRTIO_USB_ASYNC_RUNNING ||
        usb->state == SIMPLE_VIRTIO_USB_ASYNC_STOP_REQUESTED
    );
    usb->ops->worker_yield(usb->ops_context);
    if (!usb->live) {
        ASYNC_REQUIRE(usb->state == SIMPLE_VIRTIO_USB_ASYNC_STOP_REQUESTED);
        usb->state = SIMPLE_VIRTIO_USB_ASYNC_WORKER_EXITED;
        usb->ops->worker_exit(usb->ops_context);
        return false;
    }
    if (event_bytes == 0) {
        ASYNC_REQUIRE(produced_size == 0);
        return true;
    }
    ASYNC_REQUIRE(produced_size >= 6);
    ASYNC_REQUIRE(produced_size <= SIMPLE_VIRTIO_USB_ASYNC_QUEUE_CAPACITY);
    const uint32_t declared_length = read_u32_le(event_bytes);
    ASYNC_REQUIRE(
        declared_length <= SIMPLE_VIRTIO_USB_ASYNC_QUEUE_CAPACITY - 6
    );
    copy_bytes(usb->queue0_event_storage, event_bytes, produced_size);

    usb->ops->enter_critical(usb->ops_context);
    usb->ops->queue0_repost(usb->ops_context);
    SimpleVirtioUSBAsyncEvent event = {
        .declared_length = declared_length,
        .endpoint = usb->queue0_event_storage[4],
        .type = usb->queue0_event_storage[5],
        .inline_data = &usb->queue0_event_storage[6],
        .inline_size = produced_size - 6,
    };
    dispatch_event(usb, &event);
    usb->ops->leave_critical(usb->ops_context);
    return true;
}

AVP_RECOVERED_ENTRY void simple_virtio_usb_async_submit_completion(
    SimpleVirtioUSBAsync *usb,
    uint8_t endpoint,
    const void *payload,
    uint32_t payload_size
) {
    ASYNC_REQUIRE(usb != 0);
    ASYNC_REQUIRE(usb->state != SIMPLE_VIRTIO_USB_ASYNC_SHUTDOWN);
    ASYNC_REQUIRE(payload_size == 0 || payload != 0);
    ASYNC_REQUIRE(payload_size <= SIMPLE_VIRTIO_USB_ASYNC_QUEUE_CAPACITY - 2);

    usb->ops->enter_critical(usb->ops_context);
    SimpleVirtioUSBAsyncCompletionSlot *slot =
        completion_slot(usb, endpoint);
    const bool queued = usb->live && slot != 0;
    if (queued && slot->pending_credit == 0) {
        enqueue_completion(usb, slot, payload, payload_size);
    } else {
        if (queued) {
            --slot->pending_credit;
        }
        submit_response(
            usb,
            SIMPLE_VIRTIO_USB_RESPONSE_TRANSFER,
            endpoint,
            payload,
            payload_size
        );
    }
    usb->ops->leave_critical(usb->ops_context);
}

AVP_RECOVERED_ENTRY void simple_virtio_usb_async_request_shutdown(
    SimpleVirtioUSBAsync *usb
) {
    ASYNC_REQUIRE(usb != 0);
    ASYNC_REQUIRE(usb->state == SIMPLE_VIRTIO_USB_ASYNC_RUNNING);
    usb->live = false;
    usb->state = SIMPLE_VIRTIO_USB_ASYNC_STOP_REQUESTED;
}

AVP_RECOVERED_ENTRY void simple_virtio_usb_async_finish_shutdown(
    SimpleVirtioUSBAsync *usb
) {
    ASYNC_REQUIRE(usb != 0);
    ASYNC_REQUIRE(usb->state == SIMPLE_VIRTIO_USB_ASYNC_WORKER_EXITED);
    usb->ops->join_worker(usb->ops_context);
    usb->state = SIMPLE_VIRTIO_USB_ASYNC_JOINED;
    usb->ops->destroy_device(usb->ops_context);
    for (uint32_t index = 0;
         index < SIMPLE_VIRTIO_USB_ASYNC_COMPLETION_SLOT_COUNT;
         ++index) {
        drain_slot(usb, &usb->completion_slots[index]);
    }
    usb->state = SIMPLE_VIRTIO_USB_ASYNC_SHUTDOWN;
}

uint32_t simple_virtio_usb_async_queued_completion_count(
    const SimpleVirtioUSBAsync *usb,
    uint8_t endpoint
) {
    ASYNC_REQUIRE(usb != 0);
    const SimpleVirtioUSBAsyncCompletionSlot *slot =
        const_completion_slot(usb, endpoint);
    ASYNC_REQUIRE(slot != 0);
    uint32_t count = 0;
    SimpleVirtioUSBAsyncOwnedReference current = slot->head;
    while (current.pointer != 0) {
        require_reference(
            current,
            sizeof(SimpleVirtioUSBAsyncCompletionRecord)
        );
        const SimpleVirtioUSBAsyncCompletionRecord *record = current.pointer;
        current = record->next;
        ASYNC_REQUIRE(count != UINT32_MAX);
        ++count;
    }
    return count;
}
