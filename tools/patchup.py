#!/usr/bin/env python3
"""patchup.py -- hand the boot ROM the patch table the host worked out.

The ROM used to carry the whole description of the image it boots, generated
into its source at build time.  That is right for exactly one build: the sizes,
the two digests, the load address and every patch offset are wrong the moment
the payload is rebuilt.  An anchor scanner on the host finds the sites by
anchor instead, so the description can be produced per image, and this
uploads it.

    patchup.py build  <scan.json> --stage ibss --im4p iBSS.im4p --raw ibss.bin \
                      --load-base 0x1FC050000 [-o stage0.bin]
    patchup.py send   <stage0.bin> --stage ibss

Four vendor requests answer on the recovery interface, in DFU mode as well as
recovery, because the tables for iBSS and iBEC have to be in place before the
image runs:

    0x70 out  wValue=stage                      open
    0x71 out  wValue=stage wIndex=chunk + data  append, in order
    0x72 out  wValue=stage wIndex=count         seal
    0x73 in                                     report what the ROM holds

`scan.json` is whatever the host's scanner produced, in this shape:

    {"patches": [{"offset": "0x1234", "instruction": "0xd503201f",
                  "stage": "ibss"}, ...]}

`offset` is a byte offset into the decompressed image, `instruction` the
32-bit word written there, and `stage` (optional) pins the row to one stage;
rows without it apply to both iBoot stages (ibss, ibec) and to nothing else.
`send` needs pymobiledevice3 (and pyusb) and a device in DFU or recovery.
`make verify` checks this file's packing against the ROM with
tools/check_patch_uploader_contract.py.
"""
import argparse, hashlib, json, struct, sys

STAGES = {'ibss': 0, 'ibec': 1, 'sptm': 2, 'kernel': 3}
MAGIC = 0x48435450          # 'PTCH'
VERSION = 1
FLAG_IDENTITY = 1
HEADER = struct.Struct('<IIIIIIIIQII32s32s')
assert HEADER.size == 112, HEADER.size

REQ_OPEN, REQ_APPEND, REQ_SEAL, REQ_REPORT = 0x70, 0x71, 0x72, 0x73
CHUNK = 0x400


def _der(data, i):
    """One DER element at `i`: (tag, body_start, body_end)."""
    tag = data[i]
    n = data[i + 1]
    i += 2
    if n & 0x80:
        k = n & 0x7F
        n = int.from_bytes(data[i:i + k], 'big')
        i += k
    return tag, i, i + n


def im4p_payload(path):
    """The payload inside an im4p, which is what the ROM measures.

    A real walk, not a search.  This used to take "the largest octet string
    whose length is written in the 0x04 0x83 three-byte form", which is right
    only for a payload between 64 KiB and 16 MiB: 24A437's device tree (50 kB,
    two-byte form) and its kernelcache (21 MB, four-byte form) both read as
    "no payload octet string found" or as the wrong element entirely, and the
    stage header built from the wrong bytes would have been a digest the ROM
    could never match.

    The structure is SEQUENCE { "IM4P", type, version, payload, ... } and the
    payload is the first OCTET STRING in it.
    """
    data = open(path, 'rb').read()
    if not data[:1] == b'\x30':
        return data                      # already a bare payload
    _, body, end = _der(data, 0)
    i = body
    seen_magic = False
    while i < end:
        tag, s_, e_ = _der(data, i)
        if tag == 0x16 and data[s_:e_] == b'IM4P':
            seen_magic = True
        if tag == 0x04 and seen_magic:
            return data[s_:e_]
        i = e_
    raise SystemExit('%s: no payload octet string found' % path)


def build(scan, stage, im4p=None, raw=None, load_base=None, entry_level=2):
    # A row marked for a stage belongs only to that stage.  The two copies of
    # this firmware are the same binary and almost always want the same words,
    # but not always: iBSS needs the image4 evaluation to decode the iBEC it
    # was handed, and iBEC needs it answered yes so the kernel is taken.
    #
    # An unmarked row belongs to the two iBoot stages and to nothing else.  It
    # used to default to whichever stage was being built, which is harmless
    # while only iBSS and iBEC are ever built and wrong the moment a third one
    # is: asked for `kernel`, every iBoot row came along and seventy iBoot
    # offsets were written into the kernelcache.
    IBOOT_STAGES = ('ibss', 'ibec')
    patches = [(int(p['offset'], 16), int(p['instruction'], 16))
               for p in scan['patches']
               if (p['stage'] == stage if 'stage' in p
                   else stage in IBOOT_STAGES)]
    patches.sort()
    flags = 0
    csize = rsize = 0
    cdigest = rdigest = b'\0' * 32
    if im4p and raw:
        payload = im4p_payload(im4p)
        body = open(raw, 'rb').read()
        csize, cdigest = len(payload), hashlib.sha256(payload).digest()
        rsize, rdigest = len(body), hashlib.sha256(body).digest()
        flags |= FLAG_IDENTITY
        if load_base is None:
            raise SystemExit('an identity needs --load-base')
    header = HEADER.pack(
        MAGIC, VERSION, len(patches), flags,
        0, 0,                       # received / next_chunk, the ROM's to keep
        csize, rsize,
        load_base or 0, entry_level, 0,
        cdigest, rdigest)
    body = b''.join(struct.pack('<II', off, word) for off, word in patches)
    return header + body


def send(dev, blob, stage):
    """Open, stream, seal, then read back what the ROM says it holds.

    pymobiledevice3's IRecv forwards keywords to pyusb, so the wValue/wIndex the
    ROM reads are named here rather than positional.
    """
    dev.ctrl_transfer(0x41, REQ_OPEN, wValue=stage, wIndex=0,
                      data_or_wLength=b'', timeout=5000)
    for index in range((len(blob) + CHUNK - 1) // CHUNK):
        piece = blob[index*CHUNK:(index+1)*CHUNK]
        dev.ctrl_transfer(0x41, REQ_APPEND, wValue=stage, wIndex=index,
                          data_or_wLength=piece, timeout=5000)
    dev.ctrl_transfer(0x41, REQ_SEAL, wValue=stage, wIndex=patch_count(blob),
                      data_or_wLength=b'', timeout=5000)
    report = dev.ctrl_transfer(0xc0, REQ_REPORT, wValue=0, wIndex=0,
                               data_or_wLength=4 * 12, timeout=5000)
    raw = bytes(report)
    return [struct.unpack_from('<III', raw, n*12) for n in range(4)]


def patch_count(blob):
    return (len(blob) - HEADER.size) // 8


def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest='verb', required=True)
    b = sub.add_parser('build')
    b.add_argument('scan'); b.add_argument('--stage', required=True, choices=STAGES)
    b.add_argument('--im4p'); b.add_argument('--raw')
    b.add_argument('--load-base'); b.add_argument('--entry-level', type=int, default=2)
    b.add_argument('-o', '--out')
    s = sub.add_parser('send')
    s.add_argument('blob'); s.add_argument('--stage', required=True, choices=STAGES)
    a = ap.parse_args()

    if a.verb == 'build':
        scan = json.load(open(a.scan))
        blob = build(scan, a.stage, a.im4p, a.raw,
                     int(a.load_base, 0) if a.load_base else None, a.entry_level)
        out = a.out or ('stage%d.bin' % STAGES[a.stage])
        open(out, 'wb').write(blob)
        n = patch_count(blob)
        print('%s: %d patches, %d bytes%s'
              % (out, n, len(blob), ', with an image identity'
                 if a.im4p and a.raw else ', patches only'))
        return
    from pymobiledevice3.irecv import IRecv
    blob = open(a.blob, 'rb').read()
    dev = IRecv(timeout=5000)
    for n, (c, received, sealed) in enumerate(
            send(dev, blob, STAGES[a.stage])):
        print('stage %d: %d patches, %d bytes, %s'
              % (n, c, received, 'sealed' if sealed else 'open'))


if __name__ == '__main__':
    main()
