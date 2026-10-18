# libzbitmap provenance

- Upstream repository: https://github.com/eafer/libzbitmap
- Pinned commit: `574abeae25b25c3319b9b6a9225cc7464e08ae88`
- Source URL: https://github.com/eafer/libzbitmap/blob/574abeae25b25c3319b9b6a9225cc7464e08ae88/libzbitmap.c
- License: MIT

OpenAVB vendors the upstream decompression half, replaces the hosted
standard-library include with `stddef.h`, and routes the magic comparison
through the project-owned musl-compatible memcmp. The decoder state machine
and bounds checks remain the upstream implementation.
