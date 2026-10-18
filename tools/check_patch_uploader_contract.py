#!/usr/bin/env python3
"""Check that the host uploader and the ROM agree on the patch-store contract.

    tools/check_patch_uploader_contract.py <patch_store_tests binary>

Nothing else checks this contract: tools/patchup.py packs the stage header with
`struct`, and the ROM reads it as `AVPPatchStageHeader`.  This compares the
uploader's constants with the ones in include/avp/image/boot/patch_store.h and
src/devices/usb/usb_device.c, then builds real blobs with `patchup.build` and
replays each through the ROM's own patch_store.c (the test binary's
`--blob FILE --stage N` mode), so a field that moves on one side only fails
here instead of on a running machine.
"""
import hashlib
import importlib.util
import json
import re
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.dont_write_bytecode = True


def load_patchup():
    spec = importlib.util.spec_from_file_location(
        "patchup", ROOT / "tools" / "patchup.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def c_enum(path, name):
    text = (ROOT / path).read_text()
    match = re.search(r"\b%s\s*=\s*([^,\n]+)," % re.escape(name), text)
    assert match, "%s: %s not found" % (path, name)
    value = match.group(1).split("/*")[0].strip()
    value = value.replace("1u << 0", "1")
    return int(value, 0)


def der_length(size):
    if size < 0x80:
        return bytes([size])
    body = size.to_bytes((size.bit_length() + 7) // 8, "big")
    return bytes([0x80 | len(body)]) + body


def der(tag, body):
    return bytes([tag]) + der_length(len(body)) + body


def make_im4p(payload):
    return der(0x30, der(0x16, b"IM4P") + der(0x16, b"ibss") +
               der(0x16, b"test") + der(0x04, payload))


def main():
    if len(sys.argv) != 2:
        print(__doc__, file=sys.stderr)
        return 64
    harness = sys.argv[1]
    patchup = load_patchup()
    header_h = "include/avp/image/boot/patch_store.h"
    usb_c = "src/devices/usb/usb_device.c"

    assert patchup.MAGIC == c_enum(header_h, "PATCH_STORE_MAGIC")
    assert patchup.VERSION == c_enum(header_h, "PATCH_STORE_VERSION")
    assert patchup.FLAG_IDENTITY == c_enum(header_h,
                                           "PATCH_STAGE_FLAG_IDENTITY")
    for name, ordinal in patchup.STAGES.items():
        assert ordinal == c_enum(header_h, "PATCH_STAGE_" + name.upper()), name
    assert len(patchup.STAGES) == c_enum(header_h, "PATCH_STAGE_COUNT")
    for name, value in (("OPEN", patchup.REQ_OPEN),
                        ("APPEND", patchup.REQ_APPEND),
                        ("SEAL", patchup.REQ_SEAL),
                        ("REPORT", patchup.REQ_REPORT)):
        assert value == c_enum(usb_c, "PATCH_STORE_REQUEST_" + name), name
    assert struct.pack("<I", patchup.MAGIC) == b"PTCH"

    # im4p_payload walks DER rather than searching for one length form.
    for size in (0x20, 0x1000, 0x20000, 0x1000001):
        payload = (bytes(range(251)) * (size // 251 + 1))[:size]
        with tempfile.NamedTemporaryFile() as handle:
            handle.write(make_im4p(payload))
            handle.flush()
            assert patchup.im4p_payload(handle.name) == payload, hex(size)

    scan = {"patches": [
        {"offset": "0x2000", "instruction": "0xd503201f"},
        {"offset": "0x1000", "instruction": "0x52800000"},
        {"offset": "0x3000", "instruction": "0xd65f03c0", "stage": "ibec"},
        {"offset": "0x80000010", "instruction": "0xd503201f",
         "stage": "kernel"},
    ] + [{"offset": hex(0x10000 + index * 4), "instruction": "0xd503201f"}
         for index in range(300)]}

    with tempfile.TemporaryDirectory() as directory:
        directory = Path(directory)
        payload = b"\xa5" * 0x2345
        raw = b"\x5a" * 0x6789
        (directory / "image.im4p").write_bytes(make_im4p(payload))
        (directory / "image.raw").write_bytes(raw)
        cases = (
            ("ibss", dict(im4p=str(directory / "image.im4p"),
                          raw=str(directory / "image.raw"),
                          load_base=0x1FC050000), 302),
            ("ibec", {}, 303),
            ("sptm", {}, 0),
            ("kernel", {}, 1),
        )
        for name, extra, expected in cases:
            stage = patchup.STAGES[name]
            blob = patchup.build(scan, name, **extra)
            assert patchup.patch_count(blob) == expected, (name, expected)
            fields = patchup.HEADER.unpack_from(blob)
            assert fields[2] == expected
            if extra:
                assert fields[3] & patchup.FLAG_IDENTITY
                assert fields[6] == len(payload) and fields[7] == len(raw)
                assert fields[11] == hashlib.sha256(payload).digest()
                assert fields[12] == hashlib.sha256(raw).digest()
            path = directory / ("stage%d.bin" % stage)
            path.write_bytes(blob)
            subprocess.run([harness, "--blob", str(path), "--stage",
                            str(stage)], check=True)
    print(json.dumps({"patch_uploader_contract": "ok",
                      "stages": len(cases)}))
    return 0


if __name__ == "__main__":
    sys.exit(main())
