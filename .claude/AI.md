# AI Context for ZBC Semihosting Project

This file contains critical design information that must persist across conversation context resets.

---

## CRITICAL DESIGN PRINCIPLE - WHY ZBC MARSHALS DATA INTO RIFF CHUNKS

**Do NOT propose passing pointers for the host to chase. This is WRONG for ZBC.**

### Background

The ARM semihosting model (and picolibc's trap-based implementation) works because a debugger or trap handler already has full access to the CPU's memory space and can read/write anywhere. The guest passes pointers, and the host just reaches into guest memory to follow them.

### Why ZBC is Different

**For a memory-mapped peripheral that operates as a discrete device on the bus**, having it autonomously master the bus to chase arbitrary pointers is:

1. **More complex hardware** - requires DMA engine, bus arbitration
2. **A security/stability concern** - device can read/write anywhere in memory
3. **Architecturally messy** - device needs to understand guest's address space, MMU state, etc.

### The Correct Approach

**The RIFF-embedded approach is cleaner for a peripheral model:**

1. Guest packs everything into a single contiguous buffer
2. Guest tells device "here's my buffer" (via RIFF_PTR register)
3. Device reads/writes ONLY that buffer
4. Guest unpacks the result

The "cost" is that the client library must copy data into/out of the RIFF buffer. But this is **standard DMA-style I/O** - you don't expect a disk controller to scatter-gather from arbitrary application pointers either.

### Picolibc Integration Implication

The client library handles the packing/unpacking internally, presenting the same API that picolibc expects (`sys_semihost(op, param)`) while copying data to/from the RIFF buffer.

The `virtual.c` in picolibc must:
1. Receive `(op, param)` where `param` points to argument struct
2. **Read the argument struct from local memory** (guest-side)
3. **Marshal those values into RIFF PARM/DATA chunks**
4. Submit via ZBC protocol (RIFF_PTR + DOORBELL)
5. Parse RETN response
6. **Copy any returned DATA back to caller's buffers** (for SYS_READ, etc.)
7. Return result

**If you find yourself suggesting the host should chase pointers, STOP and re-read this file.**

---

## Project Structure

- `/home/jbyrd/git/semihost/` - ZBC semihosting spec and libraries
  - `semihost.md` - Protocol specification
  - `include/zbc_semi_*.h` - Library headers
  - `src/client/` - Guest-side client library
  - `src/host/` - Emulator-side host library

- `/home/jbyrd/git/picolibc/semihost/` - Picolibc semihosting integration
  - `machine/virtual/` - Memory-mapped semihosting backend (uses ZBC)
  - `common/` - Architecture-agnostic high-level API

---

## Key Design Decisions

- **RIFF marshalling**: All data copied into/out of RIFF DATA chunks (no pointer chasing)
- **Single buffer per call**: Caller provides one contiguous buffer
- **Zero allocation**: Libraries allocate no memory
- **Vtable pattern**: Function pointers for memory access and syscall handlers
- **Return codes**: No global errno - errors via return values
- **C90 compatible**: Maximum portability
- **`zbc_` prefix**: All library functions use this prefix (not `riff_`)
- **Wrap values individually**: When returning multiple values (e.g., SYS_HEAPINFO's 4 pointers), each value gets its own PARM chunk - do NOT collapse into a single binary DATA blob. This maintains the self-describing nature of RIFF.
