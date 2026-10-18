# Boot-argument compatibility provenance

This module implements seven bounded boot-argument operations. The token
grammar and exact-key lookup follow Apple's public XNU boot-argument parser,
with the LF/CR separators, Firebloom bounds, platform-property hook, and
strict unsigned-integer policy of the ROM interface layered on top.

## Public implementation source

- Repository: https://github.com/apple-oss-distributions/xnu
- Commit: `f6217f891ac0bb64f3d375211650a4c1ff8ca1ea`
- Commit URL:
  https://github.com/apple-oss-distributions/xnu/commit/f6217f891ac0bb64f3d375211650a4c1ff8ca1ea
- Tree: `7cb04f5def354255785e3cb0e06d496dc918ebea`
- `pexpert/gen/bootargs.c` blob: `1ef8caa7ecbcfae4c8783110d44ad3c0d7744013`
- License: Apple Public Source License 2.0
- License URL: https://opensource.apple.com/apsl/
- Relevant upstream file: `pexpert/gen/bootargs.c`
- Source URL:
  https://github.com/apple-oss-distributions/xnu/blob/f6217f891ac0bb64f3d375211650a4c1ff8ca1ea/pexpert/gen/bootargs.c

The implementation adapts XNU's whitespace tokenization, first-match lookup,
exact key-length comparison, and base-zero numeric policy to bounded spans.
OpenAVB's existing `runtime_bounded_strtoull` provides the numeric engine;
that implementation is independently pinned to musl and FreeBSD in
`vendor/runtime-memory-public-sources/PROVENANCE.md`.

## Exported functions

- `boot_arguments_parse_next_token`: skip TAB/LF/CR/space, publish one
  bounded token and its key/token lengths.
- `boot_arguments_lookup_value`: return the first exact key and its optional
  value subspan.
- `boot_arguments_copy_value`: load `boot-args` when the input pointer is
  empty and allocate a NUL-terminated nonempty value.
- `boot_arguments_get_uint64`: parse a complete base-zero value and preserve
  the caller default for absent, empty, or partial input.
- `boot_arguments_validate_byte_span`: canonicalize an allocated result to
  the runtime byte type.
- `boot_arguments_load_platform_property`: request the bounded `boot-args`
  platform property.
- `boot_arguments_source_token`: return the diagnostic token value of the
  ROM interface.

The host oracle uses XNU's published XNUPOST vectors, libc `strtoull`, and
an independent token scanner. No upstream source text is copied into this
directory.
