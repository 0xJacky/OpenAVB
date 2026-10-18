# IMG4 context compatibility provenance

The context layout and accessor status values follow the ROM interface.
DER parsing reuses the adjacent OpenAVB IMG4 compatibility layer, which
adapts the public PongoOS IMG4 schema onto Apple Security libDER.

- PongoOS upstream: https://github.com/checkra1n/PongoOS
- PongoOS commit: `4c9b7541629234147fcc778f0ce4162482aaccef`
- IMG4 source:
  https://github.com/checkra1n/PongoOS/blob/4c9b7541629234147fcc778f0ce4162482aaccef/src/lib/img4/img4.c
- PongoOS license: MIT,
  https://github.com/checkra1n/PongoOS/blob/4c9b7541629234147fcc778f0ce4162482aaccef/LICENSE.md
- Apple libDER: Security `Security-57740.60.18`, commit
  `db455f62ee15e6adf20afe7c2e132321d2d0759c`, APSL 2.0

## Exported functions

- `Img4DecodeInit`: initialize the `0x1c8`-byte context from a complete
  IMG4 container.
- `DERImg4DecodeParseManifestProperties`: select the MANB dictionary and
  return its nested MANP properties.
- `DERImg4DecodeParseManifestPropertiesInternal`: select a caller-supplied
  manifest body and its MANP dictionary.
- `DERImg4DecodeFindProperty`: find and type-check one private FourCC
  property.
- `Img4DecodeGetPayload`: return the decoded payload byte item.
- `Img4DecodeGetPayloadType`: decode the four-byte payload type.
- `Img4DecodePayloadCompressionInfoExists`: report optional compression
  metadata presence.
- `Img4DecodeGetPayloadCompressionInfo`: return compression algorithm and
  uncompressed size.
- `Img4DecodeManifestExists`: report manifest backing-item presence.
- `Img4DecodeSectionExists`: report manifest or restore section presence.
- `Img4DecodeGetRestoreInfoData`: return a restore OCTET STRING property.
- `Img4DecodeGetDataFromSection`: return an OCTET STRING property from a
  selected section.
- `Img4DecodePayloadPropertiesExists`: report PAYP dictionary presence.
- `Img4DecodeGetPayloadProperties`: return the PAYP dictionary item.
- `Img4DecodeGetPropertyFromSection`: decode a typed property from manifest
  or restore data.
- `Img4DecodeGetPropertyData`: decode a bounded OCTET STRING property.
- `Img4DecodeGetPropertyInteger64`: decode a canonical unsigned 64-bit
  integer property.
- `Img4DecodeGetPropertyBoolean`: decode a canonical Boolean property.
