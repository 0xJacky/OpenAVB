# Runtime memory provenance

No source code is copied into this directory. The listed implementations were
reviewed to validate behavior before writing the bounded AVP adapters.

| Project | Commit | License | Reviewed implementation |
| --- | --- | --- | --- |
| musl | `f21a96538f78fa8e2040831b4209b35f2fb581da` | MIT | `src/internal/intscan.c`, `src/string/strdup.c`, `src/string/memset.c` |
| FreeBSD | `3aac283613bd3fd0228a06d6c854ca0bf190ecfb` | BSD-3-Clause | `lib/libc/stdlib/strtoull.c`, `lib/libc/string/strdup.c`, `lib/libc/string/memset_s.c` |

Upstream URLs:

- https://github.com/ifduyue/musl/commit/f21a96538f78fa8e2040831b4209b35f2fb581da
- https://github.com/freebsd/freebsd-src/commit/3aac283613bd3fd0228a06d6c854ca0bf190ecfb

The AVP interface adds Firebloom bounds, a four-word end-position tuple,
source tokens, and fatal constraint handling. Those extensions are implemented
locally.
