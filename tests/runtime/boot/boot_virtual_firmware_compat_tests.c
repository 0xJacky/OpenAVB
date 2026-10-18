#include "avp/runtime/boot/boot_virtual_firmware_compat.h"
#include "avp/crypto/hash/corecrypto_sha512_compat.h"

#include <assert.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct {
    uint8_t storage[AVP_HUFA_SCAN_SIZE];
    bool short_prefix[AVP_HUFA_SCAN_SIZE / AVP_HUFA_SCAN_STRIDE];
    bool short_header[AVP_HUFA_SCAN_SIZE / AVP_HUFA_SCAN_STRIDE];
    size_t read_count;
    uint64_t diagnostic_code;
    uint32_t diagnostic_detail;
    size_t diagnostic_candidates;
    size_t diagnostic_count;
} TestPlatform;

static void store_le32(uint8_t *bytes, uint32_t value) {
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
    bytes[2] = (uint8_t)(value >> 16);
    bytes[3] = (uint8_t)(value >> 24);
}

static void install_header(TestPlatform *platform, size_t slot,
                           uint32_t generation, uint32_t source_offset) {
    assert(slot < AVP_HUFA_SCAN_SIZE / AVP_HUFA_SCAN_STRIDE);
    uint8_t signed_bytes[AVP_HUFA_SIGNED_SIZE];
    for (size_t index = 0; index < sizeof(signed_bytes); ++index) {
        signed_bytes[index] = (uint8_t)(slot * 17 + index);
    }
    store_le32(signed_bytes, AVP_HUFA_MAGIC);
    store_le32(signed_bytes + 4, AVP_HUFA_VERSION);
    store_le32(signed_bytes + 8, generation);
    store_le32(signed_bytes + 12, source_offset);
    uint8_t *header = platform->storage + slot * AVP_HUFA_SCAN_STRIDE;
    memcpy(header, signed_bytes, sizeof(signed_bytes));
    AVPCCSHA512Context context;
    uint8_t digest[AVP_SHA384_DIGEST_SIZE];
    avp_ccsha512_init(&context, &avp_ccsha384_info);
    assert(avp_ccsha512_update(&context, signed_bytes, sizeof(signed_bytes)) ==
           0);
    assert(ccsha512_final(&avp_ccsha384_info, &context, digest) == 0);
    memcpy(header + sizeof(signed_bytes), digest,
           AVP_HUFA_DIGEST_PREFIX_SIZE);
}

static size_t test_read(void *context, uint32_t offset, void *output,
                        size_t size) {
    TestPlatform *platform = context;
    ++platform->read_count;
    assert(offset < sizeof(platform->storage));
    assert(size <= sizeof(platform->storage) - offset);
    const size_t slot = offset / AVP_HUFA_SCAN_STRIDE;
    if (size == sizeof(uint32_t) && platform->short_prefix[slot]) {
        return 0;
    }
    if (size == AVP_HUFA_HEADER_SIZE && platform->short_header[slot]) {
        memcpy(output, platform->storage + offset, size - 1);
        return size - 1;
    }
    memcpy(output, platform->storage + offset, size);
    return size;
}

static void test_diagnostic(void *context, uint64_t code, uint32_t detail,
                            size_t candidate_count) {
    TestPlatform *platform = context;
    platform->diagnostic_code = code;
    platform->diagnostic_detail = detail;
    platform->diagnostic_candidates = candidate_count;
    ++platform->diagnostic_count;
}

static BootVirtualFirmwarePlatform binding(TestPlatform *platform) {
    return (BootVirtualFirmwarePlatform){
        .context = platform,
        .read = test_read,
        .diagnostic = test_diagnostic,
    };
}

static void test_empty_scan_and_progress(void) {
    TestPlatform storage = {0};
    BootVirtualFirmwarePlatform platform = binding(&storage);
    AVPHUFAScanResult result;
    assert(boot_virtual_firmware_scan_redundant_headers(&platform, &result) ==
           0);
    assert(result.count == 0);
    assert(storage.read_count == 32);
    assert(boot_virtual_firmware_select_latest_source_offset(&platform,
                                                             &result) == 0);
    assert(storage.diagnostic_code == AVP_HUFA_PROGRESS_NO_HEADER);
    assert(storage.diagnostic_candidates == 0);
    assert(boot_virtual_firmware_no_header_progress_code() ==
           AVP_HUFA_PROGRESS_NO_HEADER);
}

static void test_validation_failures(void) {
    TestPlatform storage = {0};
    install_header(&storage, 1, 7, UINT32_C(0x1111));
    install_header(&storage, 2, 8, UINT32_C(0x2222));
    install_header(&storage, 3, 9, UINT32_C(0x3333));
    install_header(&storage, 4, 10, UINT32_C(0x4444));
    store_le32(storage.storage + 1 * AVP_HUFA_SCAN_STRIDE + 4, 2);
    storage.storage[2 * AVP_HUFA_SCAN_STRIDE + 32 + 17] ^= 1;
    storage.short_prefix[3] = true;
    storage.short_header[4] = true;
    BootVirtualFirmwarePlatform platform = binding(&storage);
    AVPHUFAScanResult result;
    assert(boot_virtual_firmware_scan_redundant_headers(&platform, &result) ==
           0);
}

static void test_generation_selection(void) {
    TestPlatform storage = {0};
    install_header(&storage, 3, 7, UINT32_C(0x12345000));
    install_header(&storage, 27, 19, UINT32_C(0x87654000));
    BootVirtualFirmwarePlatform platform = binding(&storage);
    AVPHUFAScanResult result;
    assert(boot_virtual_firmware_select_latest_source_offset(
               &platform, &result) == UINT32_C(0x87654000));
    assert(result.count == 2);
    assert(result.storage_offsets[0] == UINT32_C(0x3000));
    assert(result.storage_offsets[1] == UINT32_C(0x1b000));
    assert(storage.diagnostic_code == AVP_HUFA_PROGRESS_SELECTED);
    assert(storage.diagnostic_detail == UINT32_C(0x87654000));
    assert(storage.diagnostic_candidates == 2);

    const BootVirtualFirmwareProgress progress =
        boot_virtual_firmware_prepare_selection_progress(UINT32_C(0x87654000));
    assert(progress.code == AVP_HUFA_PROGRESS_SELECTED);
    assert(progress.source_offset == UINT32_C(0x87654000));
}

static void test_tie_preserves_first(void) {
    TestPlatform storage = {0};
    install_header(&storage, 4, 11, UINT32_C(0xaaaa));
    install_header(&storage, 8, 11, UINT32_C(0xbbbb));
    BootVirtualFirmwarePlatform platform = binding(&storage);
    AVPHUFAScanResult result;
    assert(boot_virtual_firmware_select_latest_source_offset(
               &platform, &result) == UINT32_C(0xaaaa));
}

static uint64_t next_word(uint64_t *state) {
    uint64_t value = *state;
    value ^= value << 13;
    value ^= value >> 7;
    value ^= value << 17;
    *state = value;
    return value;
}

static bool oracle_valid_header(const uint8_t *header) {
    AVPCCSHA512Context context;
    uint8_t digest[AVP_SHA384_DIGEST_SIZE];
    avp_ccsha512_init(&context, &avp_ccsha384_info);
    assert(avp_ccsha512_update(&context, header, AVP_HUFA_SIGNED_SIZE) == 0);
    assert(ccsha512_final(&avp_ccsha384_info, &context, digest) == 0);
    return header[0] == 'H' && header[1] == 'U' && header[2] == 'F' &&
           header[3] == 'A' && header[4] == 1 && header[5] == 0 &&
           header[6] == 0 && header[7] == 0 &&
           memcmp(digest, header + AVP_HUFA_SIGNED_SIZE,
                  AVP_HUFA_DIGEST_PREFIX_SIZE) == 0;
}

static size_t oracle_scan(const TestPlatform *storage,
                          uint64_t offsets[AVP_HUFA_MAX_CANDIDATES]) {
    size_t count = 0;
    for (size_t slot = 0; slot < AVP_HUFA_SCAN_SIZE / AVP_HUFA_SCAN_STRIDE;
         ++slot) {
        const uint8_t *header = storage->storage + slot * AVP_HUFA_SCAN_STRIDE;
        if (!storage->short_prefix[slot] && !storage->short_header[slot] &&
            oracle_valid_header(header)) {
            assert(count < AVP_HUFA_MAX_CANDIDATES);
            offsets[count++] = slot * AVP_HUFA_SCAN_STRIDE;
        }
    }
    return count;
}

static void test_randomized_scan_oracle(void) {
    uint64_t random_state = UINT64_C(0x687566612d736361);
    for (size_t iteration = 0; iteration < 512; ++iteration) {
        TestPlatform storage = {0};
        const size_t first_slot = next_word(&random_state) & 31;
        size_t second_slot = next_word(&random_state) & 31;
        while (second_slot == first_slot) {
            second_slot = next_word(&random_state) & 31;
        }
        install_header(&storage, first_slot, (uint32_t)next_word(&random_state),
                       (uint32_t)next_word(&random_state));
        install_header(&storage, second_slot,
                       (uint32_t)next_word(&random_state),
                       (uint32_t)next_word(&random_state));
        const size_t corrupt_slot = next_word(&random_state) & 31;
        if (corrupt_slot != first_slot && corrupt_slot != second_slot) {
            install_header(&storage, corrupt_slot, 9, 9);
            storage.storage[corrupt_slot * AVP_HUFA_SCAN_STRIDE + 47] ^= 1;
        }

        uint64_t expected_offsets[AVP_HUFA_MAX_CANDIDATES] = {0};
        const size_t expected_count = oracle_scan(&storage, expected_offsets);
        BootVirtualFirmwarePlatform platform = binding(&storage);
        AVPHUFAScanResult actual;
        assert(boot_virtual_firmware_scan_redundant_headers(
                   &platform, &actual) == expected_count);
        assert(memcmp(actual.storage_offsets, expected_offsets,
                      expected_count * sizeof(expected_offsets[0])) == 0);
    }
}

static void test_real_vphone_header_oracle(void) {
    static const uint8_t header[AVP_HUFA_HEADER_SIZE] = {
        0x48, 0x55, 0x46, 0x41, 0x01, 0x00, 0x00, 0x00,
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x25, 0x05, 0x54, 0x1d, 0x10, 0x40, 0x54, 0x71,
        0xbf, 0x5f, 0x3c, 0xd3, 0x53, 0xf4, 0xa2, 0xd3,
        0x7b, 0x40, 0xde, 0xae, 0x5e, 0x6f, 0x5c, 0x8a,
        0x95, 0xc9, 0x14, 0x1c, 0x3b, 0x24, 0x66, 0xe0,
    };
    assert(oracle_valid_header(header));
}

static void test_helpers(void) {
    TestPlatform storage = {0};
    install_header(&storage, 6, 3, UINT32_C(0x1234));
    AVPHUFAHeader header;
    BootVirtualFirmwarePlatform platform = binding(&storage);
    assert(boot_virtual_firmware_prepare_provider_read(
               &platform, UINT32_C(0x6000), &header, sizeof(header)) ==
           sizeof(header));
    assert(boot_virtual_firmware_compare_header_magic(&header));
    assert(boot_virtual_firmware_dispatch_provider_read(
               &platform, UINT32_C(0x6000), &header, sizeof(uint32_t)) ==
           sizeof(uint32_t));

    AVPHUFAScanResult result;
    memset(&result, 0xa5, sizeof(result));
    boot_virtual_firmware_clear_header_candidates(&result);
    const uint8_t zero[sizeof(result)] = {0};
    assert(memcmp(&result, zero, sizeof(result)) == 0);

    boot_virtual_firmware_log_generated_location(&platform, UINT32_C(0x1cf));
    assert(storage.diagnostic_code == UINT64_C(0x0b347e76762be1ba));
    assert(storage.diagnostic_detail == UINT32_C(0x1cf));
}

static void trigger_third_candidate_trap(void) {
    TestPlatform storage = {0};
    install_header(&storage, 1, 1, 1);
    install_header(&storage, 2, 2, 2);
    install_header(&storage, 3, 3, 3);
    BootVirtualFirmwarePlatform platform = binding(&storage);
    AVPHUFAScanResult result;
    (void)boot_virtual_firmware_scan_redundant_headers(&platform, &result);
}

static void test_candidate_limit_contract(void) {
    const pid_t child = fork();
    assert(child >= 0);
    if (child == 0) {
        trigger_third_candidate_trap();
        _exit(0);
    }
    int status = 0;
    assert(waitpid(child, &status, 0) == child);
    assert(WIFSIGNALED(status));
}

int main(void) {
    test_empty_scan_and_progress();
    test_validation_failures();
    test_generation_selection();
    test_tie_preserves_first();
    test_randomized_scan_oracle();
    test_real_vphone_header_oracle();
    test_helpers();
    test_candidate_limit_contract();
    return 0;
}
