# USB RPC command owner provenance

The USB RPC command owner uses OpenAVB's RuntimeRPC builder and field parser
as the private wire adapter. The command identifiers and result widths follow
the expected `usb_rpc_commands` interface.

TinyUSB at MIT commit `32530d8f4b6cd6d6f7a7df7e6358856c7ebc4f5e` remains
the public USB device and DFU implementation behind these commands. The RPC
layer carries lifecycle, descriptor, DFU, and SimpleVirtio requests without
duplicating TinyUSB's USB protocol state machines.

Upstream: https://github.com/hathach/tinyusb/tree/32530d8f4b6cd6d6f7a7df7e6358856c7ebc4f5e
License: https://github.com/hathach/tinyusb/blob/32530d8f4b6cd6d6f7a7df7e6358856c7ebc4f5e/LICENSE
