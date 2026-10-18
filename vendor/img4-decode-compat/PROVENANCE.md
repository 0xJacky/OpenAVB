# IMG4 decode compatibility provenance

`src/image/img4/img4_decode_compat.c` adapts the PongoOS IMG4 parser schema
and its strict outer-length validation onto OpenAVB's libDER-compatible
decoder. The file carries the PongoOS MIT notice and an owngoal-dev
modification notice.

- Upstream: https://github.com/checkra1n/PongoOS
- Commit: `4c9b7541629234147fcc778f0ce4162482aaccef`
- Parser source: https://github.com/checkra1n/PongoOS/blob/4c9b7541629234147fcc778f0ce4162482aaccef/src/lib/img4/img4.c
- Schema header: https://github.com/checkra1n/PongoOS/blob/4c9b7541629234147fcc778f0ce4162482aaccef/src/lib/img4/img4.h
- License: MIT, Copyright (C) 2019-2021 checkra1n team,
  https://github.com/checkra1n/PongoOS/blob/4c9b7541629234147fcc778f0ce4162482aaccef/LICENSE.md
- DER decoding: `src/image/der/` (Apple libDER derived, APSL 2.0; see
  `vendor/apple-libder`)

Output layouts are `0x40` bytes for the container, `0x60` for the legacy
payload, `0xa0` for the extended payload scratch, `0xc0` for the manifest,
`0x20` for restore information, and `0x30` for a two-element property
result.

Exported entry points: `DERImg4Decode`, `DERImg4DecodePayloadWithProperties`,
`DERImg4DecodePayloadProperties`, `DERImg4DecodePayload`,
`DERImg4DecodeManifest`, `DERImg4DecodeManifestCommon`,
`DERImg4DecodeRestoreInfo`, `DERImg4DecodeProperty`,
`DERImg4DecodeFindPropertyInSequence`, `DERImg4DecodeContentFindItemWithTag`,
`DERImg4DecodePayloadCompression` and `DERImg4DecodePropertyWithItem`.
