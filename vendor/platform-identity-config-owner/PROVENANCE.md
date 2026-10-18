# Platform identity configuration owner provenance

This owner implements the vphone600 platform configuration-window interface.
The register layout is platform-private and is shared with OpenAVB's existing
platform MMIO owner.

The implementation uses the freestanding volatile MMIO access primitive in
`include/avp/platform/mmio.h` and exposes a transport binding for deterministic
host transcript tests. It is original OpenAVB code and has no third-party code
dependency.
