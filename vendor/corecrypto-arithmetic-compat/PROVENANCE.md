# Fixed-width arithmetic compatibility provenance

OpenAVB specializes the public LibTomMath magnitude arithmetic algorithms
to caller-owned, little-endian arrays of 64-bit limbs, the representation
used by the corecrypto routines AVPBooter calls. The adapter covers addition,
subtraction, multiplication, conditional selection, conditional negation,
and single-word shifts without dynamic allocation.

- Repository: https://github.com/libtom/libtommath
- Commit: `ae40a87a920099a7d9d00979570e0c8d917a1fd7`
- Commit URL:
  https://github.com/libtom/libtommath/commit/ae40a87a920099a7d9d00979570e0c8d917a1fd7
- License: The Unlicense
- Relevant files: `s_mp_add.c`, `s_mp_sub.c`, `s_mp_mul_digs.c`,
  `mp_mul_d.c`, `mp_lshd.c`, `mp_rshd.c`, `mp_div_2d.c`, and `mp_2expt.c`

The entry points are ABI- and behaviour-compatible with the corecrypto
fixed-width routines AVPBooter uses.
