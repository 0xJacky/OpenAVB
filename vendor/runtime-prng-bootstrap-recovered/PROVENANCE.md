# Runtime PRNG, bootstrap, and boot dispatch provenance

The PRNG module binds to the existing pinned Mbed TLS SHA-384 HMAC_DRBG source
closure documented in `vendor/mbedtls/PROVENANCE.md`.

- Mbed TLS repository: https://github.com/Mbed-TLS/mbedtls
- Pinned commit: `ce0384b99949b5c8e80548d3b311c39ed0d5eb7a`
- Bound APIs: `mbedtls_hmac_drbg_seed`,
  `mbedtls_hmac_drbg_random_with_add`, `mbedtls_md_info_from_type`, and
  `mbedtls_platform_zeroize`
- Algorithm specification: NIST SP 800-90A Revision 1, HMAC_DRBG
  https://csrc.nist.gov/pubs/sp/800/90/a/r1/final

Bootstrap and boot-dispatch sequencing are original local adapters over
existing OpenAVB modules, written to the expected ROM interface.
