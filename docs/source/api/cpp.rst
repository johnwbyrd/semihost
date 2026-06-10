C++ Host Library
================

The C++ host library (``zbc/Semihost.h``, namespace ``zbc``) is a C++17
alternative to the C host API, intended for emulators and other host-side
embedders that prefer object lifetimes, value types, and composition over C
vtables and callbacks. It implements the **same RIFF wire protocol** as the
C host -- in fact it reuses the C library's endianness helpers -- so a guest
cannot tell which host it is talking to, and the two are verified against each
other by a shared conformance test.

When to use which
-----------------

- Use the **C host** (:doc:`host`) for C codebases, or where you want the
  zero-allocation, C90 implementation.
- Use the **C++ host** for C++ emulators that want the ``Backend`` /
  ``Policy`` object model.

Architecture
------------

Four collaborators, each independently replaceable:

``zbc::GuestMemory``
   Abstract access to guest RAM (``readByte`` / ``writeByte`` and block
   helpers). You implement it over your emulator's memory model.

``zbc::Backend``
   Performs the actual host I/O. A capability ladder: ``Backend`` (inert)
   → ``ConsoleBackend`` (stdio, time, exit, timer) → ``FileBackend`` (files).
   Operations return ``OpResult`` (value + errno + optional data).

``zbc::Policy``
   Authorizes each operation, LSM-style: ``allowOpen`` / ``allowWrite`` /
   ``resolvePath`` / ... are consulted *before* the backend runs. Secure by
   default (everything denied). Presets: ``ConsoleOnlyPolicy``,
   ``SandboxedPolicy``, ``UnrestrictedPolicy``. Security is thus decoupled
   from capability -- e.g. a ``FileBackend`` behind a ``ConsoleOnlyPolicy``
   can only do console I/O.

``zbc::Device``
   The 32-byte memory-mapped peripheral. Owns the registers, decodes the
   16-byte guest-native ``RIFF_PTR``, parses requests, runs the
   Policy-then-Backend dispatch, and writes responses back into the guest's
   pre-allocated RETN/ERRO payloads. Conforms to spec v0.2.0: STATUS is a
   bitmask (TIMER / RESPONSE_READY / PROTO_ERROR), and unparseable requests
   are reported through the ERROR_CODE register without touching guest memory.

Integration sketch
-------------------

.. code-block:: cpp

   #include "zbc/Semihost.h"

   // 1. Bridge guest memory.
   class MyMem : public zbc::GuestMemory {
   public:
     uint8_t readByte(uint64_t a) override  { return Cpu.readByte(a); }
     void writeByte(uint64_t a, uint8_t v) override { Cpu.writeByte(a, v); }
   };

   // 2. Construct the device with platform config, a backend, and a policy.
   MyMem Mem;
   zbc::PlatformConfig Cfg(/*int*/2, /*ptr*/2, zbc::Endian::Little); // 6502
   auto OnExit  = [](unsigned r, unsigned) { /* stop the machine */ };
   auto OnTimer = [](unsigned hz) { return setTimerHz(hz); }; // false => EINVAL

   zbc::Device Dev(Mem, Cfg,
                   std::make_unique<zbc::FileBackend>(OnExit, OnTimer),
                   std::make_unique<zbc::SandboxedPolicy>("/srv/sandbox"));

   // 3. Wire the IRQ line for the periodic timer.
   Dev.setIrqCallback([](bool assert) { cpu_set_irq(assert); });

   // 4. Map the register window: forward CPU accesses in [base, base+32).
   uint8_t on_read(uint64_t off)            { return Dev.read(off); }
   void    on_write(uint64_t off, uint8_t v){ Dev.write(off, v); }

   // 5. When your timer fires:
   Dev.timerTick();   // sets STATUS.TIMER and asserts IRQ

Platform configuration and CNFG
-------------------------------

``PlatformConfig`` is required at construction -- an emulator always knows the
guest's integer size, pointer size, and byte order. Per the spec's
"platform-provided defaults", this makes the CNFG chunk an *override* rather
than a requirement: a request that omits CNFG is processed with the platform
config; a request that includes CNFG adopts its values for the session. The
C++ device therefore never raises ``Missing CNFG``.

Error reporting
---------------

Mirrors the C host's tiered model:

- Normal results and host ``errno`` go in the RETN chunk.
- Protocol errors are written to the guest's pre-allocated ERRO chunk when
  one exists and the container parsed.
- If the request is unparseable (or no ERRO chunk was provided), the device
  writes **nothing** to guest memory and instead latches the code into the
  ``ERROR_CODE`` register and sets ``STATUS`` bit 2 (PROTO_ERROR).

Value types
-----------

The library depends only on the C++17 standard library. ``zbc::Result<T>``
carries a value or an error message and ``zbc::Status`` a success/failure;
``zbc::ByteSpan`` is a minimal ``std::span``-like view; owned data is a
``std::vector<uint8_t>`` (``zbc::Bytes``). Strings cross the API as
``std::string_view``.
