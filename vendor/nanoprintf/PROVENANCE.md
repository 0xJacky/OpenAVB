# nanoprintf Provenance

OpenAVB vendors nanoprintf v0.8.0 unchanged and configures its integer,
pointer, string, field-width, precision, alternate-form, and `%n` facilities.
Floating-point formatting stays outside the ROM configuration: the ROM
emits diagnostic placeholders for those conversions and builds with
general-purpose registers only.

- Upstream: https://github.com/charlesnicholson/nanoprintf
- Release: `v0.8.0`
- Commit: `115c916031ef51b73c5c373852fa550eaf134d49`
- Tree: `133dc919cdaa8780ebb36d36267e09a9c0260cf5`
- Source: `nanoprintf.h`
- Source blob: `d8d7ecd47d174d6b7a07061694b8317335017e8e`
- License: 0BSD OR Unlicense
- License blob: `572b70bd3f5baec7ab924e9b28af812b6b4699c8`
