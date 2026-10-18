# Apple libDER provenance

`include/avp/image/der/apple_libder.h`, `src/image/der/apple_libder_decode.c`
and `src/image/der/apple_libder_sequence.c` are adapted from Apple libDER and
remain under the Apple Public Source License 2.0. Each carries Apple's
copyright notice, the upstream APSL header and an owngoal-dev modification
notice.

- Upstream repository: https://github.com/apple-oss-distributions/Security
- Upstream tag: `Security-57740.60.18`
- Upstream commit: `db455f62ee15e6adf20afe7c2e132321d2d0759c`
- Decoder source: https://github.com/apple-oss-distributions/Security/blob/Security-57740.60.18/OSX/libsecurity_keychain/libDER/libDER/DER_Decode.c
- Decoder header: https://github.com/apple-oss-distributions/Security/blob/Security-57740.60.18/OSX/libsecurity_keychain/libDER/libDER/DER_Decode.h
- Core header and `DEROidCompare`: https://github.com/apple-oss-distributions/Security/blob/Security-57740.60.18/OSX/libsecurity_keychain/libDER/libDER/libDER.h
- License: Apple Public Source License 2.0 (`LICENSES/APSL-2.0.txt`)

OpenAVB keeps libDER's public symbol names, structures, error values,
definite-length decoding, scalar parsing, sequence-template flags and OID
comparison, and adds canonical-DER length and BIT STRING padding checks.
