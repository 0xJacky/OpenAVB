# Runtime physical-memory provenance

The implementation reuses OpenAVB's existing physical-region registry,
Firebloom spans, and region liveness predicates. The public design reference is
Trusted Firmware-A's translation-table implementation at commit
`4736daa241c9e9349d4931a44e95991e75a19dd1` (BSD-3-Clause):

- https://github.com/ARM-software/arm-trusted-firmware/blob/4736daa241c9e9349d4931a44e95991e75a19dd1/lib/xlat_tables_v2/xlat_tables_core.c
- https://github.com/ARM-software/arm-trusted-firmware/blob/4736daa241c9e9349d4931a44e95991e75a19dd1/docs/license.rst
- https://github.com/ARM-software/arm-trusted-firmware/commit/4736daa241c9e9349d4931a44e95991e75a19dd1

The public contracts used here are explicit region descriptors, checked
physical ranges, dynamic map/unmap ownership, and observable mapping failures.
OpenAVB implements the AVP interface's 16 KiB alignment, 0x40-byte input
records, 128-record limit, 16-bit region identifiers, policy-class table, and
bounded views. No Trusted Firmware-A source text is copied into this
implementation.
