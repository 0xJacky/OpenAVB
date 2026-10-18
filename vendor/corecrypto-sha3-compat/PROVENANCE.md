# SHA-3 permutation compatibility provenance

This module implements the Keccak-f[1600] entry
`AccelerateCrypto_SHA3_keccak`, which applies all 24 rounds in place to 25
little-endian 64-bit lanes, using the public tiny_sha3 theta, rho, pi, chi,
and iota schedule.

## Public implementation source

- Repository: https://github.com/mjosaarinen/tiny_sha3
- Commit: `dcbb3192047c2a721f5f851db591871d428036a9`
- Commit URL:
  https://github.com/mjosaarinen/tiny_sha3/commit/dcbb3192047c2a721f5f851db591871d428036a9
- Tree: `4e33d1d470726b208d298cc4893a018bc90c4a3c`
- `sha3.c` blob: `931ae020844f4c12c7e7f357849e85f6545fbaed`
- Source URL:
  https://github.com/mjosaarinen/tiny_sha3/blob/dcbb3192047c2a721f5f851db591871d428036a9/sha3.c
- License: MIT
- License blob: `d2d484d8820dcf5484212735dfc0e0f165db353e`
- License URL:
  https://github.com/mjosaarinen/tiny_sha3/blob/dcbb3192047c2a721f5f851db591871d428036a9/LICENSE

The OpenAVB source preserves tiny_sha3's 25-lane ordering, rotation
schedule, permutation indices, and FIPS 202 round constants. The host oracle
checks the zero-state Keccak-f[1600] permutation vector and builds SHA3-224,
SHA3-256, SHA3-384, and SHA3-512 sponges compared with OpenSSL 3 EVP.
