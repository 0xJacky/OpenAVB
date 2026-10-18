#include "avp/devices/simplevirtio/simple_virtio_storage_adapter.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>


typedef struct {
    unsigned initialize_calls;
    uintptr_t base;
    bool initialize_result;
} InitializeTranscript;

static InitializeTranscript initialize_transcript;

bool simple_virtio_block_initialize(SimpleVirtioBlock *block, uintptr_t base) {
    ++initialize_transcript.initialize_calls;
    initialize_transcript.base = base;
    if (!initialize_transcript.initialize_result) {
        return false;
    }
    *block = (SimpleVirtioBlock){
        .base = base,
        .sector_count = 4096,
        .block_size = 4096,
    };
    return true;
}

typedef struct {
    unsigned calls;
    uint16_t svc;
    uint16_t method;
    SimpleVirtioStorageInitializePacket request;
    uint32_t response_result;
    int status;
} RemoteTranscript;

static int record_remote_call(
    void *context,
    uint16_t svc,
    uint16_t method,
    const SimpleVirtioStorageInitializePacket *request,
    SimpleVirtioStorageInitializePacket *response
) {
    RemoteTranscript *transcript = context;
    ++transcript->calls;
    transcript->svc = svc;
    transcript->method = method;
    transcript->request = *request;
    response->result = transcript->response_result;
    return transcript->status;
}

static void test_block_device_initialize(void) {
    SimpleVirtioBlock block = {0};
    initialize_transcript = (InitializeTranscript){
        .initialize_result = true,
    };
    assert(simple_virtio_block_device_initialize(
        &block,
        SIMPLE_VIRTIO_BLOCK_SLOT_UTIL
    ));
    assert(initialize_transcript.initialize_calls == 1);
    assert(initialize_transcript.base == SIMPLE_VIRTIO_UTIL_BLOCK_BASE);
    assert(block.slot == SIMPLE_VIRTIO_BLOCK_SLOT_UTIL);
    assert(block.block_size == 4096);

    initialize_transcript.initialize_result = false;
    assert(!simple_virtio_block_device_initialize(
        &block,
        SIMPLE_VIRTIO_BLOCK_SLOT_ROOT
    ));
    assert(!simple_virtio_block_device_initialize(&block, (SimpleVirtioBlockSlot)2));
}

static void test_device_tuple_and_spans(void) {
    SimpleVirtioBlock block = {0};
    SimpleVirtioBlockDeviceTuple tuple =
        simple_virtio_block_outlined_device_span_x26(
            &block,
            (uintptr_t)&block + sizeof(block),
            SIMPLE_VIRTIO_BLOCK_DEVICE_ID
        );
    assert(simple_virtio_block_validate_device_tuple(&tuple));
    assert(simple_virtio_block_validate_device_tuple_thunk(&tuple));
    assert(simple_virtio_block_validate_device_tuple_thunk_clone(&tuple));
    assert(tuple.pointer == &block);
    assert(tuple.lower_bound == (uintptr_t)&block);

    tuple.type = 3;
    assert(!simple_virtio_block_validate_device_tuple(&tuple));
    tuple.type = SIMPLE_VIRTIO_BLOCK_DEVICE_ID;
    tuple.upper_bound = (uintptr_t)&block + sizeof(block) - 1;
    assert(!simple_virtio_block_validate_device_tuple_thunk(&tuple));

    uint32_t provider = 0x12345678;
    SimpleVirtioBlockProviderSpan span =
        simple_virtio_block_outlined_provider_span_x19_x28(
            &provider,
            (uintptr_t)&provider + sizeof(provider)
        );
    assert(span.pointer == &provider);
    assert(span.lower_bound == (uintptr_t)&provider);
    assert(span.upper_bound == (uintptr_t)&provider + sizeof(provider));
}

static void test_sector_and_frame_state(void) {
    SimpleVirtioBlock block = {.block_size = 4096};
    SimpleVirtioBlockHeader header = {0};
    assert(simple_virtio_block_outlined_update_sector_state(&block, 7, &header));
    assert(header.sector == 56);
    block.block_size = 513;
    assert(!simple_virtio_block_outlined_update_sector_state(&block, 7, &header));

    uintptr_t input[10];
    uintptr_t output[10] = {0};
    for (size_t index = 0; index < 10; ++index) {
        input[index] = 0x1900u + index;
    }
    SimpleVirtioBlockFrame70 frame;
    simple_virtio_block_outlined_frame70_save_x19_x28(&frame, input);
    simple_virtio_block_outlined_frame70_restore_x19_x28(&frame, output);
    assert(memcmp(input, output, sizeof(input)) == 0);
}

static void test_dispatch_capture(void) {
    SimpleVirtioBlock block = {0};
    uint8_t bytes[8] = {0};
    const SimpleVirtioBlockDeviceTuple device = {
        .pointer = &block,
        .lower_bound = (uintptr_t)&block,
        .upper_bound = (uintptr_t)&block + sizeof(block),
        .type = SIMPLE_VIRTIO_BLOCK_DEVICE_ID,
    };
    SimpleVirtioBufferSpan buffer = {
        .pointer = bytes,
        .lower_bound = (uintptr_t)bytes,
        .upper_bound = (uintptr_t)bytes + sizeof(bytes),
    };
    SimpleVirtioBlockDispatchCapture capture;
    assert(simple_virtio_block_outlined_capture_buffer_span_and_device(
        &device,
        &buffer,
        &capture
    ));
    assert(capture.device.pointer == &block);
    assert(capture.buffer.pointer == bytes);

    buffer.upper_bound = (uintptr_t)bytes;
    assert(!simple_virtio_block_outlined_capture_buffer_span_and_device(
        &device,
        &buffer,
        &capture
    ));
}

static void test_remote_initialize_transcript(void) {
    const char domain[SIMPLE_VIRTIO_STORAGE_DOMAIN_SIZE] = {
        'u', 't', 'i', 'l', '\0'
    };
    RemoteTranscript transcript = {0};
    assert(simple_virtio_storage_initialize_remote(
        record_remote_call,
        &transcript,
        domain,
        SIMPLE_VIRTIO_BLOCK_SLOT_UTIL
    ) == SIMPLE_VIRTIO_STORAGE_ADAPTER_SUCCESS);
    assert(transcript.calls == 1);
    assert(transcript.svc == SIMPLE_VIRTIO_STORAGE_SVC);
    assert(transcript.method == SIMPLE_VIRTIO_STORAGE_INITIALIZE_METHOD);
    assert(memcmp(transcript.request.domain, domain, sizeof(domain)) == 0);
    assert(transcript.request.slot == SIMPLE_VIRTIO_BLOCK_SLOT_UTIL);

    transcript.response_result = 1;
    assert(simple_virtio_storage_initialize_remote(
        record_remote_call,
        &transcript,
        domain,
        SIMPLE_VIRTIO_BLOCK_SLOT_UTIL
    ) == SIMPLE_VIRTIO_STORAGE_ADAPTER_RESPONSE_ERROR);
    transcript.response_result = 0;
    transcript.status = SIMPLE_VIRTIO_STORAGE_ADAPTER_REMOTE_ERROR;
    assert(simple_virtio_storage_initialize_remote(
        record_remote_call,
        &transcript,
        domain,
        SIMPLE_VIRTIO_BLOCK_SLOT_ROOT
    ) == SIMPLE_VIRTIO_STORAGE_ADAPTER_REMOTE_ERROR);
}

static void test_builder_and_u32_field(void) {
    SimpleVirtioStorageRPCBuilder builder = {0};
    const SimpleVirtioStorageBuilderSpan span =
        simple_virtio_storage_outlined_builder_span_x28(&builder);
    assert(span.pointer == &builder);
    assert(span.lower_bound == (uintptr_t)&builder);
    assert(span.upper_bound - span.lower_bound == 0x28);

    const uint32_t type_token = 0x55aa;
    const SimpleVirtioStorageU32Field field =
        simple_virtio_storage_outlined_prepare_u32_field(&type_token);
    assert(field.type == &type_token);
    assert(field.alignment == 4);
    assert(field.size == 4);
    assert(field.pointer_offset == 0);
}

int main(void) {
    test_block_device_initialize();
    test_device_tuple_and_spans();
    test_sector_and_frame_state();
    test_dispatch_capture();
    test_remote_initialize_transcript();
    test_builder_and_u32_field();
    puts("simple_virtio_storage_adapter_tests: ok");
    return 0;
}
