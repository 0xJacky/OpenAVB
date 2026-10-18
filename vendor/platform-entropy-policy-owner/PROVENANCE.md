# Platform entropy policy owner provenance

This module implements the policy and bootstrap-seed portion of the platform
entropy owner. Policy bit 31 gates both outputs, the secondary output follows
the gate, and the primary output additionally requires production-mode bit 8
plus the initialized/bit-24 policy condition.

The implementation delegates entropy state transitions to the existing
OpenAVB `platform_entropy_seed_primary_data` and
`platform_entropy_enable_primary_request` APIs. Those operations ultimately
feed the repository's pinned Mbed TLS HMAC_DRBG implementation.

The adapter is original project code. This provenance record documents its
technical sources and makes no independent license grant. Public cryptographic
implementation provenance remains documented under
`vendor/mbedtls/PROVENANCE.md`.

## Public source

- Mbed TLS HMAC_DRBG API: https://mbed-tls.readthedocs.io/projects/api/en/development/api/file/hmac__drbg_8h/
