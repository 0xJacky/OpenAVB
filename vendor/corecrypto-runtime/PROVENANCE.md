# Crypto runtime provenance

- Secure clear implementation: Mbed TLS commit
  `ce0384b99949b5c8e80548d3b311c39ed0d5eb7a`
- Source URL:
  https://github.com/Mbed-TLS/mbedtls/blob/ce0384b99949b5c8e80548d3b311c39ed0d5eb7a/library/platform_util.c
- License selection: Apache-2.0

The secure wipe binds directly to `mbedtls_platform_zeroize`. Heap
ownership, workspace geometry, abort routing, and the sixteen-byte fault
canary folding follow the ROM interface through explicit platform callbacks,
and are ABI- and behaviour-compatible with the corecrypto runtime routines
AVPBooter uses.
