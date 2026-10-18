# Boot MMU bootstrap owner provenance

The 16 KiB mapping geometry and translation-register activation model follow
Trusted Firmware-A commit `4736daa241c9e9349d4931a44e95991e75a19dd1`,
BSD-3-Clause:

https://github.com/ARM-software/arm-trusted-firmware/tree/4736daa241c9e9349d4931a44e95991e75a19dd1/lib/xlat_tables_v2

The MAIR value, SCTLR value, temporary-work-RAM policy, and mapping attributes
follow the expected ROM interface.
