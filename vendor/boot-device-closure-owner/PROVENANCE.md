# Boot device closure owner provenance

This closure composes existing OpenAVB owners for the public USB device
stack and the private SEP mailbox protocol. USB DFU lifecycle state
continues to be driven by the vendored TinyUSB implementation pinned in
`../tinyusb/README.md`.

Public implementation and specification:

- https://github.com/hathach/tinyusb/tree/32530d8f4b6cd6d6f7a7df7e6358856c7ebc4f5e
- https://www.usb.org/document-library/device-class-specification-dfu-11

TinyUSB is distributed under the MIT license. Its vendored license is at
`../tinyusb/LICENSE`.
