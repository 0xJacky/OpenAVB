# USB DFU identity adapter provenance

The adapter is original OpenAVB code implementing the DFU identity span and
constant helpers. The serial and nonce builders delegate to OpenAVB's existing
`usb_identity` implementation.

TinyUSB defines the public USB descriptor callback boundary used by the active
device path:

- Upstream: https://github.com/hathach/tinyusb
- Commit: `32530d8f4b6cd6d6f7a7df7e6358856c7ebc4f5e`
- Device descriptor API: `src/device/usbd.h`
- License: MIT

Apple VID `0x05ac`, the DFU PID formula `0x1226 + (security_domain & 3)`, and
the identity text are device compatibility data retained by this thin adapter.
