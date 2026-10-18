# Boot firmware provider owner provenance

The provider registration boundary uses the existing OpenAVB virtual
firmware loader and its VirtIO block implementation. Device discovery and block
transport semantics follow OASIS VirtIO 1.3.

The `root_firmware` and `virt_firmware` names, optional registration policy, and
observable required-provider failure follow the expected ROM interface.

Reference: https://docs.oasis-open.org/virtio/virtio/v1.3/virtio-v1.3.html
