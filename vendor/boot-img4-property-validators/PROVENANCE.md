# Boot IMG4 property validator provenance

The implementation delegates DER property decoding to the Apple libDER and
PongoOS-derived IMG4 compatibility owners already vendored by OpenAVB.

- Apple Security libDER: `db455f62ee15e6adf20afe7c2e132321d2d0759c`
  (APSL-2.0)
- PongoOS IMG4: `4c9b7541629234147fcc778f0ce4162482aaccef`
  (MIT)

The equality and minimum-value policies follow the expected call interface.
