#include "avp/platform/configuration/sysconfig3_compat.h"

#include <stddef.h>

enum {
    ASN1_BOOLEAN = 1,
    ASN1_INTEGER = 2,
    ASN1_OCTET_STRING = 4,
    ASN1_UTF8_STRING = 12,
    ASN1_PRINTABLE_STRING = 19,
    ASN1_IA5_STRING = 22,
};

typedef struct {
    DERItem key;
    DERItem key_string;
    DERItem data;
    DERItem string;
    DERItem deleted;
} DecodedPayloadEntry;

typedef struct {
    DERItem key;
    DERItem data;
    DERItem string;
} DecodedMetadataEntry;

static const DERItemSpec payload_entry_specs[] = {
    {offsetof(DecodedPayloadEntry, key), ASN1_OCTET_STRING, DER_DEC_NO_OPTS},
    {offsetof(DecodedPayloadEntry, key_string), ASN1_UTF8_STRING,
     DER_DEC_NO_OPTS},
    {offsetof(DecodedPayloadEntry, data), ASN1_OCTET_STRING, DER_DEC_OPTIONAL},
    {offsetof(DecodedPayloadEntry, string), ASN1_UTF8_STRING,
     DER_DEC_OPTIONAL},
    {offsetof(DecodedPayloadEntry, deleted), ASN1_BOOLEAN, DER_DEC_OPTIONAL},
};

static const DERItemSpec metadata_entry_specs[] = {
    {offsetof(DecodedMetadataEntry, key), ASN1_UTF8_STRING, DER_DEC_NO_OPTS},
    {offsetof(DecodedMetadataEntry, data), ASN1_OCTET_STRING, DER_DEC_OPTIONAL},
    {offsetof(DecodedMetadataEntry, string), ASN1_UTF8_STRING,
     DER_DEC_OPTIONAL},
};

static uint32_t load_little_endian_u32(const uint8_t *bytes) {
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

static DERReturn parse_word(const DERDecodedInfo *item, uint32_t *value) {
    if (item->tag == ASN1_INTEGER) {
        return DERParseInteger(&item->content, value);
    }
    if (item->tag != ASN1_OCTET_STRING || item->content.length != 4) {
        return DR_UnexpectedTag;
    }
    *value = load_little_endian_u32(item->content.data);
    return DR_Success;
}

static bool is_string_tag(DERTag tag) {
    return tag == ASN1_UTF8_STRING || tag == ASN1_PRINTABLE_STRING ||
           tag == ASN1_IA5_STRING;
}

static DERReturn decode_payload_entry(
    const DERDecodedInfo *entry,
    DecodedPayloadEntry *decoded
) {
    if (entry->tag != ASN1_CONSTR_SEQUENCE) {
        return DR_UnexpectedTag;
    }
    DERReturn result = DERParseSequenceContent(
        &entry->content,
        (DERShort)(sizeof(payload_entry_specs) /
                   sizeof(payload_entry_specs[0])),
        payload_entry_specs,
        decoded,
        sizeof(*decoded)
    );
    if (result == DR_Success) {
        return result;
    }

    /* SysConfig3 accepts the DER character-string tags used by older
     * manifests while retaining strict field ordering and value exclusivity. */
    *decoded = (DecodedPayloadEntry){0};
    DERSequence sequence;
    DERDecodeSeqContentInit(&entry->content, &sequence);
    DERDecodedInfo item;
    result = DERDecodeSeqNext(&sequence, &item);
    if (result != DR_Success || item.tag != ASN1_OCTET_STRING) {
        return result == DR_Success ? DR_UnexpectedTag : result;
    }
    decoded->key = item.content;
    result = DERDecodeSeqNext(&sequence, &item);
    if (result != DR_Success || !is_string_tag(item.tag)) {
        return result == DR_Success ? DR_UnexpectedTag : result;
    }
    decoded->key_string = item.content;
    result = DERDecodeSeqNext(&sequence, &item);
    if (result != DR_Success) {
        return result;
    }
    if (item.tag == ASN1_OCTET_STRING) {
        decoded->data = item.content;
    } else if (is_string_tag(item.tag)) {
        decoded->string = item.content;
    } else if (item.tag == ASN1_BOOLEAN) {
        decoded->deleted = item.content;
    } else {
        return DR_UnexpectedTag;
    }
    return DERDecodeSeqNext(&sequence, &item) == DR_EndOfSequence
               ? DR_Success
               : DR_DecodeError;
}

static DERReturn decode_metadata_entry(
    const DERDecodedInfo *entry,
    DecodedMetadataEntry *decoded
) {
    if (entry->tag != ASN1_CONSTR_SEQUENCE) {
        return DR_UnexpectedTag;
    }
    DERReturn result = DERParseSequenceContent(
        &entry->content,
        (DERShort)(sizeof(metadata_entry_specs) /
                   sizeof(metadata_entry_specs[0])),
        metadata_entry_specs,
        decoded,
        sizeof(*decoded)
    );
    if (result == DR_Success) {
        return result;
    }
    *decoded = (DecodedMetadataEntry){0};
    DERSequence sequence;
    DERDecodeSeqContentInit(&entry->content, &sequence);
    DERDecodedInfo item;
    result = DERDecodeSeqNext(&sequence, &item);
    if (result != DR_Success || !is_string_tag(item.tag)) {
        return result == DR_Success ? DR_UnexpectedTag : result;
    }
    decoded->key = item.content;
    result = DERDecodeSeqNext(&sequence, &item);
    if (result != DR_Success) {
        return result;
    }
    if (item.tag == ASN1_OCTET_STRING) {
        decoded->data = item.content;
    } else if (is_string_tag(item.tag)) {
        decoded->string = item.content;
    } else {
        return DR_UnexpectedTag;
    }
    return DERDecodeSeqNext(&sequence, &item) == DR_EndOfSequence
               ? DR_Success
               : DR_DecodeError;
}

static DERReturn run_entry_callbacks(
    DERSequence *sequence,
    SysConfig3DEREntryCallback callback,
    SysConfig3 *configuration
) {
    for (;;) {
        DERDecodedInfo entry;
        DERReturn result = DERDecodeSeqNext(sequence, &entry);
        if (result == DR_EndOfSequence) {
            return DR_Success;
        }
        if (result != DR_Success) {
            return result;
        }
        bool stop = false;
        result = callback(&entry, &stop, configuration);
        if (result != DR_Success || stop) {
            return result;
        }
    }
}

DERReturn SYSCONFIG3_PUBLIC _DERDecodeSysConfig3ParseSequence(
    const DERItem *encoded_set,
    SysConfig3DEREntryCallback callback,
    SysConfig3 *configuration
) {
    if (encoded_set == NULL || callback == NULL || configuration == NULL) {
        return DR_ParamErr;
    }
    DERTag tag;
    DERSequence sequence;
    DERReturn result = DERDecodeSeqInit(encoded_set, &tag, &sequence);
    if (result != DR_Success) {
        return result;
    }
    if (tag != ASN1_CONSTR_SET && tag != ASN1_CONSTR_SEQUENCE) {
        return DR_UnexpectedTag;
    }
    return run_entry_callbacks(&sequence, callback, configuration);
}

DERReturn SYSCONFIG3_PUBLIC
_DERDecodeSysConfig3ParseAndPopulatePayloadSequence(
    const DERDecodedInfo *entry,
    bool *stop,
    SysConfig3 *configuration
) {
    if (entry == NULL || stop == NULL || configuration == NULL) {
        return DR_ParamErr;
    }
    DecodedPayloadEntry decoded;
    DERReturn result = decode_payload_entry(entry, &decoded);
    if (result != DR_Success) {
        return result;
    }
    if (decoded.key.length != 4 || decoded.key.data == NULL ||
        decoded.key_string.data == NULL) {
        return DR_DecodeError;
    }
    bool deleted = false;
    if (decoded.deleted.data != NULL) {
        result = DERParseBoolean(&decoded.deleted, &deleted);
        if (result != DR_Success || !deleted) {
            return result == DR_Success ? DR_DecodeError : result;
        }
    }
    char *key_string = _allocAndCopyDERString(&decoded.key_string);
    char *string = decoded.string.data != NULL
                       ? _allocAndCopyDERString(&decoded.string)
                       : NULL;
    if (key_string == NULL ||
        (decoded.string.data != NULL && string == NULL)) {
        sysconfig3_release_owned(key_string);
        sysconfig3_release_owned(string);
        return DR_BufOverflow;
    }
    SysConfig3PayloadEntry *allocated = _allocSysConfig3PayloadEntryInternal(
        load_little_endian_u32(decoded.key.data),
        key_string,
        decoded.data.data,
        (uint32_t)decoded.data.length,
        string,
        deleted
    );
    sysconfig3_release_owned(key_string);
    sysconfig3_release_owned(string);
    if (allocated == NULL) {
        return DR_DecodeError;
    }
    const bool added = _sysConfig3AddPayloadEntry(configuration, allocated);
    freeSysConfig3PayloadEntry(allocated);
    if (!added) {
        return DR_BufOverflow;
    }
    *stop = false;
    return DR_Success;
}

DERReturn SYSCONFIG3_PUBLIC
_DERDecodeSysConfig3ParseAndPopulateMetadataSequence(
    const DERDecodedInfo *entry,
    bool *stop,
    SysConfig3 *configuration
) {
    if (entry == NULL || stop == NULL || configuration == NULL) {
        return DR_ParamErr;
    }
    DecodedMetadataEntry decoded;
    DERReturn result = decode_metadata_entry(entry, &decoded);
    if (result != DR_Success || decoded.key.data == NULL) {
        return result == DR_Success ? DR_DecodeError : result;
    }
    char *key = _allocAndCopyDERString(&decoded.key);
    char *string = decoded.string.data != NULL
                       ? _allocAndCopyDERString(&decoded.string)
                       : NULL;
    if (key == NULL || (decoded.string.data != NULL && string == NULL)) {
        sysconfig3_release_owned(key);
        sysconfig3_release_owned(string);
        return DR_BufOverflow;
    }
    SysConfig3MetadataEntry *allocated = allocSysConfig3MetadataEntry(
        key, decoded.data.data, (uint32_t)decoded.data.length, string);
    sysconfig3_release_owned(key);
    sysconfig3_release_owned(string);
    if (allocated == NULL) {
        return DR_DecodeError;
    }
    const bool added = _sysConfig3AddMetadataEntry(configuration, allocated);
    freeSysConfig3MetadataEntry(allocated);
    if (!added) {
        return DR_BufOverflow;
    }
    *stop = false;
    return DR_Success;
}

static DERReturn parse_collection(
    const DERDecodedInfo *collection,
    SysConfig3DEREntryCallback callback,
    SysConfig3 *configuration
) {
    if (collection->tag != ASN1_CONSTR_SET &&
        collection->tag != ASN1_CONSTR_SEQUENCE) {
        return DR_UnexpectedTag;
    }
    DERSequence sequence;
    DERDecodeSeqContentInit(&collection->content, &sequence);
    return run_entry_callbacks(&sequence, callback, configuration);
}

static DERReturn decode_manifest(
    const DERDecodedInfo *entry,
    uint32_t version,
    SysConfig3Manifest **manifest
) {
    if (entry->tag != ASN1_CONSTR_SEQUENCE) {
        return DR_UnexpectedTag;
    }
    DERSequence sequence;
    DERDecodeSeqContentInit(&entry->content, &sequence);
    SysConfigData fields[3] = {{0}};
    for (size_t index = 0; index < 3; ++index) {
        DERDecodedInfo item;
        DERReturn result = DERDecodeSeqNext(&sequence, &item);
        if (result != DR_Success || item.tag != ASN1_OCTET_STRING ||
            item.content.length > UINT32_MAX) {
            return result == DR_Success ? DR_UnexpectedTag : result;
        }
        fields[index].data = item.content.data;
        fields[index].length = (uint32_t)item.content.length;
    }
    DERDecodedInfo trailing;
    if (DERDecodeSeqNext(&sequence, &trailing) != DR_EndOfSequence) {
        return DR_DecodeError;
    }
    *manifest =
        allocSysConfig3Manifest(version, &fields[0], &fields[1], &fields[2]);
    return *manifest != NULL ? DR_Success : DR_BufOverflow;
}

SysConfig3 *SYSCONFIG3_PUBLIC DERDecodeSysConfig3(
    const uint8_t *encoded,
    uint32_t length
) {
    if (encoded == NULL || length == 0) {
        return NULL;
    }
    DERItem top = {(uint8_t *)encoded, length};
    DERTag tag;
    DERSequence sequence;
    if (DERDecodeSeqInit(&top, &tag, &sequence) != DR_Success ||
        tag != ASN1_CONSTR_SEQUENCE) {
        return NULL;
    }

    DERDecodedInfo fields[5];
    for (size_t index = 0; index < 5; ++index) {
        if (DERDecodeSeqNext(&sequence, &fields[index]) != DR_Success) {
            return NULL;
        }
    }
    DERDecodedInfo trailing;
    if (DERDecodeSeqNext(&sequence, &trailing) != DR_EndOfSequence) {
        return NULL;
    }

    uint32_t magic;
    uint32_t version;
    if (parse_word(&fields[0], &magic) != DR_Success ||
        parse_word(&fields[1], &version) != DR_Success) {
        return NULL;
    }
    SysConfig3 *configuration = allocSysConfig3(magic, version);
    if (configuration == NULL) {
        return NULL;
    }
    DERReturn result = parse_collection(
        &fields[2],
        _DERDecodeSysConfig3ParseAndPopulatePayloadSequence,
        configuration
    );
    if (result == DR_Success) {
        result = parse_collection(
            &fields[3],
            _DERDecodeSysConfig3ParseAndPopulateMetadataSequence,
            configuration
        );
    }
    SysConfig3Manifest *manifest = NULL;
    if (result == DR_Success) {
        result = decode_manifest(&fields[4], version, &manifest);
    }
    if (result == DR_Success && !_sysConfig3SetManifest(configuration, manifest)) {
        result = DR_BufOverflow;
    }
    freeSysConfig3Manifest(manifest);
    if (result != DR_Success) {
        freeSysConfig3(configuration);
        return NULL;
    }
    return configuration;
}
