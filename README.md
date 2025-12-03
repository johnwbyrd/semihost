# ZBC Semihosting

This is a reference implementation of a semihosting device suitable for use with the [Zero Board Computer](https://www.zeroboardcomputer.com).

It contains C libraries for both implementing a virtual semihosting device, as well as libraries for marshaling function calls to and from the semihosting device.

A semihosting device provides file I/O, console, and time services to guest programs, via a simple device register interface.  Your software can immediately access completely filesystem and timekeeping services, without you explicitly needing to port them to your new emulator or architecture.

## Who Is This For?

- **Bare-metal developers** -- people who need filesystem, console, and time services immediately on a new architecture
- **Toolchain and SDK developers** -- run compiler test suites on emulated hardware, without writing filesystem drivers
- **Emulator authors** -- add semihosting to your emulator so guest programs can access host files
- **libc porters** -- implement `fopen`/`fread`/`fwrite` on a new target without real device drivers

## Features

- Works on any CPU from 8-bit to 64-bit (architecture-agnostic RIFF protocol)
- ARM semihosting compatible syscall numbers
- C90 compliant, extremely portable, with zero heap allocation
- Secure (sandboxed) and insecure backends
- GitHub test suite for Ubuntu, MacOS, and Windows
- GitHub automatic fuzzing of RIFF parser to reduce possibility of malicious use

## Documentation

| I want to... | Read this |
|--------------|-----------|
| Add semihosting to my emulator | [Emulator Integration](docs/emulator-integration.md) |
| Use semihosting from guest code | [Client Library](docs/client-library.md) |
| Understand the wire protocol | [Protocol Specification](docs/specification.md) |

## Building

```bash
cmake -B build && cmake --build build
```

## Testing

```bash
ctest --test-dir build
```

Tests run on Linux, macOS, and Windows. CI includes AddressSanitizer, UndefinedBehaviorSanitizer, and continuous fuzzing via ClusterFuzzLite.

## Related Projects

- [MAME](https://github.com/mamedev/mame) — includes ZBC machine drivers
- [zeroboardcomputer.com](https://www.zeroboardcomputer.com) — ZBC specification

## License

[MIT](LICENSE) (SPDX: `MIT`)
