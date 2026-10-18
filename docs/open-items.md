# Open items

Known, deliberately deferred work. Anything that changes the ROM image needs
a boot run on every supported iOS version before it is merged.

## Crypto layer

Most of the crypto already runs on the vendored Mbed TLS / TF-PSA-Crypto
(`vendor/mbedtls`):

| Primitive | Where | Notes |
|---|---|---|
| SHA-384/512, HMAC | `vendor/mbedtls` via `src/crypto/support/corecrypto_mbedtls.c`, `src/crypto/hash/corecrypto_sha512_compat.c` | thin wrappers that keep the interface names AVPBooter's callers use |
| HMAC-DRBG-SHA384 (live PRNG) | `src/runtime/prng/` | `mbedtls_hmac_drbg_*` |
| SHA-256 | `src/crypto/hash/sha256.c` | project-written, also used by SourceIBSS |
| Keccak-f1600 | `src/crypto/hash/corecrypto_sha3_compat.c` | written from FIPS 202 |
| bignum, RSA-4096 PKCS#1 v1.5 verify | `src/crypto/bignum/`, `src/crypto/rsa/` | project-written; used only by the IMG4 trust path, which nothing on the boot path calls |
| `ccdrbg_nisthmac_*` | `src/crypto/drbg/corecrypto_hmac_drbg.c` | no callers; kept alive only by `used, retain` |

Options:

1. Delete the unused `ccdrbg_nisthmac_*` (−2.1 KB).
2. Call Mbed TLS directly instead of through the wrappers, or rename the
   wrappers to neutral names.
3. RSA: port to `mbedtls_rsa_pkcs1_verify` (about +1–2 KB net; the Mbed TLS
   heap must grow from 2 KB to about 6–8 KB for 4096-bit keys, which fits the
   16 KB window at `0x70010000`), or keep the current code under neutral
   names, or drop it if the IMG4 RSA path stays unused.

Size budget: `linker.ld` requires `__rom_end <= 0x180000` (512 KiB; about
460 KB used) and `.text`/`.rodata` must end below the fixed `.handoff` at
`0x170000` (about 84 KB free).

Nothing that leaves the ROM depends on these internals: iBoot receives a
standard SHA-384 manifest digest and DRBG-generated nonces, and the
`.handoff` code does no crypto. The DRBG test vector in
`tests/runtime/prng/runtime_prng_tests.c` is reproduced by any SP 800-90A
HMAC-DRBG with the same parameters.

What a rename or swap does break: `make verify` has 108 symbol-presence
checks (`rg -q "cc..."` in the `Makefile`) that would need updating at the
same time.

## APSL-2.0 files

`apple_lz4.c`, the three libDER files and `apple_lzss.c` are APSL-2.0 (see
`LICENSE` and `LICENSES/APSL-2.0.txt`). APSL §2.2(c) requires publishing the
source of modifications that are deployed externally: if a ROM image built
from these files is given to anyone, their modified source must be public
too. Publishing this repository satisfies that.
