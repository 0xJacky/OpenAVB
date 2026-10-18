#include "avp/devices/simplevirtio/simple_virtio_usb_async.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>


enum {
    MAX_RESPONSES = 16,
    MAX_RESPONSE_SIZE = 64,
    MAX_ORDER = 128,
};

typedef struct {
    uint8_t bytes[MAX_RESPONSE_SIZE];
    uint32_t size;
} CapturedResponse;

typedef struct {
    CapturedResponse responses[MAX_RESPONSES];
    uint32_t response_count;
    uint32_t allocation_count;
    uint32_t release_count;
    uint32_t active_allocations;
    uint32_t yield_count;
    uint32_t repost_count;
    uint32_t dispatch_count;
    uint32_t enter_count;
    uint32_t leave_count;
    uint32_t worker_exit_count;
    uint32_t join_count;
    uint32_t destroy_count;
    SimpleVirtioUSBAsyncEvent last_event;
    uint8_t last_inline[32];
    char order[MAX_ORDER];
    uint32_t order_size;
    uint8_t *event_to_poison;
    uint32_t event_to_poison_size;
} TestContext;

static void record_order(TestContext *context, char marker) {
    assert(context->order_size < MAX_ORDER);
    context->order[context->order_size++] = marker;
}

static SimpleVirtioUSBAsyncOwnedReference allocate_owned(
    void *opaque,
    size_t size
) {
    TestContext *context = opaque;
    void *pointer = malloc(size);
    assert(pointer != 0);
    memset(pointer, 0, size);
    ++context->allocation_count;
    ++context->active_allocations;
    return (SimpleVirtioUSBAsyncOwnedReference){
        .pointer = pointer,
        .lower_bound = (uintptr_t)pointer,
        .upper_bound = (uintptr_t)pointer + size,
        .cookie = (uintptr_t)pointer ^ UINT64_C(0x415650555342),
    };
}

static void release_owned(
    void *opaque,
    SimpleVirtioUSBAsyncOwnedReference reference,
    size_t size
) {
    TestContext *context = opaque;
    assert(reference.pointer != 0);
    assert(reference.lower_bound == (uintptr_t)reference.pointer);
    assert(reference.upper_bound == (uintptr_t)reference.pointer + size);
    memset(reference.pointer, 0xa5, size);
    free(reference.pointer);
    ++context->release_count;
    assert(context->active_allocations != 0);
    --context->active_allocations;
    record_order(context, 'F');
}

static void worker_yield(void *opaque) {
    TestContext *context = opaque;
    ++context->yield_count;
    record_order(context, 'Y');
}

static void queue0_repost(void *opaque) {
    TestContext *context = opaque;
    ++context->repost_count;
    record_order(context, 'R');
    if (context->event_to_poison != 0) {
        memset(
            context->event_to_poison,
            0xee,
            context->event_to_poison_size
        );
    }
}

static void queue1_submit(
    void *opaque,
    const uint8_t *bytes,
    uint32_t size
) {
    TestContext *context = opaque;
    assert(context->response_count < MAX_RESPONSES);
    assert(size <= MAX_RESPONSE_SIZE);
    CapturedResponse *response =
        &context->responses[context->response_count++];
    response->size = size;
    memcpy(response->bytes, bytes, size);
    record_order(context, 'S');
}

static void dispatch_event(
    void *opaque,
    const SimpleVirtioUSBAsyncEvent *event
) {
    TestContext *context = opaque;
    ++context->dispatch_count;
    context->last_event = *event;
    assert(event->inline_size <= sizeof(context->last_inline));
    memcpy(context->last_inline, event->inline_data, event->inline_size);
    context->last_event.inline_data = context->last_inline;
    record_order(context, 'D');
}

static void enter_critical(void *opaque) {
    TestContext *context = opaque;
    ++context->enter_count;
    record_order(context, 'C');
}

static void leave_critical(void *opaque) {
    TestContext *context = opaque;
    ++context->leave_count;
    record_order(context, 'L');
}

static void worker_exit(void *opaque) {
    TestContext *context = opaque;
    ++context->worker_exit_count;
    record_order(context, 'E');
}

static void join_worker(void *opaque) {
    TestContext *context = opaque;
    ++context->join_count;
    record_order(context, 'J');
}

static void destroy_device(void *opaque) {
    TestContext *context = opaque;
    ++context->destroy_count;
    record_order(context, 'X');
}

static const SimpleVirtioUSBAsyncOps test_ops = {
    .allocate_owned = allocate_owned,
    .release_owned = release_owned,
    .worker_yield = worker_yield,
    .queue0_repost = queue0_repost,
    .queue1_submit = queue1_submit,
    .dispatch_event = dispatch_event,
    .enter_critical = enter_critical,
    .leave_critical = leave_critical,
    .worker_exit = worker_exit,
    .join_worker = join_worker,
    .destroy_device = destroy_device,
};

static void initialize(
    SimpleVirtioUSBAsync *usb,
    TestContext *context
) {
    *context = (TestContext){0};
    simple_virtio_usb_async_initialize(usb, &test_ops, context);
}

static void assert_response(
    const TestContext *context,
    uint32_t index,
    const uint8_t *expected,
    uint32_t size
) {
    assert(index < context->response_count);
    assert(context->responses[index].size == size);
    assert(memcmp(context->responses[index].bytes, expected, size) == 0);
}

static void test_layout_and_endpoint_mapping(void) {
    SimpleVirtioUSBAsync usb;
    TestContext context;
    initialize(&usb, &context);
    assert(usb.state == SIMPLE_VIRTIO_USB_ASYNC_RUNNING);
    assert(usb.live);
    assert(simple_virtio_usb_async_endpoint_slot(0x81) == 0);
    assert(simple_virtio_usb_async_endpoint_slot(0x02) == 1);
    assert(simple_virtio_usb_async_endpoint_slot(0x85) == 2);
    assert(simple_virtio_usb_async_endpoint_slot(0x04) == 3);
    assert(simple_virtio_usb_async_endpoint_slot(0x80) == -1);
    for (uint32_t index = 0; index < 4; ++index) {
        assert(usb.completion_slots[index].head.pointer == 0);
        assert(
            usb.completion_slots[index].tail_cursor.pointer ==
            &usb.completion_slots[index].head
        );
    }
}

static void test_queue0_copy_and_pending_credit(void) {
    SimpleVirtioUSBAsync usb;
    TestContext context;
    initialize(&usb, &context);
    uint8_t event[] = {
        3, 0, 0, 0,
        0x81,
        SIMPLE_VIRTIO_USB_EVENT_TRANSFER,
        0x11, 0x22, 0x33,
    };
    context.event_to_poison = event;
    context.event_to_poison_size = sizeof(event);
    assert(simple_virtio_usb_async_worker_cycle(
        &usb,
        event,
        sizeof(event)
    ));
    assert(context.yield_count == 1);
    assert(context.repost_count == 1);
    assert(context.dispatch_count == 1);
    assert(context.last_event.declared_length == 3);
    assert(context.last_event.endpoint == 0x81);
    assert(context.last_event.type == SIMPLE_VIRTIO_USB_EVENT_TRANSFER);
    assert(context.last_event.inline_size == 3);
    assert(context.last_inline[0] == 0x11);
    assert(context.last_inline[1] == 0x22);
    assert(context.last_inline[2] == 0x33);
    assert(usb.completion_slots[0].pending_credit == 1);
    assert(context.order[0] == 'Y');
    assert(context.order[1] == 'C');
    assert(context.order[2] == 'R');
    assert(context.order[3] == 'D');
    assert(context.order[4] == 'L');

    const uint8_t payload[] = {0xaa, 0xbb};
    simple_virtio_usb_async_submit_completion(
        &usb,
        0x81,
        payload,
        sizeof(payload)
    );
    const uint8_t expected[] = {1, 0x81, 0xaa, 0xbb};
    assert_response(&context, 0, expected, sizeof(expected));
    assert(usb.completion_slots[0].pending_credit == 0);
    assert(context.allocation_count == 0);
    assert(context.enter_count == 2);
    assert(context.leave_count == 2);
}

static void test_owned_fifo_and_exactly_once_release(void) {
    SimpleVirtioUSBAsync usb;
    TestContext context;
    initialize(&usb, &context);
    const uint8_t first[] = {0x10, 0x11};
    const uint8_t second[] = {0x20, 0x21, 0x22};
    simple_virtio_usb_async_submit_completion(
        &usb,
        0x85,
        first,
        sizeof(first)
    );
    simple_virtio_usb_async_submit_completion(
        &usb,
        0x85,
        second,
        sizeof(second)
    );
    assert(context.allocation_count == 4);
    assert(context.active_allocations == 4);
    assert(simple_virtio_usb_async_queued_completion_count(&usb, 0x85) == 2);

    uint8_t event[] = {
        8, 0, 0, 0,
        0x85,
        SIMPLE_VIRTIO_USB_EVENT_TRANSFER,
    };
    assert(simple_virtio_usb_async_worker_cycle(&usb, event, sizeof(event)));
    assert(simple_virtio_usb_async_worker_cycle(&usb, event, sizeof(event)));
    const uint8_t expected_first[] = {1, 0x85, 0x10, 0x11};
    const uint8_t expected_second[] = {1, 0x85, 0x20, 0x21, 0x22};
    assert_response(&context, 0, expected_first, sizeof(expected_first));
    assert_response(&context, 1, expected_second, sizeof(expected_second));
    assert(context.dispatch_count == 0);
    assert(context.release_count == 4);
    assert(context.active_allocations == 0);
    assert(simple_virtio_usb_async_queued_completion_count(&usb, 0x85) == 0);
    assert(
        usb.completion_slots[2].tail_cursor.pointer ==
        &usb.completion_slots[2].head
    );
}

static void test_out_transfer_completion_branch(void) {
    SimpleVirtioUSBAsync usb;
    TestContext context;
    initialize(&usb, &context);
    uint8_t event[] = {
        2, 0, 0, 0,
        0x02,
        SIMPLE_VIRTIO_USB_EVENT_TRANSFER,
        0xde, 0xad,
    };
    assert(simple_virtio_usb_async_worker_cycle(
        &usb,
        event,
        sizeof(event)
    ));
    const uint8_t expected[] = {1, 0x02};
    assert_response(&context, 0, expected, sizeof(expected));
    assert(context.dispatch_count == 1);
    assert(usb.completion_slots[1].pending_credit == 1);
    assert(context.order[2] == 'R');
    assert(context.order[3] == 'D');
    assert(context.order[4] == 'S');
}

static void test_reset_and_pause_semantics(void) {
    SimpleVirtioUSBAsync usb;
    TestContext context;
    initialize(&usb, &context);
    const uint8_t queued[] = {0x31, 0x32, 0x33};
    simple_virtio_usb_async_submit_completion(
        &usb,
        0x04,
        queued,
        sizeof(queued)
    );
    usb.completion_slots[3].pending_credit = 2;
    uint8_t pause[] = {
        0, 0, 0, 0,
        0x04,
        SIMPLE_VIRTIO_USB_EVENT_PAUSE,
    };
    assert(simple_virtio_usb_async_worker_cycle(
        &usb,
        pause,
        sizeof(pause)
    ));
    const uint8_t pause_expected[] = {5, 0x04};
    assert_response(&context, 0, pause_expected, sizeof(pause_expected));
    assert(context.dispatch_count == 0);
    assert(usb.completion_slots[3].pending_credit == 0);
    assert(simple_virtio_usb_async_queued_completion_count(&usb, 0x04) == 0);
    assert(context.release_count == 2);

    uint8_t reset[] = {
        0, 0, 0, 0,
        0,
        SIMPLE_VIRTIO_USB_EVENT_RESET,
    };
    assert(simple_virtio_usb_async_worker_cycle(
        &usb,
        reset,
        sizeof(reset)
    ));
    const uint8_t reset_expected[] = {4, 0};
    assert_response(&context, 1, reset_expected, sizeof(reset_expected));
    assert(context.dispatch_count == 1);
}

static void test_shutdown_order_and_queue_drain(void) {
    SimpleVirtioUSBAsync usb;
    TestContext context;
    initialize(&usb, &context);
    const uint8_t first[] = {1};
    const uint8_t second[] = {2, 3};
    simple_virtio_usb_async_submit_completion(
        &usb,
        0x81,
        first,
        sizeof(first)
    );
    simple_virtio_usb_async_submit_completion(
        &usb,
        0x85,
        second,
        sizeof(second)
    );
    assert(context.active_allocations == 4);
    context.order_size = 0;

    simple_virtio_usb_async_request_shutdown(&usb);
    assert(usb.state == SIMPLE_VIRTIO_USB_ASYNC_STOP_REQUESTED);
    assert(!usb.live);
    assert(!simple_virtio_usb_async_worker_cycle(&usb, 0, 0));
    assert(usb.state == SIMPLE_VIRTIO_USB_ASYNC_WORKER_EXITED);
    assert(context.worker_exit_count == 1);
    simple_virtio_usb_async_finish_shutdown(&usb);
    assert(usb.state == SIMPLE_VIRTIO_USB_ASYNC_SHUTDOWN);
    assert(context.join_count == 1);
    assert(context.destroy_count == 1);
    assert(context.release_count == 4);
    assert(context.active_allocations == 0);
    assert(context.order[0] == 'Y');
    assert(context.order[1] == 'E');
    assert(context.order[2] == 'J');
    assert(context.order[3] == 'X');
    assert(context.order[4] == 'F');
}

int main(void) {
    test_layout_and_endpoint_mapping();
    test_queue0_copy_and_pending_credit();
    test_owned_fifo_and_exactly_once_release();
    test_out_transfer_completion_branch();
    test_reset_and_pause_semantics();
    test_shutdown_order_and_queue_drain();
    return 0;
}
