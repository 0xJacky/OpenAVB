# Runtime page table provenance

The module delegates translation geometry, descriptor encoding, fixed-pool
allocation, walks, and break-before-make updates to the existing OpenAVB
runtime page-table engine.

The EL1 register, barrier, and TLBI sequences were checked against Arm Trusted
Firmware-A `arch_helpers.h` and its translation-table v2 core at commit
`4736daa241c9e9349d4931a44e95991e75a19dd1` (BSD-3-Clause). The reviewed blobs
are `23c3e3504c6542613ea14eb4907fb2e106b5cb81` and
`ae7ad891f23f4cb12473a6432657b8542d6eaa68`.

The range and mapping-record API shape was also checked against Zephyr's Arm64
MMU implementation at commit `0d65ce46dda0626164503c50f37e6e1f59d310a9`
(Apache-2.0), blob `2c50b8f883d6125d329a21a916c5e353310288ef`.

No source text was copied. The implementation follows the AVP interface's
16 KiB geometry, 42-bit virtual-address profile, fixed MAIR/TCR values, mapping
flag grammar, source-line fatal markers, and tuple-oriented wrapper contracts.
