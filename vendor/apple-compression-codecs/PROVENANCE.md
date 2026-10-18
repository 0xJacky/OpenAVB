# Apple compression codec provenance

DEFLATE, zlib and LZ4 decoding for the ROM's compression interface.

## DEFLATE and zlib

- Upstream repository: https://github.com/madler/zlib
- Release: `v1.3.1`
- Pinned commit: `51b7f2abdade71cd9bb0e7a373ef2610ec6f9daf`
- Imported source: `contrib/puff/puff.c`, blob `d759825ab1d79fc67b453944931f87d14b6f591e`
  (vendored unmodified in `vendor/zlib-puff`)
- License: zlib License
- License URL: https://github.com/madler/zlib/blob/51b7f2abdade71cd9bb0e7a373ef2610ec6f9daf/LICENSE

Puff is zlib's stack-only RFC 1951 reference decoder intended for bootstrap
applications. OpenAVB supplies a freestanding AArch64 setjmp context and an
RFC 1950 framing adapter (`src/image/compression/apple_deflate.c`).

## LZ4

- Upstream repository: https://github.com/apple-oss-distributions/xnu
- Pinned commit: `f6217f891ac0bb64f3d375211650a4c1ff8ca1ea`
- Source URL: https://github.com/apple-oss-distributions/xnu/blob/f6217f891ac0bb64f3d375211650a4c1ff8ca1ea/osfmk/vm/lz4.c
- License: Apple Public Source License 2.0 (`LICENSES/APSL-2.0.txt`)

`src/image/compression/apple_lz4.c` is adapted from XNU `lz4.c` and carries
Apple's copyright notice, the APSL 2.0 header and an owngoal-dev
modification notice. Its scalar decoder also serves the `lz4_decode_asm`
entry. The frame adapter handles the `bv41`, `bv4-` and `bv4$` records used
by compression algorithm `0x100`.

Exported entry points: `lz4_decode_buffer`, `lz4_decode_asm`, `adler32`,
`zlib_decode_scratch_size`, `zlib_decode_buffer` (raw DEFLATE `0x205`,
zlib `0x505`), `zlibDecodeBuffer`, `zlibDecodeBufferSafe`,
`readHuffmanTable`, `lz4raw_decode_buffer` and `lz4_decode`.
