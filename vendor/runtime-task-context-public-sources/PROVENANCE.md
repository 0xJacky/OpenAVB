# Runtime task-context provenance

The implementation was reviewed against the following public source:

- Project: FreeBSD
- Repository: https://github.com/freebsd/freebsd-src
- Commit: `7d576f1d657fc21f97cedc28cc971621ee2490a7`
- Files: `sys/arm64/include/pcpu.h`, `sys/arm64/include/cpufunc.h`, and
  `sys/arm64/arm64/exception.S`
- License: BSD-2-Clause
- Current-thread source: https://github.com/freebsd/freebsd-src/blob/7d576f1d657fc21f97cedc28cc971621ee2490a7/sys/arm64/include/pcpu.h
- Interrupt-state source: https://github.com/freebsd/freebsd-src/blob/7d576f1d657fc21f97cedc28cc971621ee2490a7/sys/arm64/include/cpufunc.h

FreeBSD supplies the established AArch64 pattern of retrieving the current
thread from an architecture-owned register and testing saved interrupt state
around blocking operations. OpenAVB applies that pattern to the AVP interface's
0x70 current-task view, 0x5b0 selected-task tuple, exception-frame fields, and
task registry. No FreeBSD source text is copied.
