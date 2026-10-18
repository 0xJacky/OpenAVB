#include "avp/image/boot/virtual_firmware.h"
#include "avp/crypto/hash/corecrypto_sha512_compat.h"

#include <assert.h>
#include <string.h>

enum {
    TEST_STORAGE_SIZE = 0x50000,
    TEST_BLOCK_SIZE_MAX = 0x1000,
};

typedef struct {
    SimpleVirtioBlock block;
    uint8_t storage[TEST_STORAGE_SIZE];
    uint64_t block_indices[128];
    uint32_t block_counts[128];
    size_t read_count;
    uint64_t diagnostic_code;
    uint32_t diagnostic_detail;
    size_t diagnostic_candidates;
} TestDevice;

static void store_le32(uint8_t *bytes, uint32_t value) {
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
    bytes[2] = (uint8_t)(value >> 16);
    bytes[3] = (uint8_t)(value >> 24);
}

static bool test_read(
    const SimpleVirtioBlock *block,
    uint64_t block_index,
    uint32_t block_count,
    void *buffer
) {
    TestDevice *device = (TestDevice *)(uintptr_t)block->base;
    assert(device->read_count < 128);
    device->block_indices[device->read_count] = block_index;
    device->block_counts[device->read_count] = block_count;
    ++device->read_count;
    const uint64_t offset = block_index * block->block_size;
    const size_t size = (size_t)block_count * block->block_size;
    assert(offset <= sizeof(device->storage));
    assert(size <= sizeof(device->storage) - offset);
    memcpy(buffer, device->storage + offset, size);
    return true;
}

static const SimpleVirtioBlockOps test_ops = {
    .read = test_read,
    .write = NULL,
};

static void initialize_device(TestDevice *device, uint32_t block_size) {
    memset(device, 0, sizeof(*device));
    device->block = (SimpleVirtioBlock){
        .base = (uintptr_t)device,
        .sector_count = sizeof(device->storage) / 512,
        .block_size = block_size,
        .ops = &test_ops,
        .slot = SIMPLE_VIRTIO_BLOCK_SLOT_UTIL,
    };
    for (size_t index = 0; index < sizeof(device->storage); ++index) {
        device->storage[index] = (uint8_t)(index * 37U + 11U);
    }
}

static void install_header(
    TestDevice *device,
    uint32_t offset,
    uint32_t generation,
    uint32_t source_offset
) {
    assert(offset <= sizeof(device->storage) - AVP_HUFA_HEADER_SIZE);
    uint8_t *header = device->storage + offset;
    memset(header, 0, AVP_HUFA_HEADER_SIZE);
    store_le32(header, AVP_HUFA_MAGIC);
    store_le32(header + 4, AVP_HUFA_VERSION);
    store_le32(header + 8, generation);
    store_le32(header + 12, source_offset);
    AVPCCSHA512Context context;
    uint8_t digest[AVP_SHA384_DIGEST_SIZE];
    avp_ccsha512_init(&context, &avp_ccsha384_info);
    assert(avp_ccsha512_update(&context, header, AVP_HUFA_SIGNED_SIZE) == 0);
    assert(ccsha512_final(&avp_ccsha384_info, &context, digest) == 0);
    memcpy(
        header + AVP_HUFA_SIGNED_SIZE,
        digest,
        AVP_HUFA_DIGEST_PREFIX_SIZE
    );
}

static void diagnostic(
    void *context,
    uint64_t code,
    uint32_t detail,
    size_t candidate_count
) {
    TestDevice *device = context;
    device->diagnostic_code = code;
    device->diagnostic_detail = detail;
    device->diagnostic_candidates = candidate_count;
}

static VirtualFirmwareReader reader_for(
    TestDevice *device,
    uint8_t *bounce,
    size_t bounce_size
) {
    VirtualFirmwareReader reader;
    assert(virtual_firmware_reader_initialize(
        &reader, &device->block, bounce, bounce_size, device, diagnostic));
    return reader;
}

static void test_unaligned_byte_reads(uint32_t block_size) {
    TestDevice device;
    uint8_t bounce[TEST_BLOCK_SIZE_MAX];
    initialize_device(&device, block_size);
    VirtualFirmwareReader reader = reader_for(&device, bounce, sizeof(bounce));

    uint8_t actual[block_size * 3];
    const uint32_t offset = block_size - 13;
    const size_t size = block_size * 2 + 29;
    assert(virtual_firmware_reader_read(&reader, offset, actual, size) == size);
    assert(memcmp(actual, device.storage + offset, size) == 0);
    assert(device.block_indices[0] == 0 && device.block_counts[0] == 1);
    assert(device.block_indices[1] == 1 && device.block_counts[1] == 2);
    assert(device.block_indices[2] == 3 && device.block_counts[2] == 1);
}

static void test_hufa_selection_uses_util_byte_offsets(void) {
    TestDevice device;
    uint8_t bounce[TEST_BLOCK_SIZE_MAX];
    initialize_device(&device, 0x1000);
    install_header(&device, 0x0000, 1, 0x30000);
    install_header(&device, 0x1f000, 9, 0x40000);
    memcpy(device.storage + 0x1800, "HUFA", 4);
    memcpy(device.storage + 0x12000, "AFUH", 4);
    VirtualFirmwareReader reader = reader_for(&device, bounce, sizeof(bounce));
    AVPHUFAScanResult scan;
    uint32_t source_offset = 0;
    assert(virtual_firmware_reader_select_source(
        &reader, &scan, &source_offset));
    assert(source_offset == 0x40000);
    assert(scan.count == 2);
    assert(scan.storage_offsets[0] == 0);
    assert(scan.storage_offsets[1] == 0x1f000);
    assert(device.diagnostic_code == AVP_HUFA_PROGRESS_SELECTED);
    assert(device.diagnostic_detail == 0x40000);
    assert(device.diagnostic_candidates == 2);
}

static void test_reader_contract(void) {
    TestDevice device;
    uint8_t bounce[TEST_BLOCK_SIZE_MAX];
    initialize_device(&device, 0x1000);
    VirtualFirmwareReader reader = reader_for(&device, bounce, sizeof(bounce));
    uint8_t output[32];
    assert(virtual_firmware_reader_read(
               &reader, (uint32_t)reader.capacity - 16, output, 32) == 0);
    device.block.block_size = VIRTUAL_FIRMWARE_MAX_BLOCK_SIZE * 2;
    assert(!virtual_firmware_reader_initialize(
        &reader, &device.block, bounce, sizeof(bounce), &device, diagnostic));
}

int main(void) {
    test_unaligned_byte_reads(512);
    test_unaligned_byte_reads(0x1000);
    test_hufa_selection_uses_util_byte_offsets();
    test_reader_contract();
    return 0;
}
