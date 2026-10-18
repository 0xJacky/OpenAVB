# Boot IMG4 parser owner provenance

The parser directly uses Apple Security libDER from tag
`Security-57740.60.18`, commit
`db455f62ee15e6adf20afe7c2e132321d2d0759c`, licensed under APSL-2.0.
Cursor and bounded-intersection behavior is an original OpenAVB adapter over
the public DER decoder.

Upstream: https://github.com/apple-oss-distributions/Security/tree/db455f62ee15e6adf20afe7c2e132321d2d0759c
License: https://github.com/apple-oss-distributions/Security/blob/db455f62ee15e6adf20afe7c2e132321d2d0759c/APPLE_LICENSE
