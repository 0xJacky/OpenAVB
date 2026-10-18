# SHA-512-family compatibility provenance

This module binds four SHA-384/SHA-512 compression and finalization entries
of the ROM interface to the vendored Mbed TLS SHA-512-family
implementation. Software and Armv8-named compression entries share the same
protocol result and allow Mbed TLS to select its compiled compression
backend.

## Public implementation source

- Mbed TLS repository: https://github.com/Mbed-TLS/mbedtls
- Mbed TLS commit: `ce0384b99949b5c8e80548d3b311c39ed0d5eb7a`
- Mbed TLS tree: `4d78086faf19037760375910a9097f3effe455c7`
- Mbed TLS commit URL:
  https://github.com/Mbed-TLS/mbedtls/commit/ce0384b99949b5c8e80548d3b311c39ed0d5eb7a
- TF-PSA-Crypto repository: https://github.com/Mbed-TLS/TF-PSA-Crypto
- TF-PSA-Crypto commit: `a92cb6a97d84762d2f4355eae99af7b9ff5c6ae1`
- Source blob: `36f99bfc47eb185a599cb09163fbd1cffee4cc42`
- Source URL:
  https://github.com/Mbed-TLS/TF-PSA-Crypto/blob/a92cb6a97d84762d2f4355eae99af7b9ff5c6ae1/drivers/builtin/src/sha512.c
- License selection: Apache-2.0

The vendored `vendor/mbedtls/library/sha512.c` has the same Git blob
identity `36f99bfc47eb185a599cb09163fbd1cffee4cc42` as the fixed upstream
source.

The implementation binds to `mbedtls_sha512_update`,
`mbedtls_sha512_clone`, and `mbedtls_sha512_finish`. SHA-384 and SHA-512
initial states come from FIPS 180-4. The test oracle uses OpenSSL 3
SHA-384, SHA-512, and raw `SHA512_Transform` results.

## Exported functions

- `ccsha384_vng_arm_hw_compress`: compress whole 128-byte blocks into an
  eight-word SHA-384 state.
- `ccsha512_ltc_compress`: apply the SHA-512-family software compression
  transform with feed-forward.
- `ccsha512_final`: clone, pad, append the 128-bit bit count, serialize 48
  or 64 digest bytes, and clear the clone.
- `AccelerateCrypto_SHA512_compress_hwassist`: provide the Armv8-named
  compression entry with identical state semantics.
