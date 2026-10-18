#pragma once

#include "avp/image/der/apple_libder.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if __STDC_HOSTED__
#include <stdlib.h>

static inline void *sysconfig3_allocate_owned(size_t size) {
    return malloc(size);
}

static inline void sysconfig3_release_owned(void *allocation) {
    free(allocation);
}
#else
#include "avp/runtime/memory/allocator/runtime_allocator_array.h"

static inline void *sysconfig3_allocate_owned(size_t size) {
    RuntimeAllocatorRoute *route = runtime_allocator_select_available_route();
    return runtime_allocator_allocate_bytes(route, size).current;
}

static inline void sysconfig3_release_owned(void *allocation) {
    runtime_allocator_release_address(allocation);
}
#endif

enum {
    SYSCONFIG3_VERSION_1 = UINT32_C(0x00300001),
    SYSCONFIG3_VERSION_2 = UINT32_C(0x00300002),
    SYSCONFIG3_VERSION_3 = UINT32_C(0x00300003),
};

typedef struct {
    uint8_t *data;
    uint32_t length;
    uint32_t reserved;
} SysConfigData;

typedef struct SysConfig3PayloadEntry {
    uint32_t key;
    bool deleted;
    uint8_t reserved[3];
    char *key_string;
    char *string;
    uint8_t *data;
    uint32_t data_length;
    uint32_t data_reserved;
    struct SysConfig3PayloadEntry *next;
} SysConfig3PayloadEntry;

typedef struct SysConfig3MetadataEntry {
    char *key;
    char *string;
    uint8_t *data;
    uint32_t data_length;
    uint32_t data_reserved;
    struct SysConfig3MetadataEntry *next;
} SysConfig3MetadataEntry;

typedef struct {
    uint32_t version;
    uint32_t reserved;
    SysConfigData payload_hmac;
    SysConfigData metadata_hmac;
    SysConfigData full_hmac;
} SysConfig3Manifest;

typedef struct {
    uint32_t magic;
    uint32_t version;
    SysConfig3PayloadEntry *payload_entries;
    SysConfig3MetadataEntry *metadata_entries;
    SysConfig3Manifest *manifest;
    uint64_t reserved_20;
    uint64_t reserved_28;
} SysConfig3;

typedef DERReturn (*SysConfig3DEREntryCallback)(
    const DERDecodedInfo *entry,
    bool *stop,
    SysConfig3 *configuration
);

typedef struct {
    uint32_t version;
    uint32_t reserved;
    SysConfigData payload_hmac;
    SysConfigData metadata_hmac;
    SysConfigData full_hmac;
} SysConfig3DecodedManifest;

#define SYSCONFIG3_PUBLIC __attribute__((used, retain))

SYSCONFIG3_PUBLIC char *_allocAndCopyDERString(const DERItem *string);
SYSCONFIG3_PUBLIC char *_allocAndCopyString(
    const char *string,
    size_t maximum_length
);
SYSCONFIG3_PUBLIC SysConfigData _allocAndCopyData(
    const void *data,
    uint32_t length
);

SYSCONFIG3_PUBLIC SysConfig3PayloadEntry *_allocSysConfig3PayloadEntryInternal(
    uint32_t key,
    const char *key_string,
    const void *data,
    uint32_t data_length,
    const char *string,
    bool deleted
);
SYSCONFIG3_PUBLIC void
freeSysConfig3PayloadEntry(SysConfig3PayloadEntry *entry);
SYSCONFIG3_PUBLIC bool
sysConfig3PayloadEntryIsDeleted(const SysConfig3PayloadEntry *entry);
SYSCONFIG3_PUBLIC const char *
sysConfig3PayloadEntryGetString(const SysConfig3PayloadEntry *entry);
SYSCONFIG3_PUBLIC bool
sysConfig3PayloadEntryGetData(
    const SysConfig3PayloadEntry *entry,
    SysConfigData *data
);
SYSCONFIG3_PUBLIC bool
sysconfig3_payload_entry_get_data_invalid_arguments(void);

SYSCONFIG3_PUBLIC SysConfig3MetadataEntry *allocSysConfig3MetadataEntry(
    const char *key,
    const void *data,
    uint32_t data_length,
    const char *string
);
SYSCONFIG3_PUBLIC void
freeSysConfig3MetadataEntry(SysConfig3MetadataEntry *entry);

SYSCONFIG3_PUBLIC SysConfig3Manifest *allocSysConfig3Manifest(
    uint32_t version,
    const SysConfigData *payload_hmac,
    const SysConfigData *metadata_hmac,
    const SysConfigData *full_hmac
);
SYSCONFIG3_PUBLIC bool _copySysConfigData(
    const SysConfigData *source,
    SysConfigData *destination
);
SYSCONFIG3_PUBLIC SysConfig3Manifest *
_allocSysConfig3ManifestFromDecodedSequence(
    const SysConfig3DecodedManifest *decoded);
SYSCONFIG3_PUBLIC void freeSysConfig3Manifest(SysConfig3Manifest *manifest);

SYSCONFIG3_PUBLIC SysConfig3 *allocSysConfig3(
    uint32_t magic,
    uint32_t version
);
SYSCONFIG3_PUBLIC bool _sysConfig3SetManifest(
    SysConfig3 *configuration,
    const SysConfig3Manifest *manifest
);
SYSCONFIG3_PUBLIC void freeSysConfig3(SysConfig3 *configuration);
SYSCONFIG3_PUBLIC uint32_t
sysConfig3GetVersion(const SysConfig3 *configuration);
SYSCONFIG3_PUBLIC SysConfig3PayloadEntry *sysConfig3GetPayloadEntryForKey(
    const SysConfig3 *configuration,
    uint32_t key
);
SYSCONFIG3_PUBLIC bool
_sysConfig3DeletePayloadEntryForKey(SysConfig3 *configuration, uint32_t key);
SYSCONFIG3_PUBLIC bool _sysConfig3DeleteMetadataEntryForKey(
    SysConfig3 *configuration,
    const char *key
);
SYSCONFIG3_PUBLIC bool _sysConfig3AddPayloadEntry(
    SysConfig3 *configuration,
    const SysConfig3PayloadEntry *entry
);
SYSCONFIG3_PUBLIC bool _sysConfig3AddMetadataEntry(
    SysConfig3 *configuration,
    const SysConfig3MetadataEntry *entry
);

SYSCONFIG3_PUBLIC DERReturn _DERDecodeSysConfig3ParseSequence(
    const DERItem *encoded_set,
    SysConfig3DEREntryCallback callback,
    SysConfig3 *configuration
);
SYSCONFIG3_PUBLIC DERReturn
_DERDecodeSysConfig3ParseAndPopulatePayloadSequence(
    const DERDecodedInfo *entry,
    bool *stop,
    SysConfig3 *configuration
);
SYSCONFIG3_PUBLIC DERReturn
_DERDecodeSysConfig3ParseAndPopulateMetadataSequence(
    const DERDecodedInfo *entry,
    bool *stop,
    SysConfig3 *configuration
);
SYSCONFIG3_PUBLIC SysConfig3 *DERDecodeSysConfig3(
    const uint8_t *encoded,
    uint32_t length
);
