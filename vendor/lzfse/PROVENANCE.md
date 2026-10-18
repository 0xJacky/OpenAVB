# lzfse provenance

- Upstream repository: https://github.com/lzfse/lzfse
- Pinned commit: `e634ca58b4821d9f3d560cdc6df5dec02ffc93fd`
- License: BSD-3-Clause, Copyright (c) 2015-2016, Apple Inc. (`LICENSE`,
  identical to upstream)

The decode half of upstream `src/` is vendored. Files identical to the
pinned commit: `lzfse.h`, `lzfse_fse.c`, `lzfse_tunables.h`,
`lzvn_decode_base.c`, `lzvn_decode_base.h`.

Files modified by owngoal-dev, 2026 (each keeps the upstream licence
header; the changes are under the same BSD-3-Clause terms):

- `lzfse_decode.c`: `lzfse_decode_buffer` no longer falls back to
  `malloc`/`free` when `scratch_buffer` is NULL (the ROM has no heap for it);
  it returns 0 instead. The function is marked `used, retain`.
- `lzfse_decode_base.c`: the four-state literal loop inside `lzfse_decode`
  is moved into a new function `lzfse_decode_literals`; `lzfse_decode_lmd`
  is no longer `static`. Both are marked `used, retain` so they remain
  separately callable entry points.
- `lzfse_internal.h`: prototypes for `lzfse_decode_lmd` and
  `lzfse_decode_literals`.
- `lzfse_fse.h`: `fse_in_pull` loads its 32-bit word with `memcpy` instead
  of a `uint32_t *` dereference, so an unaligned source buffer is safe when
  the guest enforces strict alignment.

`src/image/compression/apple_compression_v1.c` is adapted from upstream
`lzfse_decode_base.c` and carries the lzfse BSD-3-Clause notice with its
own modification note.
