# USB device dispatch adapter provenance

The adapter uses TinyUSB's device controller and endpoint state APIs from the
vendored TinyUSB snapshot.

- Project: TinyUSB
- Repository: https://github.com/hathach/tinyusb
- Commit: `32530d8f4b6cd6d6f7a7df7e6358856c7ebc4f5e`
- License: MIT
- APIs: `dcd_set_address`, `usbd_edpt_stall`, `usbd_edpt_clear_stall`

The 18-slot, 0x120-byte callback table, local-versus-domain-four routing,
callback type words, optional slots, and fatal source attribution are a
private ABI. The adapter implements that table layout and routes hardware
operations through TinyUSB.

## Behavior contract

- Install a validated 0x120-byte table, or clear the live table.
- Route slots one and three through the selected local or remote table.
- Invoke required slots four (set address), five, seven (transfer submit),
  eight (endpoint halt), nine through twelve (eleven carries the deferred
  test-mode selector); a missing required slot publishes a fatal error with
  its own source line.
- Invoke optional slots fourteen and seventeen (transfer completion) only when
  installed.
- Return source token `0x0a033545e4350115` and the bounded live table span.

Exported functions:

- `usb_device_dispatch_control_transfer_buffer_release`
- `usb_device_dispatch_shutdown`
- `usb_device_dispatch_table_install_or_clear`
- `usb_device_dispatch_slot{0,1,2,3,4,5,7,8,9,10,11,12}_missing_callback_fatal`
- `usb_device_dispatch_set_address`
- `usb_device_dispatch_slot5`
- `usb_device_dispatch_transfer_submit`
- `usb_device_dispatch_endpoint_set_halt`
- `usb_device_dispatch_slot9`
- `usb_device_dispatch_slot10`
- `usb_device_dispatch_set_test_mode`
- `usb_device_dispatch_slot12`
- `usb_device_dispatch_slot14_optional`
- `usb_device_dispatch_transfer_completion_optional`
- `usb_device_dispatch_source_token`
- `usb_device_dispatch_table_span`
