# Runtime exception dispatch provenance

The AArch64 vector-slot layout, register capture boundary, and fatal exception
routing were checked against these public implementations:

- FreeBSD arm64 exception entry, commit
  `7d576f1d657fc21f97cedc28cc971621ee2490a7`, BSD-2-Clause:
  https://github.com/freebsd/freebsd-src/blob/7d576f1d657fc21f97cedc28cc971621ee2490a7/sys/arm64/arm64/exception.S
- Trusted Firmware-A EL3 exception vectors, commit
  `4736daa241c9e9349d4931a44e95991e75a19dd1`, BSD-3-Clause:
  https://github.com/ARM-software/arm-trusted-firmware/blob/4736daa241c9e9349d4931a44e95991e75a19dd1/bl31/aarch64/runtime_exceptions.S

The frame layout, source token, source-line diagnostics, and breakpoint policy
are original OpenAVB code written to the expected ROM interface. No public
source text was copied.
