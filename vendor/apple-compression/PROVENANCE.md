# Apple compression (LZFSE) provenance

The OpenAVB compression layer exposes the LZFSE/LZVN decoding entry points
the ROM interface expects, on top of Apple's open-source `lzfse`.

- Upstream repository: https://github.com/lzfse/lzfse
- Pinned commit: `e634ca58b4821d9f3d560cdc6df5dec02ffc93fd`
- License: BSD-3-Clause
- License URL: https://github.com/lzfse/lzfse/blob/e634ca58b4821d9f3d560cdc6df5dec02ffc93fd/LICENSE

`vendor/lzfse` holds the vendored decoder; four of its files are modified
(listed in `vendor/lzfse/PROVENANCE.md`), the rest match the pinned commit.
`src/image/compression/apple_compression_v1.c` adapts the V2-to-V1 header
conversion from upstream `src/lzfse_decode_base.c` and carries the lzfse
BSD-3-Clause notice. The adapters in `src/image/compression/` (original
OpenAVB code, MIT) add the profile dispatch and the bounded iBoot
output-buffer handling and delegate bitstream decoding to `vendor/lzfse`.

Exported entry points: `lzvnDecode`, `lzvn_decode_buffer`,
`lzfse_decode_lmd`, `lzfse_decode_literals`, `lzfse_decode_buffer`,
`lzfseDecodeV1`, `lzfse_decode_buffer_iboot`,
`lzfse_decode_lzvn_block_iboot`, `LZFSEIBootBufferPushN`,
`LZFSEIBootBufferPush`, `LZFSEIBootBufferCopy`, `lzfseDecode`,
`compression_decode_scratch_buffer_size` and `compression_decode_buffer`
(algorithms `0x891` and `0x8a1`).
