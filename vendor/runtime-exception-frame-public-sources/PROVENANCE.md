# Runtime exception-frame provenance

The implementation was reviewed against the following public source:

- Project: FreeBSD
- Repository: https://github.com/freebsd/freebsd-src
- Commit: `7d576f1d657fc21f97cedc28cc971621ee2490a7`
- File: `sys/arm64/arm64/exception.S`
- License: BSD-2-Clause
- Source URL: https://github.com/freebsd/freebsd-src/blob/7d576f1d657fc21f97cedc28cc971621ee2490a7/sys/arm64/arm64/exception.S

The public implementation supplies the established AArch64 pattern of saving
system state into a fixed exception frame before dispatch and restoring DAIF
state from the frame. OpenAVB uses that pattern with the AVP interface's
`RuntimeTaskFrame` offsets, capability bounds, handler-stack interval, and
dispatch ownership. No FreeBSD source text is copied into the implementation.
