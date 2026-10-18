# Runtime RPC codec provenance

The implementation provides AVP's fixed 64-bit RPC format. Nanopb was reviewed
as the closest small freestanding public codec before the wire contract was
implemented.

| Project | Commit | License | Reviewed implementation |
| --- | --- | --- | --- |
| nanopb | `d21fa5084287ab67da2f166f4def045bedcb535e` | Zlib | `pb.h`, `pb_encode.c` |

Upstream URL:

- https://github.com/nanopb/nanopb/tree/d21fa5084287ab67da2f166f4def045bedcb535e

Nanopb encodes Protocol Buffers wire types as varint, 64-bit, string, and
32-bit fields. AVP uses fixed packet and field magics, qword-aligned payloads,
four-word bounded views, and reserved pointer-class tags. The codec helpers
therefore remain local and reuse the existing OpenAVB RPC types. No Nanopb
source text is copied.
