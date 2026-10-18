# musl string source provenance

The OpenAVB runtime string compatibility cluster derives its scalar
algorithms from musl libc's public string routines.

- Canonical upstream: https://git.musl-libc.org/cgit/musl
- GitHub mirror: https://github.com/ifduyue/musl
- Pinned commit: `f21a96538f78fa8e2040831b4209b35f2fb581da`
- Commit URL: https://github.com/ifduyue/musl/commit/f21a96538f78fa8e2040831b4209b35f2fb581da
- License: MIT and BSD-family notices collected in `COPYRIGHT`
- License URL: https://github.com/ifduyue/musl/blob/f21a96538f78fa8e2040831b4209b35f2fb581da/COPYRIGHT

The vendored `memcmp`, `memmove`, `memset`, `strchr`, `strcmp`, `strlcat`,
`strlcpy`, `strlen`, and `strncmp` files retain their exact upstream Git blob
identities. `runtime_string_compat.c` adapts these algorithms to the ROM's
bounded span ABI and terminal capacity-failure contract.

Exported entry points: `runtime_memcmp`, `runtime_bounded_strchr`,
`runtime_bounded_strcmp`, `runtime_bounded_strlcat`,
`runtime_bounded_strlcpy`, `runtime_bounded_strncmp`, `runtime_strlcpy`,
`runtime_strlen`, `runtime_strncmp`, `runtime_memmove_source_first`,
`runtime_bounded_memmove` and `runtime_bounded_memset`.
