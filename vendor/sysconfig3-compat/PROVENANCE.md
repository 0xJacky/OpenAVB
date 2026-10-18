# SysConfig3 compatibility provenance

This module uses Apple's public libDER decoder already vendored by OpenAVB.
The SysConfig3 schema, ownership rules, and mutation behaviour follow the
ROM interface and are implemented in original OpenAVB code.

## Public upstream

- Project: Apple Security / libDER
- Tag: `Security-57740.60.18`
- Commit: `db455f62ee15e6adf20afe7c2e132321d2d0759c`
- License: Apple Public Source License 2.0
- Decoder and sequence parser:
  https://github.com/apple-oss-distributions/Security/blob/Security-57740.60.18/OSX/libsecurity_keychain/libDER/libDER/DER_Decode.c
- Public interface:
  https://github.com/apple-oss-distributions/Security/blob/Security-57740.60.18/OSX/libsecurity_keychain/libDER/libDER/libDER.h
- License: https://opensource.apple.com/apsl/

## Exported functions

- `DERDecodeSysConfig3`: decode the complete payload, metadata, and manifest
  object transactionally.
- `allocSysConfig3`, `freeSysConfig3`: validate magic/version and construct
  an empty configuration; drain both entry lists and all root ownership.
- `sysConfig3GetVersion`: read the null-safe configuration version.
- `sysConfig3GetPayloadEntryForKey`: find a payload entry by numeric key.
- `freeSysConfig3PayloadEntry`: release every payload-owned field and the
  entry.
- `sysConfig3PayloadEntryIsDeleted`, `sysConfig3PayloadEntryGetString`:
  read the null-safe deletion marker or optional string.
- `sysConfig3PayloadEntryGetData`: publish a borrowed data span when
  present.
- `allocSysConfig3MetadataEntry`, `freeSysConfig3MetadataEntry`: construct
  or release one owned data or string metadata value.
- `allocSysConfig3Manifest`, `freeSysConfig3Manifest`: deep-copy or release
  the three manifest HMAC spans.

Internal helpers decode and append payload and metadata entries, iterate a
DER SET/SEQUENCE with callback stop semantics, deep-copy strings, data, and
SysConfigData spans, and add, replace, or delete payload and metadata
entries by key.
