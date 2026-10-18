# Mbed TLS crypto subset provenance

OpenAVB vendors the SHA-384 HMAC_DRBG dependency graph selected by the
Mbed TLS repository commit
`ce0384b99949b5c8e80548d3b311c39ed0d5eb7a`.

- Upstream repository: https://github.com/Mbed-TLS/mbedtls
- Upstream commit: `ce0384b99949b5c8e80548d3b311c39ed0d5eb7a`
- Upstream tree: `4d78086faf19037760375910a9097f3effe455c7`
- Retrieval: `gh api repos/Mbed-TLS/mbedtls/tarball/<commit>`
- Locked `tf-psa-crypto` submodule commit:
  `a92cb6a97d84762d2f4355eae99af7b9ff5c6ae1`
- Submodule repository: https://github.com/Mbed-TLS/TF-PSA-Crypto
- Submodule retrieval:
  `gh api repos/Mbed-TLS/TF-PSA-Crypto/tarball/<commit>`
- License selection: Apache-2.0

The files under `library/`, `core/`, and the upstream headers under `include/`
are copied without source edits. The compact `tf-psa-crypto/build_info.h` is an
OpenAVB port configuration that enables SHA-384, the legacy MD adapter,
HMAC_DRBG, platform memory hooks, and the upstream fixed-buffer allocator.

