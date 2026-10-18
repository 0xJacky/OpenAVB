# USB device controller owner provenance

The controller owner delegates endpoint lifecycle and halt handling to TinyUSB's
public device APIs at MIT commit
`32530d8f4b6cd6d6f7a7df7e6358856c7ebc4f5e`:

- `usbd_edpt_open`
- `usbd_edpt_stall`
- `usbd_edpt_clear_stall`
- `usbd_edpt_close`
- `dcd_edpt_close_all`
- `tud_disconnect`
- `tusb_deinit`
- `tu_edpt_number` and `tu_edpt_dir`

The adapter implements a two-function lifecycle owner, twelve endpoint
records, a 0x1000-byte EP0 resource, callback finalization, worker
notification, and observable initialization failures. TinyUSB owns the USB
endpoint state machine while the adapter owns lifecycle and callback metadata.

Upstream: https://github.com/hathach/tinyusb/tree/32530d8f4b6cd6d6f7a7df7e6358856c7ebc4f5e
License: https://github.com/hathach/tinyusb/blob/32530d8f4b6cd6d6f7a7df7e6358856c7ebc4f5e/LICENSE
