#include "avp/platform/configuration/sysconfig3_compat.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint8_t bytes[512];
    size_t length;
} Buffer;

static void append(Buffer *buffer, const void *bytes, size_t length) {
    assert(buffer->length + length <= sizeof(buffer->bytes));
    memcpy(buffer->bytes + buffer->length, bytes, length);
    buffer->length += length;
}

static void append_tlv(Buffer *buffer, uint8_t tag, const Buffer *content) {
    assert(content->length < 128);
    const uint8_t header[] = {tag, (uint8_t)content->length};
    append(buffer, header, sizeof(header));
    append(buffer, content->bytes, content->length);
}

static void append_bytes(
    Buffer *buffer,
    uint8_t tag,
    const void *bytes,
    size_t length
) {
    Buffer content = {0};
    append(&content, bytes, length);
    append_tlv(buffer, tag, &content);
}

static void append_word(Buffer *buffer, uint32_t word) {
    const uint8_t bytes[] = {
        (uint8_t)word,
        (uint8_t)(word >> 8),
        (uint8_t)(word >> 16),
        (uint8_t)(word >> 24),
    };
    append_bytes(buffer, 0x04, bytes, sizeof(bytes));
}

static Buffer payload_data_entry(
    uint32_t key,
    const char *key_string,
    const uint8_t *data,
    size_t data_length
) {
    Buffer content = {0};
    append_word(&content, key);
    append_bytes(&content, 0x0c, key_string, strlen(key_string));
    append_bytes(&content, 0x04, data, data_length);
    Buffer encoded = {0};
    append_tlv(&encoded, 0x30, &content);
    return encoded;
}

static Buffer payload_deleted_entry(uint32_t key, const char *key_string) {
    Buffer content = {0};
    append_word(&content, key);
    append_bytes(&content, 0x0c, key_string, strlen(key_string));
    const uint8_t deleted = 0xff;
    append_bytes(&content, 0x01, &deleted, 1);
    Buffer encoded = {0};
    append_tlv(&encoded, 0x30, &content);
    return encoded;
}

static Buffer metadata_string_entry(const char *key, const char *value) {
    Buffer content = {0};
    append_bytes(&content, 0x0c, key, strlen(key));
    append_bytes(&content, 0x0c, value, strlen(value));
    Buffer encoded = {0};
    append_tlv(&encoded, 0x30, &content);
    return encoded;
}

static Buffer valid_sysconfig3(void) {
    const uint8_t data[] = {0xde, 0xad, 0xbe, 0xef};
    const Buffer data_entry =
        payload_data_entry(UINT32_C(0x61746164), "data", data, sizeof(data));
    const Buffer deleted_entry =
        payload_deleted_entry(UINT32_C(0x6c656464), "ddel");
    Buffer payload_content = {0};
    append(&payload_content, data_entry.bytes, data_entry.length);
    append(&payload_content, deleted_entry.bytes, deleted_entry.length);
    Buffer payload = {0};
    append_tlv(&payload, 0x31, &payload_content);

    const Buffer metadata_entry = metadata_string_entry("board", "j274");
    Buffer metadata_content = {0};
    append(&metadata_content, metadata_entry.bytes, metadata_entry.length);
    Buffer metadata = {0};
    append_tlv(&metadata, 0x31, &metadata_content);

    Buffer manifest_content = {0};
    const uint8_t payload_hmac[] = {1, 2, 3};
    const uint8_t metadata_hmac[] = {4, 5};
    const uint8_t full_hmac[] = {6, 7, 8, 9};
    append_bytes(
        &manifest_content,
        0x04,
        payload_hmac,
        sizeof(payload_hmac)
    );
    append_bytes(
        &manifest_content,
        0x04,
        metadata_hmac,
        sizeof(metadata_hmac)
    );
    append_bytes(&manifest_content, 0x04, full_hmac, sizeof(full_hmac));
    Buffer manifest = {0};
    append_tlv(&manifest, 0x30, &manifest_content);

    Buffer top_content = {0};
    append_word(&top_content, UINT32_C(0x33434653));
    append_word(&top_content, SYSCONFIG3_VERSION_2);
    append(&top_content, payload.bytes, payload.length);
    append(&top_content, metadata.bytes, metadata.length);
    append(&top_content, manifest.bytes, manifest.length);
    Buffer encoded = {0};
    append_tlv(&encoded, 0x30, &top_content);
    return encoded;
}

static void test_owned_model(void) {
    assert(allocSysConfig3(0, SYSCONFIG3_VERSION_1) == NULL);
    assert(allocSysConfig3(1, SYSCONFIG3_VERSION_1 - 1) == NULL);
    SysConfig3 *configuration =
        allocSysConfig3(UINT32_C(0x33434653), SYSCONFIG3_VERSION_1);
    assert(configuration != NULL);
    assert(sysConfig3GetVersion(configuration) == SYSCONFIG3_VERSION_1);

    uint8_t first_data[] = {1, 2, 3};
    SysConfig3PayloadEntry *first = _allocSysConfig3PayloadEntryInternal(
        UINT32_C(0x61746164),
        "data",
        first_data,
        sizeof(first_data),
        NULL,
        false
    );
    assert(first != NULL && _sysConfig3AddPayloadEntry(configuration, first));
    first_data[0] = 0xff;
    freeSysConfig3PayloadEntry(first);
    SysConfig3PayloadEntry *stored = sysConfig3GetPayloadEntryForKey(
        configuration,
        UINT32_C(0x61746164)
    );
    SysConfigData view;
    assert(stored != NULL && sysConfig3PayloadEntryGetData(stored, &view));
    assert(view.length == 3 && view.data[0] == 1);

    SysConfig3PayloadEntry *replacement =
        _allocSysConfig3PayloadEntryInternal(
            UINT32_C(0x61746164),
            "data",
            NULL,
            0,
            "replacement",
            false
        );
    assert(replacement != NULL &&
           _sysConfig3AddPayloadEntry(configuration, replacement));
    freeSysConfig3PayloadEntry(replacement);
    stored = sysConfig3GetPayloadEntryForKey(
        configuration,
        UINT32_C(0x61746164)
    );
    assert(strcmp(sysConfig3PayloadEntryGetString(stored), "replacement") ==
           0);
    assert(!sysConfig3PayloadEntryGetData(stored, &view));
    assert(_sysConfig3DeletePayloadEntryForKey(configuration,
                                               UINT32_C(0x61746164)));
    assert(sysConfig3GetPayloadEntryForKey(configuration,
                                           UINT32_C(0x61746164)) == NULL);

    SysConfig3MetadataEntry *metadata =
        allocSysConfig3MetadataEntry("board", NULL, 0, "j274");
    assert(metadata != NULL &&
           _sysConfig3AddMetadataEntry(configuration, metadata));
    metadata->string[0] = 'x';
    freeSysConfig3MetadataEntry(metadata);
    assert(strcmp(configuration->metadata_entries->string, "j274") == 0);
    assert(_sysConfig3DeleteMetadataEntryForKey(configuration, "board"));
    assert(configuration->metadata_entries == NULL);

    const uint8_t hmac_bytes[] = {0xaa, 0xbb};
    const SysConfigData hmac = {(uint8_t *)hmac_bytes, sizeof(hmac_bytes), 0};
    SysConfig3Manifest *manifest = allocSysConfig3Manifest(
        SYSCONFIG3_VERSION_1,
        &hmac,
        &hmac,
        &hmac
    );
    assert(manifest != NULL && _sysConfig3SetManifest(configuration, manifest));
    manifest->payload_hmac.data[0] = 0;
    assert(configuration->manifest->payload_hmac.data[0] == 0xaa);
    freeSysConfig3Manifest(manifest);
    freeSysConfig3(configuration);
}

static uint32_t random_state = UINT32_C(0x12345678);

static uint32_t next_random(void) {
    random_state ^= random_state << 13;
    random_state ^= random_state >> 17;
    random_state ^= random_state << 5;
    return random_state;
}

static void test_replacement_oracle(void) {
    SysConfig3 *configuration =
        allocSysConfig3(UINT32_C(0x33434653), SYSCONFIG3_VERSION_3);
    assert(configuration != NULL);
    bool present[32] = {false};
    char expected[32][24] = {{0}};
    for (size_t iteration = 0; iteration < 4096; ++iteration) {
        const uint32_t slot = next_random() % 32;
        const uint32_t key = UINT32_C(0x100) + slot + 1;
        if ((next_random() & 3U) == 0) {
            assert(_sysConfig3DeletePayloadEntryForKey(configuration, key));
            present[slot] = false;
        } else {
            char value[24];
            snprintf(value, sizeof(value), "value-%u-%zu", slot, iteration);
            SysConfig3PayloadEntry *entry =
                _allocSysConfig3PayloadEntryInternal(
                    key,
                    "oracle",
                    NULL,
                    0,
                    value,
                    false
                );
            assert(entry != NULL &&
                   _sysConfig3AddPayloadEntry(configuration, entry));
            freeSysConfig3PayloadEntry(entry);
            present[slot] = true;
            memcpy(expected[slot], value, sizeof(value));
        }
        for (uint32_t check = 0; check < 32; ++check) {
            SysConfig3PayloadEntry *entry = sysConfig3GetPayloadEntryForKey(
                configuration,
                UINT32_C(0x100) + check + 1
            );
            assert((entry != NULL) == present[check]);
            if (entry != NULL) {
                assert(strcmp(entry->string, expected[check]) == 0);
            }
        }
    }
    freeSysConfig3(configuration);
}

static void test_der_decoder(void) {
    Buffer encoded = valid_sysconfig3();
    SysConfig3 *configuration =
        DERDecodeSysConfig3(encoded.bytes, (uint32_t)encoded.length);
    assert(configuration != NULL);
    assert(configuration->magic == UINT32_C(0x33434653));
    assert(configuration->version == SYSCONFIG3_VERSION_2);
    SysConfig3PayloadEntry *data = sysConfig3GetPayloadEntryForKey(
        configuration,
        UINT32_C(0x61746164)
    );
    SysConfigData view;
    assert(data != NULL && sysConfig3PayloadEntryGetData(data, &view));
    assert(view.length == 4 && view.data[0] == 0xde && view.data[3] == 0xef);
    SysConfig3PayloadEntry *deleted = sysConfig3GetPayloadEntryForKey(
        configuration,
        UINT32_C(0x6c656464)
    );
    assert(deleted != NULL && sysConfig3PayloadEntryIsDeleted(deleted));
    assert(configuration->metadata_entries != NULL);
    assert(strcmp(configuration->metadata_entries->key, "board") == 0);
    assert(strcmp(configuration->metadata_entries->string, "j274") == 0);
    assert(configuration->manifest->payload_hmac.length == 3);
    assert(configuration->manifest->metadata_hmac.length == 2);
    assert(configuration->manifest->full_hmac.length == 4);
    freeSysConfig3(configuration);

    assert(DERDecodeSysConfig3(encoded.bytes,
                               (uint32_t)encoded.length - 1) == NULL);
    encoded.bytes[0] = 0x31;
    assert(DERDecodeSysConfig3(encoded.bytes, (uint32_t)encoded.length) ==
           NULL);
}

int main(void) {
    test_owned_model();
    test_replacement_oracle();
    test_der_decoder();
    puts("sysconfig3 compatibility tests passed");
}

