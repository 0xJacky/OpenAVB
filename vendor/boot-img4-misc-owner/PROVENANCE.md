# Boot IMG4 miscellaneous owner provenance

DER INTEGER parsing uses the vendored Apple Security libDER implementation at
tag `Security-57740.60.18`, commit
`db455f62ee15e6adf20afe7c2e132321d2d0759c`, under APSL-2.0:

https://github.com/apple-oss-distributions/Security/tree/db455f62ee15e6adf20afe7c2e132321d2d0759c/OSX/libsecurity_keychain/libDER

The comparison return values and property flag follow the expected policy
interface.
