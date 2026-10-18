# Device remaining closure owner provenance

The final device closure reuses OpenAVB's SimpleVirtio MMIO adapter, which
implements the public Virtio MMIO initialization model, and the existing SEP
protocol owner for the private mailbox ABI. The USB transport above this
MMIO device remains TinyUSB at the pinned MIT revision documented in
`../tinyusb/README.md`.

Public specifications and implementation:

- https://docs.oasis-open.org/virtio/virtio/v1.0/virtio-v1.0.html
- https://github.com/hathach/tinyusb/tree/32530d8f4b6cd6d6f7a7df7e6358856c7ebc4f5e

TinyUSB is distributed under the MIT license. Its vendored license is at
`../tinyusb/LICENSE`.
