# USB transfer request adapter provenance

The adapter uses TinyUSB's device endpoint transfer interface and USB descriptor
definitions from the vendored TinyUSB snapshot.

- Project: TinyUSB
- Repository: https://github.com/hathach/tinyusb
- Commit: `32530d8f4b6cd6d6f7a7df7e6358856c7ebc4f5e`
- License: MIT
- APIs: `usbd_edpt_xfer`, `tu_unaligned_read16`,
  `tusb_desc_configuration_t`, `tusb_control_request_t`

Request field offsets, tuple layouts, EP0 state offsets, and the callback
table layout are a project-specific wire ABI and remain thin adapter code
around those public TinyUSB APIs.

## Behavior contract

- Allocate and initialize one aligned 0x70-byte request, then submit its
  bounded payload with `usbd_edpt_xfer`.
- On completion, validate the request, invoke its optional callback, publish
  eligible EP0 IN completion, and release the pool record.
- Clamp configuration descriptor requests to little-endian `wTotalLength`
  and classify the 0x101 boundary.

Exported functions:

- `usb_transfer_request_complete_and_release`
- `usb_transfer_request_create_and_submit`
- `usb_ep0_validate_descriptor_span_thunk`
- `usb_transfer_request_validate_tuple`
- `usb_ep0_outlined_clear_transfer_span`
- `usb_ep0_outlined_load_payload_bounds`
- `usb_ep0_outlined_load_payload_span`
- `usb_ep0_outlined_setup_request_span_x25`
- `usb_ep0_outlined_load_endpoint_callback_span_x9`
- `usb_ep0_outlined_select_endpoint_callback_record`
- `usb_transfer_outlined_initialize_request_tail`
- `usb_transfer_outlined_restore_request_span_x22_x19`
- `usb_ep0_outlined_clamp_configuration_descriptor_length`
- `usb_transfer_request_allocate_one`
