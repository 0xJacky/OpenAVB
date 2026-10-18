# NIST HMAC_DRBG compatibility provenance

This module binds the DRBG entry points of the ROM interface to Mbed TLS
HMAC-SHA-384 primitives and implements the descriptor dispatch and
multi-segment state transitions specified by NIST SP 800-90A Rev. 1. The
entry points, descriptors, and status codes are ABI- and
behaviour-compatible with the corecrypto routines AVPBooter uses.

## Cryptographic implementation

- Mbed TLS repository: https://github.com/Mbed-TLS/mbedtls
- Mbed TLS commit: `ce0384b99949b5c8e80548d3b311c39ed0d5eb7a`
- Mbed TLS commit URL:
  https://github.com/Mbed-TLS/mbedtls/commit/ce0384b99949b5c8e80548d3b311c39ed0d5eb7a
- TF-PSA-Crypto repository: https://github.com/Mbed-TLS/TF-PSA-Crypto
- TF-PSA-Crypto commit:
  `a92cb6a97d84762d2f4355eae99af7b9ff5c6ae1`
- TF-PSA-Crypto commit URL:
  https://github.com/Mbed-TLS/TF-PSA-Crypto/commit/a92cb6a97d84762d2f4355eae99af7b9ff5c6ae1
- License selection: Apache-2.0
- Bound implementation: `mbedtls_md_hmac_*`, exposed through the local
  Mbed TLS compatibility layer.

The independent test oracle uses `mbedtls_hmac_drbg_seed_buf`,
`mbedtls_hmac_drbg_update`, and `mbedtls_hmac_drbg_random_with_add` from the
same fixed upstream commits.

## Algorithm specification

- NIST SP 800-90A Rev. 1, sections 9 and 10.1.2:
  https://doi.org/10.6028/NIST.SP.800-90Ar1
