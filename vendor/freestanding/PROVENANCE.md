# Freestanding header provenance

These four headers (`assert.h`, `inttypes.h`, `stdlib.h`, `string.h`) are
written for OpenAVB. They declare the small part of the hosted C library that
the vendored decoders include, so those sources build unchanged against the
freestanding ROM runtime. No third-party text is copied.

- License: MIT, as the rest of OpenAVB (see `../../LICENSE`).
