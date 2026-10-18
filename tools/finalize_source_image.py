import binascii
import struct
import sys
from pathlib import Path

operation = sys.argv[1]
image_path = Path(sys.argv[2])
image = bytearray(image_path.read_bytes())
magic, version, header_size, image_size, entry_offset, identity = struct.unpack_from(
    "<IHHIIQ", image
)
assert magic == 0x53424953
assert version == 1
assert header_size == 32
assert image_size == len(image)
assert header_size <= entry_offset < image_size
assert identity == 0x5352434942535331
assert operation in ("finalize", "verify")

stored_crc32, reserved = struct.unpack_from("<II", image, 24)
assert reserved == 0
struct.pack_into("<I", image, 24, 0)
content_crc32 = binascii.crc32(image)
if operation == "finalize":
    assert stored_crc32 == 0
    struct.pack_into("<I", image, 24, content_crc32)
    image_path.write_bytes(image)
else:
    assert stored_crc32 == content_crc32
