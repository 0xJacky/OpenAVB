# USB EP0 state adapter provenance

The control status and endpoint stall paths use the TinyUSB device APIs from
the vendored MIT subset pinned at commit
`32530d8f4b6cd6d6f7a7df7e6358856c7ebc4f5e`:

- `tud_control_status`
- `dcd_edpt_stall`

Upstream: <https://github.com/hathach/tinyusb>

Pinned source: <https://github.com/hathach/tinyusb/commit/32530d8f4b6cd6d6f7a7df7e6358856c7ebc4f5e>

The five shared-buffer slots, their `0x100, 0x800, 0, 0, 0` logical lengths,
the deferred TEST_MODE selector, and the reset/status transcript follow the
expected private interface and are implemented by original OpenAVB code.
