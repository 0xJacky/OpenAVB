#include "avp/platform/configuration/sysconfig3_compat.h"

#include <string.h>

_Static_assert(sizeof(SysConfigData) == 16, "SysConfigData ABI size");
_Static_assert(sizeof(SysConfig3PayloadEntry) == 48,
               "SysConfig3 payload entry ABI size");
_Static_assert(offsetof(SysConfig3PayloadEntry, next) == 0x28,
               "SysConfig3 payload next offset");
_Static_assert(sizeof(SysConfig3MetadataEntry) == 40,
               "SysConfig3 metadata entry ABI size");
_Static_assert(offsetof(SysConfig3MetadataEntry, next) == 0x20,
               "SysConfig3 metadata next offset");
_Static_assert(sizeof(SysConfig3Manifest) == 56,
               "SysConfig3 manifest ABI size");
_Static_assert(sizeof(SysConfig3) == 48, "SysConfig3 ABI size");

static size_t bounded_string_length(const char *string, size_t maximum_length) {
    size_t length = 0;
    while (length < maximum_length && string[length] != '\0') {
        ++length;
    }
    return length;
}

static void *allocate_zeroed(size_t size) {
    uint8_t *allocation = sysconfig3_allocate_owned(size);
    if (allocation == NULL) {
        return NULL;
    }
    for (size_t index = 0; index < size; ++index) {
        allocation[index] = 0;
    }
    return allocation;
}

static bool bounded_strings_equal(
    const char *left,
    const char *right,
    size_t maximum_length
) {
    for (size_t index = 0; index < maximum_length; ++index) {
        if (left[index] != right[index]) {
            return false;
        }
        if (left[index] == '\0') {
            return true;
        }
    }
    return true;
}

char *SYSCONFIG3_PUBLIC _allocAndCopyDERString(const DERItem *string) {
    if (string == NULL || string->data == NULL || string->length == SIZE_MAX) {
        return NULL;
    }
    char *copy = sysconfig3_allocate_owned(string->length + 1);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, string->data, string->length);
    copy[string->length] = '\0';
    return copy;
}

char *SYSCONFIG3_PUBLIC _allocAndCopyString(
    const char *string,
    size_t maximum_length
) {
    if (string == NULL) {
        return NULL;
    }
    const size_t length = bounded_string_length(string, maximum_length);
    if (length == SIZE_MAX) {
        return NULL;
    }
    char *copy = sysconfig3_allocate_owned(length + 1);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, string, length);
    copy[length] = '\0';
    return copy;
}

SysConfigData SYSCONFIG3_PUBLIC _allocAndCopyData(
    const void *data,
    uint32_t length
) {
    SysConfigData copy = {0};
    if (length == 0 || data == NULL) {
        return copy;
    }
    copy.data = sysconfig3_allocate_owned(length);
    if (copy.data == NULL) {
        return (SysConfigData){0};
    }
    memcpy(copy.data, data, length);
    copy.length = length;
    return copy;
}

SysConfig3PayloadEntry *SYSCONFIG3_PUBLIC
_allocSysConfig3PayloadEntryInternal(
    uint32_t key,
    const char *key_string,
    const void *data,
    uint32_t data_length,
    const char *string,
    bool deleted
) {
    const unsigned value_count = (data != NULL ? 1U : 0U) +
                                 (string != NULL ? 1U : 0U) +
                                 (deleted ? 1U : 0U);
    if ((key & UINT32_C(0xff)) == 0 || value_count != 1 ||
        (data != NULL && data_length == 0)) {
        return NULL;
    }

    SysConfig3PayloadEntry *entry = allocate_zeroed(sizeof(*entry));
    if (entry == NULL) {
        return NULL;
    }
    entry->key = key;
    entry->key_string = _allocAndCopyString(
        key_string != NULL && key_string[0] != '\0' ? key_string : "",
        UINT32_C(0x01000000));
    if (entry->key_string == NULL) {
        freeSysConfig3PayloadEntry(entry);
        return NULL;
    }
    if (data != NULL) {
        const SysConfigData copied = _allocAndCopyData(data, data_length);
        entry->data = copied.data;
        entry->data_length = copied.length;
        if (entry->data == NULL) {
            freeSysConfig3PayloadEntry(entry);
            return NULL;
        }
    }
    if (string != NULL) {
        entry->string = _allocAndCopyString(string, UINT32_C(0x01000000));
        if (entry->string == NULL) {
            freeSysConfig3PayloadEntry(entry);
            return NULL;
        }
    }
    entry->deleted = deleted;
    return entry;
}

void SYSCONFIG3_PUBLIC
freeSysConfig3PayloadEntry(SysConfig3PayloadEntry *entry) {
    if (entry == NULL) {
        return;
    }
    entry->key = 0;
    sysconfig3_release_owned(entry->key_string);
    sysconfig3_release_owned(entry->data);
    sysconfig3_release_owned(entry->string);
    sysconfig3_release_owned(entry);
}

bool SYSCONFIG3_PUBLIC
sysConfig3PayloadEntryIsDeleted(const SysConfig3PayloadEntry *entry) {
    return entry != NULL && entry->deleted;
}

const char *SYSCONFIG3_PUBLIC
sysConfig3PayloadEntryGetString(const SysConfig3PayloadEntry *entry) {
    return entry != NULL ? entry->string : NULL;
}

bool SYSCONFIG3_PUBLIC
sysconfig3_payload_entry_get_data_invalid_arguments(void) {
    return false;
}

bool SYSCONFIG3_PUBLIC
sysConfig3PayloadEntryGetData(
    const SysConfig3PayloadEntry *entry,
    SysConfigData *data
) {
    if (entry == NULL || data == NULL) {
        return sysconfig3_payload_entry_get_data_invalid_arguments();
    }
    if (entry->data == NULL) {
        return false;
    }
    *data = (SysConfigData){entry->data, entry->data_length, 0};
    return true;
}

SysConfig3MetadataEntry *SYSCONFIG3_PUBLIC allocSysConfig3MetadataEntry(
    const char *key,
    const void *data,
    uint32_t data_length,
    const char *string
) {
    if (key == NULL || (data == NULL && string == NULL) ||
        (data != NULL && (data_length == 0 || string != NULL))) {
        return NULL;
    }
    SysConfig3MetadataEntry *entry = allocate_zeroed(sizeof(*entry));
    if (entry == NULL) {
        return NULL;
    }
    entry->key = _allocAndCopyString(key, UINT32_C(0x100));
    if (entry->key == NULL) {
        freeSysConfig3MetadataEntry(entry);
        return NULL;
    }
    if (data != NULL) {
        const SysConfigData copied = _allocAndCopyData(data, data_length);
        entry->data = copied.data;
        entry->data_length = copied.length;
        if (entry->data == NULL) {
            freeSysConfig3MetadataEntry(entry);
            return NULL;
        }
    }
    if (string != NULL) {
        entry->string = _allocAndCopyString(string, UINT32_C(0x01000000));
        if (entry->string == NULL) {
            freeSysConfig3MetadataEntry(entry);
            return NULL;
        }
    }
    return entry;
}

void SYSCONFIG3_PUBLIC
freeSysConfig3MetadataEntry(SysConfig3MetadataEntry *entry) {
    if (entry == NULL) {
        return;
    }
    sysconfig3_release_owned(entry->key);
    sysconfig3_release_owned(entry->data);
    sysconfig3_release_owned(entry->string);
    sysconfig3_release_owned(entry);
}

bool SYSCONFIG3_PUBLIC _copySysConfigData(
    const SysConfigData *source,
    SysConfigData *destination
) {
    if (source == NULL || destination == NULL ||
        (source->data == NULL) != (source->length == 0)) {
        return false;
    }
    *destination = _allocAndCopyData(source->data, source->length);
    return source->length == 0 || destination->data != NULL;
}

SysConfig3Manifest *SYSCONFIG3_PUBLIC allocSysConfig3Manifest(
    uint32_t version,
    const SysConfigData *payload_hmac,
    const SysConfigData *metadata_hmac,
    const SysConfigData *full_hmac
) {
    if (payload_hmac == NULL || metadata_hmac == NULL || full_hmac == NULL) {
        return NULL;
    }
    SysConfig3Manifest *manifest = allocate_zeroed(sizeof(*manifest));
    if (manifest == NULL) {
        return NULL;
    }
    manifest->version = version;
    if (!_copySysConfigData(payload_hmac, &manifest->payload_hmac) ||
        !_copySysConfigData(metadata_hmac, &manifest->metadata_hmac) ||
        !_copySysConfigData(full_hmac, &manifest->full_hmac)) {
        freeSysConfig3Manifest(manifest);
        return NULL;
    }
    return manifest;
}

SysConfig3Manifest *SYSCONFIG3_PUBLIC
_allocSysConfig3ManifestFromDecodedSequence(
    const SysConfig3DecodedManifest *decoded) {
    if (decoded == NULL) {
        return NULL;
    }
    return allocSysConfig3Manifest(
        decoded->version,
        &decoded->payload_hmac,
        &decoded->metadata_hmac,
        &decoded->full_hmac
    );
}

void SYSCONFIG3_PUBLIC freeSysConfig3Manifest(SysConfig3Manifest *manifest) {
    if (manifest == NULL) {
        return;
    }
    sysconfig3_release_owned(manifest->payload_hmac.data);
    sysconfig3_release_owned(manifest->metadata_hmac.data);
    sysconfig3_release_owned(manifest->full_hmac.data);
    sysconfig3_release_owned(manifest);
}

SysConfig3 *SYSCONFIG3_PUBLIC allocSysConfig3(
    uint32_t magic,
    uint32_t version
) {
    if ((magic & UINT32_C(0xff)) == 0 || version < SYSCONFIG3_VERSION_1 ||
        version > SYSCONFIG3_VERSION_3) {
        return NULL;
    }
    SysConfig3 *configuration = allocate_zeroed(sizeof(*configuration));
    if (configuration == NULL) {
        return NULL;
    }
    configuration->magic = magic;
    configuration->version = version;
    const SysConfigData empty = {0};
    SysConfig3Manifest *manifest =
        allocSysConfig3Manifest(version, &empty, &empty, &empty);
    if (manifest == NULL || !_sysConfig3SetManifest(configuration, manifest)) {
        freeSysConfig3Manifest(manifest);
        freeSysConfig3(configuration);
        return NULL;
    }
    freeSysConfig3Manifest(manifest);
    return configuration;
}

bool SYSCONFIG3_PUBLIC _sysConfig3SetManifest(
    SysConfig3 *configuration,
    const SysConfig3Manifest *manifest
) {
    if (configuration == NULL || manifest == NULL ||
        configuration->version != manifest->version) {
        return false;
    }
    freeSysConfig3Manifest(configuration->manifest);
    configuration->manifest = allocSysConfig3Manifest(
        manifest->version,
        &manifest->payload_hmac,
        &manifest->metadata_hmac,
        &manifest->full_hmac
    );
    return configuration->manifest != NULL;
}

void SYSCONFIG3_PUBLIC freeSysConfig3(SysConfig3 *configuration) {
    if (configuration == NULL) {
        return;
    }
    while (configuration->payload_entries != NULL) {
        SysConfig3PayloadEntry *entry = configuration->payload_entries;
        configuration->payload_entries = entry->next;
        freeSysConfig3PayloadEntry(entry);
    }
    while (configuration->metadata_entries != NULL) {
        SysConfig3MetadataEntry *entry = configuration->metadata_entries;
        configuration->metadata_entries = entry->next;
        freeSysConfig3MetadataEntry(entry);
    }
    freeSysConfig3Manifest(configuration->manifest);
    sysconfig3_release_owned(configuration);
}

uint32_t SYSCONFIG3_PUBLIC
sysConfig3GetVersion(const SysConfig3 *configuration) {
    return configuration != NULL ? configuration->version : 0;
}

SysConfig3PayloadEntry *SYSCONFIG3_PUBLIC sysConfig3GetPayloadEntryForKey(
    const SysConfig3 *configuration,
    uint32_t key
) {
    if (configuration == NULL) {
        return NULL;
    }
    for (SysConfig3PayloadEntry *entry = configuration->payload_entries;
         entry != NULL; entry = entry->next) {
        if (entry->key == key) {
            return entry;
        }
    }
    return NULL;
}

bool SYSCONFIG3_PUBLIC
_sysConfig3DeletePayloadEntryForKey(SysConfig3 *configuration, uint32_t key) {
    if (configuration == NULL) {
        return false;
    }
    SysConfig3PayloadEntry **link = &configuration->payload_entries;
    while (*link != NULL) {
        SysConfig3PayloadEntry *entry = *link;
        if (entry->key == key) {
            *link = entry->next;
            freeSysConfig3PayloadEntry(entry);
        } else {
            link = &entry->next;
        }
    }
    return true;
}

bool SYSCONFIG3_PUBLIC _sysConfig3DeleteMetadataEntryForKey(
    SysConfig3 *configuration,
    const char *key
) {
    if (configuration == NULL || key == NULL) {
        return false;
    }
    SysConfig3MetadataEntry **link = &configuration->metadata_entries;
    while (*link != NULL) {
        SysConfig3MetadataEntry *entry = *link;
        if (bounded_strings_equal(entry->key, key, UINT32_C(0x100))) {
            *link = entry->next;
            freeSysConfig3MetadataEntry(entry);
        } else {
            link = &entry->next;
        }
    }
    return true;
}

bool SYSCONFIG3_PUBLIC _sysConfig3AddPayloadEntry(
    SysConfig3 *configuration,
    const SysConfig3PayloadEntry *entry
) {
    if (configuration == NULL || entry == NULL ||
        !_sysConfig3DeletePayloadEntryForKey(configuration, entry->key)) {
        return false;
    }
    SysConfig3PayloadEntry *copy = _allocSysConfig3PayloadEntryInternal(
        entry->key,
        entry->key_string,
        entry->data,
        entry->data_length,
        entry->string,
        entry->deleted
    );
    if (copy == NULL) {
        return false;
    }
    SysConfig3PayloadEntry **tail = &configuration->payload_entries;
    while (*tail != NULL) {
        tail = &(*tail)->next;
    }
    *tail = copy;
    return true;
}

bool SYSCONFIG3_PUBLIC _sysConfig3AddMetadataEntry(
    SysConfig3 *configuration,
    const SysConfig3MetadataEntry *entry
) {
    if (configuration == NULL || entry == NULL ||
        !_sysConfig3DeleteMetadataEntryForKey(configuration, entry->key)) {
        return false;
    }
    SysConfig3MetadataEntry *copy = allocSysConfig3MetadataEntry(
        entry->key, entry->data, entry->data_length, entry->string);
    if (copy == NULL) {
        return false;
    }
    SysConfig3MetadataEntry **tail = &configuration->metadata_entries;
    while (*tail != NULL) {
        tail = &(*tail)->next;
    }
    *tail = copy;
    return true;
}
