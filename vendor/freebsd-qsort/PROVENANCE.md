# FreeBSD qsort provenance

- Upstream repository: https://github.com/freebsd/freebsd-src
- Pinned commit: `3aac283613bd3fd0228a06d6c854ca0bf190ecfb`
- Source URL: https://github.com/freebsd/freebsd-src/blob/3aac283613bd3fd0228a06d6c854ca0bf190ecfb/lib/libc/stdlib/qsort.c
- License: BSD-3-Clause, Copyright (c) 1992, 1993 The Regents of the
  University of California

`src/runtime/support/runtime_qsort.c` adapts FreeBSD's Bentley-McIlroy
three-way partitioning and carries the full FreeBSD notice with an
owngoal-dev modification note. It exposes the swap and median helpers as
separate entry points and keeps the generic freestanding comparator ABI.
