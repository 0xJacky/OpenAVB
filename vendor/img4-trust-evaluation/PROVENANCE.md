# IMG4 trust evaluation provenance

The trust evaluation code (`src/image/img4/img4_trust_chain.c` and the
trust-evaluation entry points) is original OpenAVB code (MIT). DER decoding
goes through the libDER-compatible decoder in `src/image/der/`, which is
derived from Apple libDER `DER_Decode.c`/`DER_Decode.h` under the Apple
Public Source License 2.0 (see `vendor/apple-libder`). Certificate parsing
goes through `img4_x509_compat.c` (RFC 5280).

The IMG4 container and manifest property schemas follow PongoOS (MIT; see
`vendor/img4-decode-compat`):

- Repository: https://github.com/checkra1n/PongoOS
- Commit: `4c9b7541629234147fcc778f0ce4162482aaccef`
- Paths: `src/lib/img4/img4.c` and `src/lib/img4/img4.h`
- License: MIT

## Embedded trust anchor

`src/image/img4/img4_trust_rsa4k_anchor.inc` embeds the Apple DDI Secure
Boot Root CA - G1 certificate. This is public certificate data: a
self-signed root CA certificate, distributed openly so that signatures
issued under it can be verified. OpenAVB uses it only as the anchor that
personalized IMG4 manifests are checked against. Apple Inc. holds whatever
rights exist in it; OpenAVB's MIT licence does not apply to it.

- Subject and issuer:
  `CN=Apple DDI Secure Boot Root CA - G1, O=Apple Inc., C=US`
- Size: 1394 bytes of DER
- DER SHA-256:
  `f4f2cbd3fa6516a2b70fb1d7e8c88e2a526d612deb14e3fd656475a4fcaece2b`
- Independently catalogued at https://github.com/hack-different/apple-knowledge
  (commit `76b168bcaaf43754bda7c7b6ec96caeade98e09c`, `_data/pki.yaml`, MIT)

The five public entry-point names, callback ordering, fixed two-certificate
policy, and embedded-anchor identity follow the ROM interface. Tests use
OpenSSL-generated 4096-bit root and leaf certificates as an independent
signature oracle.
