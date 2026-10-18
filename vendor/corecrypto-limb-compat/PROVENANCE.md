# Fixed-width limb compatibility provenance

This module implements fifteen fixed-width unsigned integer operations that
are ABI- and behaviour-compatible with the corecrypto `ccn` routines
AVPBooter uses. The implementation follows the public LibTomMath magnitude,
import/export, bit-count, copy, exchange, and negation algorithms,
specialized to OpenAVB's fixed little-endian array of 64-bit limbs.

## Public implementation source

- Repository: https://github.com/libtom/libtommath
- Commit: `ae40a87a920099a7d9d00979570e0c8d917a1fd7`
- Commit URL:
  https://github.com/libtom/libtommath/commit/ae40a87a920099a7d9d00979570e0c8d917a1fd7
- Tree: `bb3a9c3ad2ee806dbff50e693af3bf591ff89bf0`
- License: The Unlicense
- Relevant upstream files: `mp_cmp_mag.c`, `mp_cnt_lsb.c`,
  `mp_count_bits.c`, `mp_from_ubin.c`, `mp_to_ubin.c`, `mp_neg.c`,
  `mp_exch.c`, `mp_copy.c`, and `mp_zero.c`.

The source adapter removes LibTomMath's dynamic `mp_int` allocation and sign
field because callers provide the exact limb capacity. Arithmetic values and
byte-import results are checked by an independent OpenSSL BIGNUM test
oracle.
