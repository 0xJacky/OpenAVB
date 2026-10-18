# SimpleVirtio USB adapter provenance

The adapter is original OpenAVB code implementing the private SimpleVirtio USB
tuple and event-envelope helpers.

The device/queue model follows the public VirtIO MMIO specification:

- Specification: https://docs.oasis-open.org/virtio/virtio/v1.3/virtio-v1.3.html
- Version: VirtIO 1.3
- Publisher: OASIS Open

USB protocol behavior remains implemented by TinyUSB. The private slot 16,
device tuple types, four endpoint completion queues, and six-byte event
envelope remain isolated in this adapter.
