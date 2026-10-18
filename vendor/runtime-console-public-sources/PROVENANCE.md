# Runtime console provenance

The implementation was reviewed against these public sources:

- Project: FreeRTOS-Kernel
- Repository: https://github.com/FreeRTOS/FreeRTOS-Kernel
- Commit: `ce221a8bb468e462ca6b435cef66a9636e00baf4`
- File: `stream_buffer.c`
- License: MIT
- Source URL: https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/ce221a8bb468e462ca6b435cef66a9636e00baf4/stream_buffer.c
- Project: musl libc mirror
- Repository: https://github.com/ifduyue/musl
- Commit: `f21a96538f78fa8e2040831b4209b35f2fb581da`
- File: `src/string/strnlen.c`
- License: MIT
- Source URL: https://github.com/ifduyue/musl/blob/f21a96538f78fa8e2040831b4209b35f2fb581da/src/string/strnlen.c

OpenAVB directly reuses its existing `runtime_byte_ring`, whose public
algorithm provenance includes FreeRTOS StreamBuffer. The console layer adds
the AVP event callbacks, full-ring yield protocol, CRLF expansion,
0x2000-byte routing chunks, bounded C-string scan, and fixed lock tuples. No
public source text is copied into this adapter.
