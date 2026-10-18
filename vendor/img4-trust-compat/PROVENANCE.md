# IMG4 RSA trust compatibility provenance

`src/image/img4/img4_trust_compat.c` verifies RSA signatures for the IMG4
trust path. It is original OpenAVB code (MIT) that calls the
libDER-compatible decoder in `src/image/der/` (see `vendor/apple-libder`);
the RSAPublicKey structure it parses is the one RFC 8017 defines. RSA
verification is routed through the RSA public-key compatibility module
(`vendor/corecrypto-rsa-public-compat`).

- RSA key syntax and EMSA-PKCS1-v1_5: RFC 8017,
  https://www.rfc-editor.org/rfc/rfc8017

Exported entry points:

- `verify_pkcs1_sig`: parse an RSA SubjectPublicKey, build a bounded public
  context, and verify a precomputed digest and signature.
- `verify_signature_rsa`: verify a 4096-bit RSA signature over a digest.
- `img4_make_rsa_public_key_checked`: strip INTEGER padding, enforce the
  caller's bound, and import the RSA key.
- `verify_chain_signatures`: check the signature AlgorithmIdentifier, hash
  the encoded TBSCertificate, and verify its signature.
