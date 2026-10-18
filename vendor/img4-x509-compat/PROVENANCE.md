# IMG4 X.509 compatibility provenance

`src/image/img4/img4_x509_compat.c` parses X.509 certificates for the IMG4
trust path. It is original OpenAVB code (MIT) that calls the
libDER-compatible decoder in `src/image/der/` (see `vendor/apple-libder`).
Its item-spec tables follow the ASN.1 structures that RFC 5280 defines; no
libDER schema tables or other libDER source text are copied into it.

- Certificate profile: RFC 5280, https://www.rfc-editor.org/rfc/rfc5280
- DER encoding rules: ITU-T X.690
- IMG4 manifest certificate extension OID (1.2.840.113635.100.6.1.15), as
  declared publicly in XNU (APSL 2.0, declaration only, nothing copied):
  https://github.com/apple-oss-distributions/xnu/blob/f6217f891ac0bb64f3d375211650a4c1ff8ca1ea/EXTERNAL_HEADERS/CoreTrust/CTEvaluate.h

Exported entry points (names as the ROM interface expects them):

- `parse_chain`: parse each certificate, require an RSA SubjectPublicKeyInfo
  with absent or NULL parameters, and select the IMG4 manifest extension.
- `_crack_chain_with_anchor`: keep slot zero as the trust anchor and split a
  concatenated DER chain into the remaining slots.
- `parse_extensions`: parse a non-empty v3 extension sequence and return the
  encoded SET of the IMG4 manifest extension when present.
- `parse_algorithm_identifier`: split an AlgorithmIdentifier into its OID
  and optional parameters.
