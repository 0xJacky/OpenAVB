# Finite-field compatibility provenance

The fixed-width division, modular arithmetic, Montgomery representation,
binary inversion, exponentiation, and Tonelli-Shanks square-root algorithms
follow the corresponding public LibTomMath designs. Function names and
caller-owned context layouts follow the ROM interface; the entry points are
ABI- and behaviour-compatible with the corecrypto routines AVPBooter uses.

- Upstream: https://github.com/libtom/libtommath
- Commit: `ae40a87a920099a7d9d00979570e0c8d917a1fd7`
- Division:
  https://github.com/libtom/libtommath/blob/ae40a87a920099a7d9d00979570e0c8d917a1fd7/mp_div.c
- Exponentiation:
  https://github.com/libtom/libtommath/blob/ae40a87a920099a7d9d00979570e0c8d917a1fd7/s_mp_exptmod.c
- Odd-modulus inversion:
  https://github.com/libtom/libtommath/blob/ae40a87a920099a7d9d00979570e0c8d917a1fd7/s_mp_invmod_odd.c
- Montgomery setup/reduction:
  https://github.com/libtom/libtommath/blob/ae40a87a920099a7d9d00979570e0c8d917a1fd7/mp_montgomery_reduce.c
- Prime-field square root:
  https://github.com/libtom/libtommath/blob/ae40a87a920099a7d9d00979570e0c8d917a1fd7/mp_sqrtmod_prime.c
- License: Unlicense,
  https://github.com/libtom/libtommath/blob/ae40a87a920099a7d9d00979570e0c8d917a1fd7/LICENSE

## Exported functions

- `cczp_mm_init_ws`: initialize an odd-modulus Montgomery context, radix
  inverse, and R-squared value.
- `cczp_mm_to_ws`: convert a canonical residue into Montgomery
  representation.
- `cczp_mm_from_ws`: convert a Montgomery residue into canonical
  representation.
- `cczp_mm_mul_ws`: multiply two Montgomery residues modulo the context
  prime.
- `cczp_mm_sqr_ws`: square a Montgomery residue modulo the context prime.
- `cczp_mm_mod_ws`: reduce a double-width integer and return Montgomery
  representation.
- `cczp_mm_redc_ws`: reduce a double-width Montgomery value by one radix
  factor.
- `cczp_from_default_ws`: copy a canonical default-field residue from its
  representation.
- `cczp_to_default_ws`: copy a canonical residue into the default
  representation.
- `cczp_is_quadratic_residue_ws`: evaluate the Legendre symbol, treating
  zero as a quadratic residue.
- `cczp_inv_default_ws`: invert a nonzero residue with the binary
  extended-GCD algorithm.
- `cczp_inv_update_redc_ws`: apply a signed two-term modular update, shift
  the fixed-point matrix result by 31 bits, and reduce it for inversion.
- `cczp_sqrt_tonelli_shanks_precomp_ws`: compute a prime-field square root
  using the precomputed-entry ABI.
- `cczp_sqrt_default_ws`: compute a prime-field square root with
  Tonelli-Shanks.
- `cczp_power_fast_ws`: exponentiate in the context's selected residue
  representation.
- `ccn_gcd_approximate`: produce high-word GCD approximations while
  retaining 31 low bits.
- `ccn_gcd_update_ws`: apply a signed linear GCD update, shift the
  fixed-point matrix result by 31 bits, and return its sign.
- `cczp_cond_negate`: select a residue or its modular negation.
- `cczp_add_default_ws`, `cczp_sub_default_ws`, `cczp_mul_default_ws`,
  `cczp_sqr_default_ws`: add, subtract, multiply, or square canonical
  residues modulo the field prime.
- `cczp_init_ws`: validate and initialize a caller-owned default field
  context.
- `cczp_modn_ws`: reduce an arbitrary limb-count integer modulo the field
  prime.
- `cczp_mod_default_ws`: reduce a double-width integer into a canonical
  residue.
- `ccn_sqr_ws`: produce the exact double-width square of a limb vector.
- `ccn_divmod_ws`: produce bounded quotient and remainder for arbitrary limb
  vectors.
