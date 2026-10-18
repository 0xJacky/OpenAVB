# USB transfer queue owner provenance

The transfer transport is implemented with TinyUSB device-stack APIs from the
vendored TinyUSB revision documented in `../tinyusb/README.md`.
`usbd_edpt_xfer()` owns controller submission and completion delivery. The
owner in `src/devices/usb/usb_transfer_queue_owner.c` implements the private
0x70-byte request records, twelve endpoint queues, FIFO completion order, EP0
setup/control phases, cancellation status, and optional completion dispatch.

Public upstream:

- https://github.com/hathach/tinyusb/tree/32530d8f4b6cd6d6f7a7df7e6358856c7ebc4f5e
- https://docs.tinyusb.org/en/latest/reference/getting_started.html
- https://docs.tinyusb.org/en/latest/reference/concurrency.html

TinyUSB is distributed under the MIT license. Its vendored license is at
`../tinyusb/LICENSE`.
