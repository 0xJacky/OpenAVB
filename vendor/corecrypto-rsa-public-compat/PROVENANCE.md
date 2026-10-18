# RSA public-key compatibility provenance

The fixed-width public modular exponentiation follows LibTomMath's public
binary modular arithmetic design. EMSA-PKCS1-v1_5 construction and checking
follow RFC 8017 section 9.2. The exported functions are ABI- and
behaviour-compatible with the corecrypto RSA routines AVPBooter uses.

- Arithmetic upstream: https://github.com/libtom/libtommath
- Arithmetic commit: `ae40a87a920099a7d9d00979570e0c8d917a1fd7`
- Arithmetic reference:
  https://github.com/libtom/libtommath/blob/ae40a87a920099a7d9d00979570e0c8d917a1fd7/s_mp_exptmod.c
- Arithmetic license: Unlicense,
  https://github.com/libtom/libtommath/blob/ae40a87a920099a7d9d00979570e0c8d917a1fd7/LICENSE
- Encoding standard: https://www.rfc-editor.org/rfc/rfc8017#section-9.2

## Exported functions

- `ccrsa_verify_pkcs1v15_internal_ws`: compute the signature representative
  with the public key, enforce the selected signature-length policy, and
  validate EMSA-PKCS1-v1_5 with a success canary.
- `ccrsa_make_pub`: import and validate an odd RSA modulus and public
  exponent into a caller-sized public-key context.
- `ccrsa_emsa_pkcs1v15_verify_canary_out`: strictly validate the RFC 8017
  encoded message and publish the fault canary only after full success.
- `ccrsa_pub_crypt_ws`: compute the public operation `source^e mod modulus`
  for an in-range representative.
- `ccrsa_verify_pkcs1v15`: report signature validity through a boolean
  while preserving input and key errors.
- `ccrsa_verify_pkcs1v15_digest`: return the strict signature-verification
  status and success canary for a precomputed digest.

The fault canary values are values observed in the ROM interface.
