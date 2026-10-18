# USB descriptor adapter provenance

The adapter is original OpenAVB code implementing the descriptor tuple and
copy helpers used by the USB descriptor path.

USB configuration descriptor layout and constants come directly from the
vendored TinyUSB public headers:

- Upstream: https://github.com/hathach/tinyusb
- Commit: `32530d8f4b6cd6d6f7a7df7e6358856c7ebc4f5e`
- Header: `src/common/tusb_types.h`
- License: MIT

The adapter uses TinyUSB's packed `tusb_desc_configuration_t` as the source of
truth for the nine-byte configuration header. Bounded-tuple type tags remain
local ABI details of this adapter.
