#include "avp/image/boot/virtual_firmware.h"

static bool add_u64(uint64_t left, uint64_t right, uint64_t *result) {
    if (right > UINT64_MAX - left) {
        return false;
    }
    *result = left + right;
    return true;
}

static uint64_t minimum_u64(uint64_t left, uint64_t right) {
    return left < right ? left : right;
}

bool virtual_firmware_reader_initialize(
    VirtualFirmwareReader *reader,
    const SimpleVirtioBlock *block,
    void *bounce,
    size_t bounce_size,
    void *diagnostic_context,
    BootVirtualFirmwareDiagnostic diagnostic
) {
    if (reader == 0 || block == 0 || block->ops == 0 ||
        block->ops->read == 0 || bounce == 0 || diagnostic == 0 ||
        block->block_size < 512 ||
        (block->block_size & 511U) != 0 ||
        block->block_size > bounce_size ||
        block->block_size > VIRTUAL_FIRMWARE_MAX_BLOCK_SIZE ||
        block->sector_count > UINT64_MAX / 512) {
        return false;
    }
    const uint64_t device_capacity = block->sector_count * 512;
    *reader = (VirtualFirmwareReader){
        .block = block,
        .capacity = minimum_u64(
            device_capacity,
            VIRTUAL_FIRMWARE_PROVIDER_CAPACITY
        ),
        .bounce = bounce,
        .bounce_size = bounce_size,
        .diagnostic_context = diagnostic_context,
        .diagnostic = diagnostic,
    };
    return reader->capacity >= AVP_HUFA_SCAN_SIZE;
}

size_t virtual_firmware_reader_read(
    VirtualFirmwareReader *reader,
    uint32_t offset,
    void *output,
    size_t size
) {
    if (reader == 0 || output == 0 || size == 0) {
        return 0;
    }
    uint64_t end = 0;
    if (!add_u64(offset, size, &end) || end > reader->capacity) {
        return 0;
    }

    uint8_t *destination = output;
    uint64_t cursor = offset;
    size_t remaining = size;
    const uint32_t block_size = reader->block->block_size;
    while (remaining != 0) {
        const uint64_t block_index = cursor / block_size;
        const size_t intra_block = (size_t)(cursor % block_size);
        if (intra_block == 0 && remaining >= block_size) {
            size_t block_count = remaining / block_size;
            const size_t maximum_count = UINT32_MAX / block_size;
            if (block_count > maximum_count) {
                block_count = maximum_count;
            }
            if (!reader->block->ops->read(
                    reader->block,
                    block_index,
                    (uint32_t)block_count,
                    destination
                )) {
                return 0;
            }
            const size_t transferred = block_count * block_size;
            destination += transferred;
            cursor += transferred;
            remaining -= transferred;
            continue;
        }

        if (!reader->block->ops->read(
                reader->block,
                block_index,
                1,
                reader->bounce
            )) {
            return 0;
        }
        size_t transferred = block_size - intra_block;
        if (transferred > remaining) {
            transferred = remaining;
        }
        for (size_t index = 0; index < transferred; ++index) {
            destination[index] = reader->bounce[intra_block + index];
        }
        destination += transferred;
        cursor += transferred;
        remaining -= transferred;
    }
    return size;
}

static size_t reader_dispatch(
    void *context,
    uint32_t offset,
    void *output,
    size_t size
) {
    return virtual_firmware_reader_read(context, offset, output, size);
}

static void reader_diagnostic(
    void *context,
    uint64_t code,
    uint32_t detail,
    size_t candidate_count
) {
    VirtualFirmwareReader *reader = context;
    reader->diagnostic(
        reader->diagnostic_context,
        code,
        detail,
        candidate_count
    );
}

bool virtual_firmware_reader_select_source(
    VirtualFirmwareReader *reader,
    AVPHUFAScanResult *scan,
    uint32_t *source_offset
) {
    if (reader == 0 || scan == 0 || source_offset == 0) {
        return false;
    }
    const BootVirtualFirmwarePlatform platform = {
        .context = reader,
        .read = reader_dispatch,
        .diagnostic = reader_diagnostic,
    };
    *source_offset =
        boot_virtual_firmware_select_latest_source_offset(&platform, scan);
    return scan->count != 0 && *source_offset < reader->capacity;
}
