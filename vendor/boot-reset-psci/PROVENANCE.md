# Boot reset and PSCI provenance

The reset adapter follows the Arm Power State Coordination Interface protocol.

- Specification: https://developer.arm.com/documentation/den0022/latest/
- Function identifiers: `PSCI_VERSION = 0x84000000`,
  `SYSTEM_RESET = 0x84000009`
- Accepted versions: PSCI 0.2, 1.0, and 1.1

The AVP transport invokes PSCI through `SVC #0xa0`. The reset reason and early
platform hook around the standard PSCI call follow the expected ROM interface.
