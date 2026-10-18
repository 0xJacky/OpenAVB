# Runtime root RPC provenance

- Nanopb, commit `d21fa5084287ab67da2f166f4def045bedcb535e`, zlib
  License: reviewed as a public, freestanding RPC serialization option. Its
  Protocol Buffers wire format does not match the AVP interface's field
  headers, pointer tokens, or packet sentinels, so no Nanopb source is copied.
- OpenAVB's existing `runtime_rpc` codec is reused for the custom AVP packet
  format. This keeps the canonical magic values, field indexes, pointer
  metadata, bounds checks, and finalization rules in one module.

Sources:

- https://github.com/nanopb/nanopb/blob/d21fa5084287ab67da2f166f4def045bedcb535e/pb_encode.c
- https://github.com/nanopb/nanopb/blob/d21fa5084287ab67da2f166f4def045bedcb535e/LICENSE.txt
- include/avp/runtime/ipc/runtime_rpc.h
- src/runtime/ipc/runtime_rpc.c
