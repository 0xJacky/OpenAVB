# USB endpoint queue adapter provenance

Endpoint-number and direction decoding uses TinyUSB's public inline helpers
`tu_edpt_number` and `tu_edpt_dir` from the vendored MIT subset pinned at
commit `32530d8f4b6cd6d6f7a7df7e6358856c7ebc4f5e`.

Upstream: <https://github.com/hathach/tinyusb>

Pinned source: <https://github.com/hathach/tinyusb/commit/32530d8f4b6cd6d6f7a7df7e6358856c7ebc4f5e>

The twelve `0x70`-byte endpoint records, controller offsets, bounded tuples,
setup-event six-byte prefix, phase transitions, transport arguments, and
move-and-clear ownership rule follow the expected private interface and are
implemented by original OpenAVB code.
