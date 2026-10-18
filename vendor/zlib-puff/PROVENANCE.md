# zlib puff provenance

- Upstream repository: https://github.com/madler/zlib
- Release: `v1.3.1`
- Pinned commit: `51b7f2abdade71cd9bb0e7a373ef2610ec6f9daf`
- Files: `contrib/puff/puff.c` (blob
  `d759825ab1d79fc67b453944931f87d14b6f591e`) and `contrib/puff/puff.h`
- License: zlib License, Copyright (C) 2002-2013 Mark Adler (see the
  notices in `puff.c` / `puff.h`, and `LICENSE`)

`puff.c` and `puff.h` are byte-identical to the pinned commit. The
OpenAVB adapters that call puff live in `src/image/compression/`.
