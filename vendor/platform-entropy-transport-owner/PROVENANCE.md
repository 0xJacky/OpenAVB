# Platform entropy transport owner provenance

This module implements the platform entropy request interface: request UUIDs,
record layout, descriptor operations, register-slot spacing, status bits,
MMIO addresses, and progress codes.

The 48-byte command descriptor is a private platform protocol: version and
kind, a 128-bit request identifier, request size, operation, DMA-visible data
address, and transfer size. No public protocol library exposes this wire ABI.
The source therefore keeps the private encoding in a thin transport owner and
exposes callbacks for address translation, submission, and progress
publication. Host tests use those callbacks as the device boundary.

The implementation is original project code. It uses C17 fixed-width integer
and object-layout facilities only and contains no copied third-party source.
This provenance record documents its technical sources and makes no independent
license grant.

## Public references reviewed

- ISO C working group public C17 draft: https://www.open-std.org/jtc1/sc22/wg14/www/docs/n2176.pdf
- Arm memory-mapped I/O ordering overview: https://developer.arm.com/documentation/102376/latest/
