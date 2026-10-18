# Digest, HMAC, and comparison compatibility provenance

This module implements ten entry points that are ABI- and
behaviour-compatible with the corecrypto digest, HMAC, and constant-time
comparison routines AVPBooter uses (`ccdigest_*`, `cchmac_*`,
`cc_cmp_safe`). The cryptographic implementation is the SHA-384 and HMAC
API already vendored from Mbed TLS.

## Mbed TLS SHA-384 and HMAC

- Repository: https://github.com/Mbed-TLS/mbedtls
- Commit: `ce0384b99949b5c8e80548d3b311c39ed0d5eb7a`
- Commit URL:
  https://github.com/Mbed-TLS/mbedtls/commit/ce0384b99949b5c8e80548d3b311c39ed0d5eb7a
- TF-PSA-Crypto submodule repository:
  https://github.com/Mbed-TLS/TF-PSA-Crypto
- TF-PSA-Crypto commit:
  `a92cb6a97d84762d2f4355eae99af7b9ff5c6ae1`
- TF-PSA-Crypto commit URL:
  https://github.com/Mbed-TLS/TF-PSA-Crypto/commit/a92cb6a97d84762d2f4355eae99af7b9ff5c6ae1
- License selection: Apache-2.0
- Bound APIs: `mbedtls_sha512_starts` in SHA-384 mode,
  `mbedtls_sha512_update`, `mbedtls_sha512_finish`, `mbedtls_md`, and the
  `mbedtls_md_hmac_*` family.

The constant-flow comparison loop follows `mbedtls_ct_memcmp` from
`utilities/constant_time.c` at the fixed TF-PSA-Crypto commit. Its volatile
full-buffer reads and 32-bit difference reduction are retained. The
zero-length convention of the ROM interface is applied by the compatibility
entry.
