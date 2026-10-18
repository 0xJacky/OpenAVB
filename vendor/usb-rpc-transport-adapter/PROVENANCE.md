# USB RPC transport adapter provenance

The adapter is original OpenAVB code implementing the private USB service
envelope. Packet construction delegates to OpenAVB's existing `runtime_rpc`
implementation.

Standard USB device and DFU behavior remains owned by the vendored TinyUSB
device core and DFU class driver:

- Upstream: https://github.com/hathach/tinyusb
- Commit: `32530d8f4b6cd6d6f7a7df7e6358856c7ebc4f5e`
- License: MIT

The private service identifier `0x627375`, SVC `0xc2`, builder layout, field
tokens, and response-field-zero rule have no upstream TinyUSB equivalent.
They stay in this thin adapter so the public USB implementation remains
separate from the private transport ABI.
