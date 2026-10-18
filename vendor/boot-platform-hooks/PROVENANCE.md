# Boot platform hook provenance

These functions implement the platform hook and bounded handoff ABI expected
by the boot path. The no-op hooks are platform integration boundaries. The
trampoline view exposes the 16 KiB executable handoff range. This is original
OpenAVB code and contains no third-party source.
