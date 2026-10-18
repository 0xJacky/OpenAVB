# USB DFU stack lifecycle adapter provenance

The transport lifecycle uses the TinyUSB device APIs from the vendored MIT
subset pinned at commit `32530d8f4b6cd6d6f7a7df7e6358856c7ebc4f5e`:

- `tusb_rhport_init`
- `tusb_deinit`
- `tud_connect`
- `tud_disconnect`

Upstream: <https://github.com/hathach/tinyusb>

Pinned source: <https://github.com/hathach/tinyusb/commit/32530d8f4b6cd6d6f7a7df7e6358856c7ebc4f5e>

The reference-counted policy word, command registry keys, one-time dispatch
configuration, and lifecycle transcript follow the expected private interface.
They remain a thin original adapter around the public TinyUSB device
lifecycle.
