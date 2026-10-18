#include "avp/devices/simplevirtio/simplevirtio_queue_adapter.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>


enum {
    TRANSCRIPT_CAPACITY = 32,
};

typedef struct {
    uint32_t registers[SIMPLEVIRTIO_ADAPTER_MMIO_LIMIT / sizeof(uint32_t)];
    uint32_t write_offsets[TRANSCRIPT_CAPACITY];
    uint64_t write_values[TRANSCRIPT_CAPACITY];
    uint8_t write_widths[TRANSCRIPT_CAPACITY];
    uint8_t write_count;
    uint32_t completion_after_reads;
    uint32_t completion_reads;
    uint64_t translation_delta;
} FakeSimpleVirtio;

static uint32_t fake_read32(
    void *context,
    uintptr_t base,
    uint32_t offset
) {
    (void)base;
    FakeSimpleVirtio *fake = context;
    if (offset == 0x10 && fake->completion_after_reads != 0) {
        ++fake->completion_reads;
        if (fake->completion_reads >= fake->completion_after_reads) {
            return 1;
        }
    }
    return fake->registers[offset / sizeof(uint32_t)];
}

static void record_write(
    FakeSimpleVirtio *fake,
    uint32_t offset,
    uint64_t value,
    uint8_t width
) {
    assert(fake->write_count < TRANSCRIPT_CAPACITY);
    fake->write_offsets[fake->write_count] = offset;
    fake->write_values[fake->write_count] = value;
    fake->write_widths[fake->write_count] = width;
    ++fake->write_count;
}

static void fake_write32(
    void *context,
    uintptr_t base,
    uint32_t offset,
    uint32_t value
) {
    (void)base;
    FakeSimpleVirtio *fake = context;
    fake->registers[offset / sizeof(uint32_t)] = value;
    record_write(fake, offset, value, 4);
}

static void fake_write64(
    void *context,
    uintptr_t base,
    uint32_t offset,
    uint64_t value
) {
    (void)base;
    record_write(context, offset, value, 8);
}

static uint64_t fake_translate(void *context, uint64_t address) {
    const FakeSimpleVirtio *fake = context;
    return address + fake->translation_delta;
}

static SimpleVirtioDeviceSpan device_span(SimpleVirtioQueueDevice *device) {
    const uintptr_t address = (uintptr_t)device;
    return simplevirtio_outlined_save_device_bounds_x19_x20(
        device,
        address,
        address + sizeof(*device)
    );
}

static SimpleVirtioAdapterIO fake_io(FakeSimpleVirtio *fake) {
    return (SimpleVirtioAdapterIO){
        .context = fake,
        .read32 = fake_read32,
        .write32 = fake_write32,
        .write64 = fake_write64,
        .translate = fake_translate,
    };
}

static void initialize_device(
    SimpleVirtioQueueDevice *device,
    FakeSimpleVirtio *fake
) {
    fake->registers[0] = 1;
    fake->registers[1] = 0x20;
    const uint64_t descriptor_addresses[] = {0x1000, 0x2000};
    const SimpleVirtioAdapterIO io = fake_io(fake);
    assert(simplevirtio_device_initialize(
        device,
        &io,
        0x30100000,
        0x20,
        2,
        true,
        descriptor_addresses
    ) == SIMPLEVIRTIO_ADAPTER_OK);
}

static void test_initialize_and_queue_selection_transcript(void) {
    FakeSimpleVirtio fake = {0};
    SimpleVirtioQueueDevice device;
    initialize_device(&device, &fake);
    assert(device.queue_count == 2);
    assert(device.status == 3);
    assert(fake.write_count == 2);
    assert(fake.write_offsets[0] == 0x400);
    assert(fake.write_values[0] == 1);
    assert(fake.write_offsets[1] == 0x404);
    assert(fake.write_values[1] == 3);

    SimpleVirtioDeviceSpan span = device_span(&device);
    uint32_t available = 0;
    fake.registers[0x10 / 4] = 7;
    assert(simplevirtio_queue_available(span, 1, &available) ==
        SIMPLEVIRTIO_ADAPTER_OK);
    assert(available == 7);
    assert(device.selected_queue == 1);
    assert(fake.write_offsets[2] == 0x410);
    assert(fake.write_values[2] == 1);
    assert(simplevirtio_queue_select(span, 2) ==
        SIMPLEVIRTIO_ADAPTER_ERROR_QUEUE);
}

static void test_descriptor_chain_and_commit_transcript(void) {
    FakeSimpleVirtio fake = {.translation_delta = 0x100000};
    SimpleVirtioQueueDevice device;
    initialize_device(&device, &fake);
    SimpleVirtioDeviceSpan span = device_span(&device);
    assert(simplevirtio_queue_select(span, 0) == SIMPLEVIRTIO_ADAPTER_OK);
    assert(simplevirtio_append_descriptor_segment(
        span, 0x5000, 16, false, true
    ) == SIMPLEVIRTIO_ADAPTER_OK);
    assert(simplevirtio_append_descriptor_segment(
        span, 0x6000, 512, true, false
    ) == SIMPLEVIRTIO_ADAPTER_OK);
    const SimpleVirtioSegment *descriptors = device.queues[0].descriptors;
    assert(descriptors[0].flags == SIMPLE_VIRTIO_NEXT);
    assert(descriptors[1].flags == SIMPLE_VIRTIO_WRITABLE);

    fake.completion_after_reads = 2;
    assert(simplevirtio_commit_descriptor(span, true, 3) ==
        SIMPLEVIRTIO_ADAPTER_OK);
    assert(descriptors[0].ipa == 0x105000);
    assert(descriptors[1].ipa == 0x106000);
    assert(device.queues[0].descriptor_count == 0);
    assert(fake.write_offsets[fake.write_count - 1] == 0x408);
    assert(fake.write_values[fake.write_count - 1] == 0x101000);
    assert(fake.write_widths[fake.write_count - 1] == 8);
}

static void test_descriptor_and_busy_errors(void) {
    FakeSimpleVirtio fake = {0};
    SimpleVirtioQueueDevice device;
    initialize_device(&device, &fake);
    SimpleVirtioDeviceSpan span = device_span(&device);
    assert(simplevirtio_append_descriptor_segment(
        span, 0x5000, 16, false, false
    ) == SIMPLEVIRTIO_ADAPTER_OK);
    assert(simplevirtio_append_descriptor_segment(
        span, 0x6000, 16, false, false
    ) == SIMPLEVIRTIO_ADAPTER_OK);
    assert(simplevirtio_commit_descriptor(span, false, 0) ==
        SIMPLEVIRTIO_ADAPTER_ERROR_DESCRIPTOR);

    device.queues[0].descriptor_count = 0;
    assert(simplevirtio_append_descriptor_segment(
        span, 0x7000, 16, false, false
    ) == SIMPLEVIRTIO_ADAPTER_OK);
    assert(simplevirtio_commit_descriptor(span, true, 2) ==
        SIMPLEVIRTIO_ADAPTER_ERROR_BUSY);
    assert(device.queues[0].descriptor_count == 1);
}

static void test_bounds_lifecycle_and_helpers(void) {
    FakeSimpleVirtio fake = {0};
    SimpleVirtioQueueDevice device;
    initialize_device(&device, &fake);
    SimpleVirtioDeviceSpan span = device_span(&device);
    assert(simplevirtio_outlined_device_span_x21(span).device == &device);
    assert(simplevirtio_outlined_device_span_x22(span).upper_bound ==
        span.upper_bound);
    assert(simplevirtio_queue_allocation_validate(
        &device,
        &device.queues[0]
    ));
    assert(simplevirtio_source_token() == SIMPLEVIRTIO_SOURCE_TOKEN);
    const SimpleVirtioDiagnostic diagnostic =
        simplevirtio_outlined_log_source_line(0x53);
    assert(diagnostic.source_token == SIMPLEVIRTIO_SOURCE_TOKEN);
    assert(diagnostic.line == 0x53);

    uint32_t value = 0;
    SimpleVirtioDeviceSpan invalid = span;
    invalid.upper_bound = (uintptr_t)&device + sizeof(device) - 1;
    assert(!simplevirtio_mmio_read32(invalid, 0, &value));
    assert(device.last_error == SIMPLEVIRTIO_ADAPTER_ERROR_BOUNDS);
    assert(!simplevirtio_mmio_write32(span, 3, 1));
    assert(device.last_error == SIMPLEVIRTIO_ADAPTER_ERROR_BOUNDS);

    assert(simplevirtio_device_deinitialize(span, 1) ==
        SIMPLEVIRTIO_ADAPTER_OK);
    assert(device.queue_count == 0);
    assert(device.status == 0);
    assert(fake.write_offsets[fake.write_count - 1] == 0x404);
    assert(fake.write_values[fake.write_count - 1] == 0);
}

static void test_initialize_rejections(void) {
    FakeSimpleVirtio fake = {0};
    SimpleVirtioQueueDevice device;
    const SimpleVirtioAdapterIO io = fake_io(&fake);
    const uint64_t descriptors[] = {0x1000, 0x2000};
    fake.registers[0] = 2;
    fake.registers[1] = 0x20;
    assert(simplevirtio_device_initialize(
        &device, &io, 0x30100000, 0x20, 2, true, descriptors
    ) == SIMPLEVIRTIO_ADAPTER_ERROR_VERSION);

    fake.registers[0] = 1;
    fake.registers[1] = 0x21;
    assert(simplevirtio_device_initialize(
        &device, &io, 0x30100000, 0x20, 2, true, descriptors
    ) == SIMPLEVIRTIO_ADAPTER_ERROR_DEVICE);
    assert(simplevirtio_device_initialize(
        &device, &io, 0x30100000, 0x20, 3, true, descriptors
    ) == SIMPLEVIRTIO_ADAPTER_ERROR_ARGUMENT);
}

int main(void) {
    test_initialize_and_queue_selection_transcript();
    test_descriptor_chain_and_commit_transcript();
    test_descriptor_and_busy_errors();
    test_bounds_lifecycle_and_helpers();
    test_initialize_rejections();
    puts("simplevirtio_queue_adapter_tests: ok");
    return 0;
}
