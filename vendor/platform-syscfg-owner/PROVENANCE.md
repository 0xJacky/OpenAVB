# Platform SysCfg owner provenance

This module implements the Platform SysCfg source registry, source lifecycle,
payload lookup, and registration-record flow, including the 0xc0-byte source
layout, access-class mapping, status mask, FourCC name encoding, and callback
interfaces.

DER parsing and the owned SysConfig3 object model are delegated to the existing
OpenAVB `DERDecodeSysConfig3`, `sysConfig3GetPayloadEntryForKey`, and payload
accessor APIs. Those APIs are based on Apple's public libDER implementation at
commit `db455f62ee15e6adf20afe7c2e132321d2d0759c` under APSL-2.0. The platform
registry also uses the existing OpenAVB runtime-log callback owner. The
private source ABI remains a thin original adapter under the repository's
MIT license.

## Public source

- Apple Security libDER: https://github.com/apple-oss-distributions/Security/tree/db455f62ee15e6adf20afe7c2e132321d2d0759c/libsecurity_der
- APSL 2.0: https://opensource.apple.com/apsl/
