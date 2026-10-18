# OpenAVB

An open-source boot ROM for Apple's virtual iPhone and iPad, compatible with
AVPBooter. Boots iOS 15 through iOS 27 on
[vphone-cli and iphone-cli](#compatibility).

- Serves DFU and Recovery over USB
- Loads your signed iBSS, or LLB from the firmware disk
- Plain C17 for AArch64, no Apple code inside

Use it as the boot ROM of a compatible emulator or hypervisor, e.g.
`-bios build/OpenAVB.bin`.

> Not affiliated with Apple Inc. Apple and iPhone are trademarks of Apple Inc.

## Compatibility

| Host | Device | iOS 15 | iOS 16 | iOS 17 | iOS 18 | iOS 26 | iOS 27 |
| :-- | :-- | :-: | :-: | :-: | :-: | :-: | :-: |
| vphone-cli | iPhone17,3 | | | | | ✅ | ✅ |
| vphone-cli | iPad15,3 | | | | | ✅ | ✅ |
| iphone-cli | iPhone14,4 | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |

✅ boots &nbsp;·&nbsp; blank: not tested

vphone-cli runs on Virtualization.framework (`vresearch101` / `vphone600`)
with Apple's stock SEP ROM; iphone-cli runs on QEMU.

## Build

The build runs on macOS (Apple silicon or Intel) with the Xcode Command Line
Tools and:

```sh
brew install llvm lld uv ripgrep radare2
```

```sh
make            # build/OpenAVB.bin and build/SourceIBSS.bin
make verify     # build every image, run all host tests and image checks
make clean
```

Set `LLVM_PREFIX=` or `HOST_CC=` to use a toolchain outside Homebrew's
default paths. A single host test can be built and run on its own:

```sh
make build/usb_identity_tests && build/usb_identity_tests
```

## Outputs

| File | What it is |
| --- | --- |
| `build/OpenAVB.bin` | The ROM: a flat image linked at `0x00100000`, at most 512 KiB. `build/OpenAVB.elf` keeps DWARF for debugging. |
| `build/SourceIBSS.bin` | A source-built iBSS stand-in to send over DFU. |
| `build/NormalBootPayload.bin` | The normal-boot (block device) test payload. |
| `build/OpenAVB.<name>-probe.bin` | ROM images that each exercise one subsystem on a live machine. |

## Documentation

- [External contract](docs/external-contract.md): the memory map, devices,
  patch store, boot-args and USB identity a host must agree on.
- [Internals](docs/internals.md): repository tools, probe images,
  implementation notes, the work RAM layout and the source layout.
- [Open items](docs/open-items.md): known, deferred work.

## Acknowledgements

Thanks to [@lbr77](https://github.com/lbr77) for the initial work this
project builds on.

## License

OpenAVB is released under the MIT License. Third-party code under `vendor/`
and a few adapted files keep their own licences, including the Apple Public
Source License 2.0; [`LICENSE`](LICENSE) lists each of them, and each
`vendor/*/PROVENANCE.md` records the upstream source it pins.
