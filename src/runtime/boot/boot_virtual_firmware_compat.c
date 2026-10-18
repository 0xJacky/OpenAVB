#include "avp/runtime/boot/boot_virtual_firmware_compat.h"
#include "avp/crypto/hash/corecrypto_sha512_compat.h"

static _Noreturn void boot_virtual_firmware_contract_failure(void) {
    __builtin_trap();
}

static uint32_t load_le32(const uint8_t *bytes) {
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

static bool byte_ranges_equal(
    const uint8_t *left,
    const uint8_t *right,
    size_t size
) {
    uint8_t difference = 0;
    for (size_t index = 0; index < size; ++index) {
        difference |= left[index] ^ right[index];
    }
    return difference == 0;
}

static uint32_t header_generation(const AVPHUFAHeader *header) {
    return load_le32(header->bytes + 8);
}

static uint32_t header_source_offset(const AVPHUFAHeader *header) {
    return load_le32(header->bytes + 12);
}

static bool header_is_valid(const AVPHUFAHeader *header) {
    uint8_t digest[AVP_SHA384_DIGEST_SIZE];
    AVPCCSHA512Context context;
    avp_ccsha512_init(&context, &avp_ccsha384_info);
    if (avp_ccsha512_update(&context, header->bytes,
                            AVP_HUFA_SIGNED_SIZE) != 0 ||
        ccsha512_final(&avp_ccsha384_info, &context, digest) != 0) {
        boot_virtual_firmware_contract_failure();
    }
    return boot_virtual_firmware_compare_header_magic(header) &&
           load_le32(header->bytes + 4) == AVP_HUFA_VERSION &&
           byte_ranges_equal(
               digest,
               header->bytes + AVP_HUFA_SIGNED_SIZE,
               AVP_HUFA_DIGEST_PREFIX_SIZE
           );
}

size_t BOOT_VIRTUAL_FIRMWARE_PUBLIC boot_virtual_firmware_prepare_provider_read(
    const BootVirtualFirmwarePlatform *platform,
    uint32_t offset,
    void *output,
    size_t size
) {
    if (platform == 0 || platform->read == 0 || (output == 0 && size != 0)) {
        boot_virtual_firmware_contract_failure();
    }
    return platform->read(platform->context, offset, output, size);
}

size_t BOOT_VIRTUAL_FIRMWARE_PUBLIC
boot_virtual_firmware_dispatch_provider_read(
    const BootVirtualFirmwarePlatform *platform,
    uint32_t offset,
    void *output,
    size_t size
) {
    return boot_virtual_firmware_prepare_provider_read(
        platform,
        offset,
        output,
        size
    );
}

bool BOOT_VIRTUAL_FIRMWARE_PUBLIC
boot_virtual_firmware_compare_header_magic(const AVPHUFAHeader *header) {
    if (header == 0) {
        boot_virtual_firmware_contract_failure();
    }
    return load_le32(header->bytes) == AVP_HUFA_MAGIC;
}

void BOOT_VIRTUAL_FIRMWARE_PUBLIC
boot_virtual_firmware_clear_header_candidates(AVPHUFAScanResult *result) {
    if (result == 0) {
        boot_virtual_firmware_contract_failure();
    }
    uint8_t *bytes = (uint8_t *)result->candidates;
    for (size_t index = 0; index < sizeof(result->candidates); ++index) {
        bytes[index] = 0;
    }
    for (size_t index = 0; index < AVP_HUFA_MAX_CANDIDATES; ++index) {
        result->storage_offsets[index] = 0;
    }
    result->count = 0;
}

size_t BOOT_VIRTUAL_FIRMWARE_PUBLIC
boot_virtual_firmware_scan_redundant_headers(
    const BootVirtualFirmwarePlatform *platform,
    AVPHUFAScanResult *result
) {
    if (platform == 0 || platform->read == 0 || result == 0) {
        boot_virtual_firmware_contract_failure();
    }
    boot_virtual_firmware_clear_header_candidates(result);

    for (uint32_t offset = 0; offset < AVP_HUFA_SCAN_SIZE;
         offset += AVP_HUFA_SCAN_STRIDE) {
        AVPHUFAHeader header = {{0}};
        const size_t prefix_size = boot_virtual_firmware_prepare_provider_read(
            platform,
            offset,
            &header,
            sizeof(uint32_t)
        );
        if (prefix_size != sizeof(uint32_t) ||
            !boot_virtual_firmware_compare_header_magic(&header)) {
            continue;
        }
        const size_t header_size = boot_virtual_firmware_dispatch_provider_read(
            platform,
            offset,
            &header,
            sizeof(header)
        );
        if (header_size != sizeof(header) || !header_is_valid(&header)) {
            continue;
        }
        if (result->count == AVP_HUFA_MAX_CANDIDATES) {
            boot_virtual_firmware_contract_failure();
        }
        result->candidates[result->count] = header;
        result->storage_offsets[result->count] = offset;
        ++result->count;
    }
    return result->count;
}

uint32_t BOOT_VIRTUAL_FIRMWARE_PUBLIC
boot_virtual_firmware_no_header_progress_code(void) {
    return AVP_HUFA_PROGRESS_NO_HEADER;
}

BootVirtualFirmwareProgress BOOT_VIRTUAL_FIRMWARE_PUBLIC
boot_virtual_firmware_prepare_selection_progress(uint32_t source_offset) {
    return (BootVirtualFirmwareProgress){
        .code = AVP_HUFA_PROGRESS_SELECTED,
        .source_offset = source_offset,
    };
}

void BOOT_VIRTUAL_FIRMWARE_PUBLIC boot_virtual_firmware_log_generated_location(
    const BootVirtualFirmwarePlatform *platform,
    uint32_t line
) {
    if (platform == 0 || platform->diagnostic == 0) {
        boot_virtual_firmware_contract_failure();
    }
    platform->diagnostic(
        platform->context,
        UINT64_C(0x0b347e76762be1ba),
        line,
        0
    );
}

uint32_t BOOT_VIRTUAL_FIRMWARE_PUBLIC
boot_virtual_firmware_select_latest_source_offset(
    const BootVirtualFirmwarePlatform *platform,
    AVPHUFAScanResult *result
) {
    if (platform == 0 || platform->diagnostic == 0) {
        boot_virtual_firmware_contract_failure();
    }
    const size_t count =
        boot_virtual_firmware_scan_redundant_headers(platform, result);
    if (count == 0) {
        platform->diagnostic(
            platform->context,
            boot_virtual_firmware_no_header_progress_code(),
            0,
            0
        );
        return 0;
    }

    size_t selected = 0;
    if (count == 2 && header_generation(&result->candidates[0]) <
                          header_generation(&result->candidates[1])) {
        selected = 1;
    }
    const uint32_t source_offset =
        header_source_offset(&result->candidates[selected]);
    const BootVirtualFirmwareProgress progress =
        boot_virtual_firmware_prepare_selection_progress(source_offset);
    platform->diagnostic(
        platform->context,
        progress.code,
        progress.source_offset,
        count
    );
    return source_offset;
}
