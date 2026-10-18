# Runtime unprivileged-memory provenance

The implementation provides AVP's lower-EL copy contract. FreeBSD's AArch64
copyin/copyout implementation was reviewed for its public handling of
user-address validation, observable faults, and unprivileged loads.

| Project | Commit | License | Reviewed implementation |
| --- | --- | --- | --- |
| FreeBSD | `7d576f1d657fc21f97cedc28cc971621ee2490a7` | BSD-2-Clause | `sys/arm64/arm64/copyinout.S` |

Upstream URL:

- https://github.com/freebsd/freebsd-src/blob/7d576f1d657fc21f97cedc28cc971621ee2490a7/sys/arm64/arm64/copyinout.S

AVP uses Firebloom bounds and direct `LDTR`/`STTR` width-specialized accesses.
The local C layer validates the privileged span and the AArch64 leaf hooks
preserve lower-EL translation semantics. Access faults remain visible to the
runtime exception module. No FreeBSD source text is copied.
