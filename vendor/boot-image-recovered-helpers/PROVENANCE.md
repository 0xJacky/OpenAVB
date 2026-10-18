# Boot image helpers provenance

The DER and IMG4 parsing boundary follows Apple Security libDER from tag
`Security-57740.60.18`, commit
`db455f62ee15e6adf20afe7c2e132321d2d0759c`, licensed under APSL-2.0.
The implementation reuses OpenAVB's Apple libDER and IMG4 compatibility
owners. Bounded tuples, breadcrumb values, and digest publication are original
OpenAVB adapters.

Upstream: https://github.com/apple-oss-distributions/Security/tree/db455f62ee15e6adf20afe7c2e132321d2d0759c
License: https://github.com/apple-oss-distributions/Security/blob/db455f62ee15e6adf20afe7c2e132321d2d0759c/APPLE_LICENSE
