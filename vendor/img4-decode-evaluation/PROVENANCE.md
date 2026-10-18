# IMG4 decode evaluation provenance

The decode evaluation code is original OpenAVB code (MIT). DER traversal,
strict length handling and sequence cursors go through the
libDER-compatible decoder in `src/image/der/`, which is derived from Apple
libDER `DER_Decode.c`/`DER_Decode.h` under the Apple Public Source
License 2.0 (see `vendor/apple-libder`).

The IMG4 property schema, private FourCC tags, manifest dictionary layout,
and SHA-384 policy follow PongoOS:

- Repository: https://github.com/checkra1n/PongoOS
- Commit: `4c9b7541629234147fcc778f0ce4162482aaccef`
- Paths: `src/lib/img4/img4.c` and `src/lib/img4/img4.h`
- License: MIT

SHA-384 computation is bound to the vendored Mbed TLS implementation:

- Repository: https://github.com/Mbed-TLS/mbedtls
- Commit: `ce0384b99949b5c8e80548d3b311c39ed0d5eb7a`
- Paths: `library/md.c` and `library/sha512.c`
- License selection: Apache-2.0

The 15 entry-point names, cache locations, callback ABI, and certificate
requirement markers follow the ROM interface. The tests use the published
SHA-384 `abc` vector as an independent digest oracle and exercise every
strict DER prefix truncation.
