# Platform identity status and nonce owner provenance

This owner implements the platform identity status page, boot nonce, hardware
identity, variant string, and production-mode composition interfaces over the
platform MMIO transport.

## Public implementation sources

- Mbed TLS repository: https://github.com/Mbed-TLS/mbedtls
- Mbed TLS commit: `ce0384b99949b5c8e80548d3b311c39ed0d5eb7a`
- Mbed TLS commit URL:
  https://github.com/Mbed-TLS/mbedtls/commit/ce0384b99949b5c8e80548d3b311c39ed0d5eb7a
- License selection: Apache-2.0
- SHA-384 specification: https://doi.org/10.6028/NIST.FIPS.180-4

SHA-384 uses the existing `corecrypto_sha512_compat` owner and the vendored
Mbed TLS SHA-512-family implementation. Boot nonce generation uses the existing
OpenAVB HMAC_DRBG runtime owner, which is backed by the same fixed Mbed TLS
dependency graph. The new code is a thin original adapter around those public
implementations and the platform MMIO transport.

The host transcript test binds an in-memory MMIO and nonce transport. It checks
status-bit read-modify-write behavior, nonce caching and explicit clearing,
twelve-word hardware identity publication, the 64-byte variant window,
optional production-mode composition, and SHA-384 known-answer results.

## Behavior contract

- Compose IBFL from the effective production state and config bit eight, and
  the three CPFM bits from effective mode, config, and fuse fields.
- Lazily generate, cache, mark valid, and return a 64-bit boot nonce; clear
  both cached words when clear-request bit zero is set.
- Copy, install, or clear twelve hardware identity words, synchronized with
  status bit four.
- Snapshot the 64-byte variant window, append NUL, and return an empty span
  for an empty name.
- SHA-384 bounded inputs through the Mbed TLS-backed owner.

Exported functions:

- `platform_compute_iboot_flags`
- `platform_sha384_digest_u64_to_span`
- `platform_generate_boot_nonce`
- `platform_status_flag5_set`
- `platform_boot_nonce_clear_if_requested`
- `platform_get_boot_nonce`
- `platform_identity_copy_boot_hardware_words`
- `platform_identity_set_boot_hardware_words`
- `platform_status_flag3_is_set`
- `platform_status_flag3_update`
- `platform_copy_variant_name`
- `platform_identity_validate_u32_span_thunk`
- `j_platform_identity_validate_u32_span`
- `platform_identity_validate_u32_span`
- `platform_identity_outlined_load_status_and_base_x19_w8`
- `platform_sha384_digest_bounded`
- `platform_sha384_digest_bounded_thunk_a`
- `platform_sha384_digest_bounded_thunk_b`
- `platform_compute_chip_production_fuse_mode`
- `platform_read_effective_production_mode`
