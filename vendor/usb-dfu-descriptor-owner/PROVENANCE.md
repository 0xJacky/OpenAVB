# USB DFU descriptor owner provenance

The descriptor owner uses TinyUSB's public USB 2.0 and DFU descriptor types from
`vendor/tinyusb/src/common/tusb_types.h` and the public DFU descriptor constants
from `vendor/tinyusb/src/device/usbd.h`. TinyUSB is vendored at commit
`32530d8f4b6cd6d6f7a7df7e6358856c7ebc4f5e` under the MIT license.

The following public definitions establish the emitted wire layout:

- `tusb_desc_device_t`
- `tusb_desc_configuration_t`
- `tusb_desc_interface_t`
- `tusb_desc_dfu_functional_t`
- `TUSB_DESC_STRING`
- `TUSB_DESC_FUNCTIONAL`

The adapter implements a two-entry implementation registry, five identity
string arguments, dynamic string indices, local/remote initialization routing,
and an observable registration bounds fault. Private span metadata remains a
thin adapter around the TinyUSB-owned descriptor records.

Upstream: https://github.com/hathach/tinyusb/tree/32530d8f4b6cd6d6f7a7df7e6358856c7ebc4f5e
License: https://github.com/hathach/tinyusb/blob/32530d8f4b6cd6d6f7a7df7e6358856c7ebc4f5e/LICENSE
