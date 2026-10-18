# Runtime boot handoff provenance

The architectural handoff sequence follows the Arm Architecture Reference
Manual requirements for cache maintenance, translation invalidation, and
exception-level state transfer. The public reference is Arm's ABI repository,
commit `ee4b3c1d680c1ffbc4d66177998f75a5517d6d59`, licensed Apache-2.0 WITH
LLVM-exception.

The AVP one-shot commitment flag, scrub range layout, source token, and service
shutdown ordering follow the expected private ROM interface. Their
implementation is local and uses explicit callback boundaries so the host tests
can observe every side effect.

Reference: https://github.com/ARM-software/abi-aa/tree/ee4b3c1d680c1ffbc4d66177998f75a5517d6d59
