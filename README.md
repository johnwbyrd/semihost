# ZBC Semihosting

C libraries for memory-mapped semihosting. Provides file I/O, console, and time services to guest programs, via a simple device register interface.

## Who Is This For?

- **Emulator authors** — add semihosting to your emulator so guest programs can access host files
- **libc porters** — implement `fopen`/`fread`/`fwrite` on a new target without real device drivers
- **Toolchain developers** — run compiler test suites on emulated hardware without writing filesystem drivers

## Features

- Works on any CPU from 8-bit to 64-bit (architecture-agnostic RIFF protocol)
- ARM semihosting compatible syscall numbers
- C90 compliant, zero heap allocation
- Secure (sandboxed) and insecure backends

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
