# cczp accessor provenance

This module implements the `cczp_n` and `cczp_prime` entry points, two
fixed-layout loads from a `struct cczp` field context. They are ABI- and
behaviour-compatible with the corecrypto routines AVPBooter uses.

The implementation is original OpenAVB code and contains no third-party
source.
