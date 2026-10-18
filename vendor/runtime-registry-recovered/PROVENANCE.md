# Runtime registry helper provenance

The registry record geometry and typed materialization contract are owned by
the existing OpenAVB registry implementation. These helpers add the 0x38-byte
checked cursor arithmetic, fixed table spans, source token, and observable
unsupported-tag failure. Integer overflow handling follows the C17 unsigned
arithmetic rules. This is original OpenAVB code; no third-party source code is
included.
