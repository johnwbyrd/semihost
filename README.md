# ZBC Semihosting Libraries

C libraries for implementing the ZBC semihosting device.

**ZBC** (Zero Board Computer) is a minimal hardware specification for bringing up compilers and libraries on new CPUs. **Semihosting** provides I/O services (file access, console, timekeeping) to guest programs via a memory-mapped device instead of architecture-specific trap instructions.

## Documentation

| I want to... | Read this |
|--------------|-----------|
| Add semihosting to my emulator | [Emulator Integration Guide](docs/emulator-integration.md) |
| Port the client library to a new target | [Client Library Guide](docs/client-library.md) |
| Understand the wire protocol | [Protocol Specification](docs/specification.md) |

## Quick Start

```bash
cmake -B build && cmake --build build && ctest --test-dir build
```

## Related Projects

- [MAME](https://github.com/mamedev/mame) — Includes ZBC machine drivers and semihosting plugin
- [zeroboardcomputer.com](https://www.zeroboardcomputer.com) — ZBC specification and documentation

## License

BSD-3-Clause
