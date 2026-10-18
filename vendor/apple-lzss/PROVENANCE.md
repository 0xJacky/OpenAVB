# Apple XNU LZSS provenance

`src/image/compression/apple_lzss.c` is adapted from the LZSS decoder in
XNU `libkern/mkext.c` and remains under the Apple Public Source License 2.0.
It carries Apple's copyright notice, the APSL header, Haruhiko Okumura's
original LZSS.C notice and an owngoal-dev modification notice.

- Upstream repository: https://github.com/apple-oss-distributions/xnu
- Pinned commit: `f6217f891ac0bb64f3d375211650a4c1ff8ca1ea`
- Source URL: https://github.com/apple-oss-distributions/xnu/blob/f6217f891ac0bb64f3d375211650a4c1ff8ca1ea/libkern/mkext.c
- License: Apple Public Source License 2.0 (`LICENSES/APSL-2.0.txt`); the
  embedded LZSS.C notice by Haruhiko Okumura (1989) permits free use,
  distribution and modification.

The decoder keeps XNU's 4096-byte space-filled ring, 12-bit match offset
and match-length-nibble-plus-three semantics, with bounded `size_t` input
and output counts.
