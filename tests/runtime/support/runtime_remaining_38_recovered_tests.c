#include "avp/image/boot/boot_image_sources_recovered.h"
#include "avp/runtime/ipc/runtime_endpoint_recovered.h"
#include "avp/runtime/firebloom/runtime_firebloom_remaining_recovered.h"
#include "avp/runtime/task/runtime_task_remaining_recovered.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const uint8_t *bytes;
    size_t size;
    size_t calls;
} MemoryReader;

static BootImageRecoveredStatus memory_read(void *context, size_t offset,
                                            void *destination, size_t size) {
    MemoryReader *reader = context;
    ++reader->calls;
    if (offset > reader->size || size > reader->size - offset) {
        return BOOT_IMAGE_RECOVERED_IO;
    }
    memcpy(destination, reader->bytes + offset, size);
    return BOOT_IMAGE_RECOVERED_OK;
}

typedef struct {
    BootImageRecoveredProperty property;
    size_t calls;
} PropertyCapture;

static BootImageRecoveredStatus capture_property(
    void *context, const BootImageRecoveredProperty *property
) {
    PropertyCapture *capture = context;
    capture->property = *property;
    ++capture->calls;
    return BOOT_IMAGE_RECOVERED_OK;
}

static void test_boot_image_owner(void) {
    static const uint8_t im4p[] = {
        0x30, 0x13, 0x16, 0x04, 'I', 'M', '4', 'P',
        0x16, 0x04, 'i', 'b', 's', 's', 0x16, 0x00,
        0x04, 0x03, 'a', 'b', 'c',
    };
    BootImageRecoveredRegistry registry;
    boot_image_recovered_registry_initialize(&registry);
    BootImageRecoveredSource *memory_source = NULL;
    assert(boot_image_create_memory_source(
               &registry, im4p, sizeof(im4p), AVP_BOOT_IMAGE_CONTAINER_IMG4,
               0u, &memory_source) == BOOT_IMAGE_RECOVERED_OK);
    assert(memory_source->image_type == AVP_BOOT_IMAGE_FOURCC('i', 'b', 's', 's'));
    assert(memory_source->view.payload_size == 3u);
    assert(boot_image_source_lookup_by_type(
               &registry, AVP_BOOT_IMAGE_FOURCC('i', 'b', 's', 's')) ==
           memory_source);
    assert(boot_image_source_lookup_by_type_thunk(
               &registry, AVP_BOOT_IMAGE_FOURCC('i', 'b', 's', 's')) ==
           memory_source);

    uint8_t scratch[sizeof(im4p)];
    MemoryReader reader = {.bytes = im4p, .size = sizeof(im4p)};
    BootImageRecoveredSource *provider_source = NULL;
    assert(boot_image_register_provider_record_from_header(
               &registry, memory_read, &reader, 0u, sizeof(im4p), scratch,
               sizeof(scratch), &provider_source) == BOOT_IMAGE_RECOVERED_OK);
    assert(reader.calls == 2u);
    assert(provider_source->loaded);

    static const uint8_t payload_with_properties[] = {
        0x30, 0x36, 0x16, 0x04, 'I', 'M', '4', 'P',
        0x16, 0x04, 'i', 'b', 's', 's', 0x16, 0x04, '1', '.', '0', 0,
        0x04, 0x03, 'a', 'b', 'c', 0xa0, 0x1d,
        0x30, 0x1b, 0x16, 0x04, 'P', 'A', 'Y', 'P', 0x31, 0x13,
        0xff, 0x86, 0xf3, 0xbd, 0xdc, 0x63, 0x0c,
        0x30, 0x0a, 0x16, 0x04, 'n', 'o', 'n', 'c',
        0x02, 0x02, 0x01, 0x23,
    };
    img4_decode_context_t image = {0};
    image.encoded_payload = (DERItem){
        .data = (DERByte *)(uintptr_t)payload_with_properties,
        .length = sizeof(payload_with_properties),
    };
    PropertyCapture capture = {0};
    const BootImageRecoveredStatus property_status =
        boot_img4_evaluate_image_payload_properties(
            &image, capture_property, &capture);
    assert(property_status == BOOT_IMAGE_RECOVERED_OK);
    assert(capture.calls == 1u);
    assert(capture.property.key == AVP_BOOT_IMAGE_FOURCC('n', 'o', 'n', 'c'));
    assert(capture.property.integer == 0x123u);
    image.encoded_payload = (DERItem){
        .data = (DERByte *)(uintptr_t)im4p,
        .length = sizeof(im4p),
    };
    assert(boot_img4_evaluate_image_payload_properties(
               &image, capture_property, &capture) ==
           BOOT_IMAGE_RECOVERED_FORMAT);

    uint8_t region[64];
    const RuntimeFirebloomType byte_type = {
        .identity = 1u, .element_size = 1u, .bytewise_copyable = true};
    const RuntimeFirebloomSpan slice = boot_image_resolve_physical_region_slice(
        region, sizeof(region), 12u, 20u, &byte_type);
    assert(slice.current == &region[12]);
    assert(slice.upper_bound == &region[64]);

    boot_image_release_memory_source(&registry, provider_source);
    boot_image_release_memory_source(&registry, memory_source);
    assert(registry.count == 0u);
}

typedef struct {
    size_t calls;
    RuntimeTask *last_task;
    RuntimeEndpointStatus last_status;
} WakeCapture;

static void endpoint_wake(void *context, RuntimeTask *task,
                          RuntimeEndpointStatus status) {
    WakeCapture *capture = context;
    ++capture->calls;
    capture->last_task = task;
    capture->last_status = status;
}

static void test_endpoint_owner(void) {
    RuntimeEndpoint endpoint;
    RuntimeEndpointMessage storage[1];
    WakeCapture wake = {0};
    RuntimeTask sender_a = {0};
    RuntimeTask sender_b = {0};
    RuntimeTask receiver = {0};
    assert(runtime_endpoint_create(&endpoint, storage, 1u, endpoint_wake,
                                   &wake) == RUNTIME_ENDPOINT_OK);
    const uint64_t first[] = {11u, 12u};
    const uint64_t second[] = {21u};
    assert(runtime_endpoint_send(&endpoint, &sender_a, first, 2u,
                                 UINT64_MAX) == RUNTIME_ENDPOINT_OK);
    assert(runtime_endpoint_send(&endpoint, &sender_b, second, 1u,
                                 UINT64_MAX) == RUNTIME_ENDPOINT_BLOCKED);
    assert(sender_b.state == RUNTIME_TASK_STATE_IPC_WAIT);

    uint64_t values[4] = {0};
    size_t received = 0;
    assert(runtime_endpoint_receive(&endpoint, &receiver, values, 4u,
                                    &received, UINT64_MAX) ==
           RUNTIME_ENDPOINT_OK);
    assert(received == 2u && values[0] == 11u && values[1] == 12u);
    assert(sender_b.state == RUNTIME_TASK_STATE_READY);
    memset(values, 0, sizeof(values));
    assert(runtime_endpoint_receive(&endpoint, &receiver, values, 4u,
                                    &received, 0u) == RUNTIME_ENDPOINT_OK);
    assert(received == 1u && values[0] == 21u);
    RuntimeEndpointRequest *request =
        runtime_task_endpoint_request_acquire(&endpoint, &receiver);
    RuntimeEndpointRequestBounds bounds =
        runtime_endpoint_outlined_load_task_pending_request_bounds(
            &endpoint, request);
    assert(bounds.current == request);
    assert(bounds.lower_bound == &endpoint.requests[0]);
    assert(bounds.upper_bound ==
           &endpoint.requests[RUNTIME_ENDPOINT_REQUEST_LIMIT]);
    assert(runtime_endpoint_outlined_prepare_blocked_request_validation(
        &endpoint, request, &receiver, RUNTIME_ENDPOINT_REQUEST_FREE));
    assert(wake.calls >= 3u);
}

typedef struct {
    size_t ready_calls;
    size_t cancel_calls;
} TaskHookCapture;

static void task_ready(void *context, RuntimeTask *task) {
    TaskHookCapture *capture = context;
    ++capture->ready_calls;
    assert(task->state == RUNTIME_TASK_STATE_READY);
}

static void task_cancel(void *context, RuntimeTimerEvent *event) {
    TaskHookCapture *capture = context;
    ++capture->cancel_calls;
    event->deadline = 0u;
}

static void empty_task(void *context) {
    (void)context;
}

static void test_task_owner(void) {
    TaskHookCapture capture = {0};
    RuntimeTaskWaitQueueRecovered queue = {
        .ready = task_ready,
        .cancel_timer = task_cancel,
        .hook_context = &capture,
    };
    RuntimeTask first = {0};
    RuntimeTask second = {0};
    assert(runtime_task_block_on_wait_queue(&queue, &first));
    assert(runtime_task_block_on_wait_queue_until_deadline(
        &queue, &second, 100u, 50u));
    assert(runtime_task_wait_queue_wake(&queue, 7u) == &first);
    assert(first.frame.x[0] == 7u);
    assert(runtime_task_wait_queue_wake_all(&queue, 9u) == 1u);
    assert(second.frame.x[0] == 9u);
    assert(capture.ready_calls == 2u && capture.cancel_calls == 2u);
    assert(!runtime_task_block_on_wait_queue_until_deadline(
        &queue, &first, 50u, 50u));
    assert(first.frame.x[0] == 1u);

    uint8_t *storage = aligned_alloc(RUNTIME_TASK_STACK_PAGE_SIZE,
                                     3u * RUNTIME_TASK_STACK_PAGE_SIZE);
    assert(storage != NULL);
    RuntimeTaskStackPoolRecovered pool;
    runtime_task_stack_initialize(&pool, storage, 3u);
    void *page0 = runtime_task_stack_allocate_bitmap_page(&pool);
    void *page1 = runtime_task_stack_allocate_bitmap_page(&pool);
    assert(page0 == storage);
    assert(page1 == storage + RUNTIME_TASK_STACK_PAGE_SIZE);
    runtime_task_stack_release_bitmap_page(&pool, page0);
    assert(runtime_task_stack_allocate_bitmap_page(&pool) == page0);

    RuntimeTask root;
    RuntimeTask main_task;
    RuntimeTask interrupt_task;
    RuntimeSchedulerBootstrapRecovered bootstrap = {
        .stack_pool = &pool,
        .root_task = &root,
        .main_task = &main_task,
        .interrupt_task = &interrupt_task,
        .main_descriptor = {.entry = empty_task, .entry_context = &root},
        .interrupt_descriptor = {.entry = empty_task,
                                 .entry_context = &interrupt_task},
    };
    runtime_task_stack_release_bitmap_page(&pool, page0);
    runtime_task_stack_release_bitmap_page(&pool, page1);
    assert(runtime_scheduler_bootstrap(&bootstrap));
    assert(root.state == RUNTIME_TASK_STATE_RUNNING);
    assert(main_task.state == RUNTIME_TASK_STATE_READY);
    assert(interrupt_task.state == RUNTIME_TASK_STATE_PARKED);
    assert(main_task.frame.x[0] == (uintptr_t)&root);
    const RuntimeFirebloomType task_type = {
        .identity = 3u,
        .element_size = sizeof(RuntimeTask),
        .bytewise_copyable = true,
    };
    const RuntimeFirebloomSpan task_span =
        runtime_scheduler_bootstrap_outlined_prepare_task_initialize_arguments(
            &main_task, &task_type);
    assert(task_span.current == &main_task);
    assert(task_span.upper_bound == (uint8_t *)&main_task + sizeof(main_task));
    assert(runtime_scheduler_interrupt_task_stack_storage_span().current ==
           (void *)(uintptr_t)0x70029b90u);
    assert(runtime_scheduler_bootstrap_task_stack_storage_span().upper_bound ==
           (void *)(uintptr_t)0x70029c70u);
    free(storage);
}

static void test_firebloom_owner(void) {
    const RuntimeFirebloomType type = {
        .identity = 2u, .element_size = 1u, .bytewise_copyable = true};
    const RuntimeFirebloomSpan empty =
        j_runtime_firebloom_return_empty_tuple(&type);
    const RuntimeFirebloomSpan clone =
        j_runtime_firebloom_return_empty_tuple_clone(&type);
    assert(empty.current == NULL && empty.type == &type);
    assert(clone.current == NULL && clone.type == &type);
    uint8_t descriptor[0x28] = {0};
    descriptor[0x20] = 0x78;
    descriptor[0x21] = 0x56;
    descriptor[0x22] = 0x34;
    descriptor[0x23] = 0x12;
    assert(runtime_firebloom_descriptor_byte_size(
               descriptor, descriptor, descriptor + sizeof(descriptor)) ==
           0x12345678u);
    const RuntimeFirebloomDescriptorBounds bounds =
        runtime_firebloom_outlined_save_descriptor_bounds_x21_x20(
            descriptor, descriptor + sizeof(descriptor));
    assert(bounds.lower_bound == (uintptr_t)descriptor);
    assert(bounds.upper_bound == (uintptr_t)(descriptor + sizeof(descriptor)));
}

int main(void) {
    test_boot_image_owner();
    test_endpoint_owner();
    test_task_owner();
    test_firebloom_owner();
    puts("runtime remaining 38 recovered tests passed");
    return 0;
}
