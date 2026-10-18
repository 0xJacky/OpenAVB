# USB stack closure owner provenance

The USB control and lifecycle transport uses TinyUSB device APIs from the
vendored TinyUSB revision documented in `../tinyusb/README.md`.
`tud_control_xfer()`, `tud_control_status()`, `tusb_rhport_init()`,
`tud_connect()`, `tud_disconnect()`, and `tusb_deinit()` own the public USB
device operations. OpenAVB's runtime RPC codec owns the private lifecycle
notification packet with command `0x13` and two one-byte fields.

Public upstream:

- https://github.com/hathach/tinyusb/tree/32530d8f4b6cd6d6f7a7df7e6358856c7ebc4f5e
- https://docs.tinyusb.org/en/latest/reference/getting_started.html
- https://docs.tinyusb.org/en/latest/reference/concurrency.html

TinyUSB is distributed under the MIT license. Its vendored license is at
`../tinyusb/LICENSE`.
