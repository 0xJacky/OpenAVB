# Platform panic owner provenance

This module implements the four platform panic entry points: the fixed
`double panic in ` and `iBoot Panic: %s: %s` records, progress words,
cache-clean ordering, separator writes, the pvpanic event, and the reset
reason.

The implementation delegates reset to the existing PSCI platform reset path,
panic publication to the existing pvpanic device, and console/cache/progress
operations to existing OpenAVB owners. The binding surface makes every
terminal side effect observable in host transcript tests.

The adapter is original project code and imports no third-party implementation.
This provenance record documents its technical sources and makes no independent
license grant.

## Public specifications

- QEMU pvpanic device: https://www.qemu.org/docs/master/specs/pvpanic.html
- Arm PSCI system reset: https://developer.arm.com/documentation/den0022/latest/
