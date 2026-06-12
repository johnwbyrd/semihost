=====================
Library Architecture
=====================

The ZBC host library is the code that lives on the host side -- the
emulator, simulator, or developer machine that services semihosting
requests from a guest CPU. It exists in two implementations: a C90
reference library (:doc:`api/host`, :doc:`api/backend`,
:doc:`api/protocol`) and a C++17 alternative (:doc:`api/cpp`). Both
speak the identical RIFF wire protocol defined in :doc:`specification`,
and the conformance test in ``test/conformance/test_conformance.cpp``
enforces byte-for-byte equivalence between them on every CI run.

This page describes how the host library is structured -- what the
collaborators are, how they fit together, and where the customization
seams are -- in terms that apply equally to both implementations.

When to use which
-----------------

- Use the **C host** for C codebases, or where you want the
  zero-allocation, C90 implementation.
- Use the **C++ host** for C++ emulators that want the ``Backend`` /
  ``Policy`` object model.

The collaborator model
----------------------

Four collaborators, each independently replaceable:

``GuestMemory``
   Abstract access to guest RAM (``readByte`` / ``writeByte`` and block
   helpers). The embedder implements it over the emulator's memory
   model. In C this is a callback struct (``zbc_host_mem_ops_t``); in
   C++ it is the ``zbc::GuestMemory`` base class.

``Backend``
   Performs the actual host I/O. A capability ladder: ``Backend`` (inert)
   → ``ConsoleBackend`` (stdio, time, exit, timer) → ``FileBackend``
   (files). Operations return a result value, an errno, and optional
   response data. In C this is the ``zbc_backend_t`` vtable; in C++ it
   is the ``zbc::Backend`` class hierarchy returning ``zbc::OpResult``.

``Policy``
   Authorizes each operation, LSM-style: ``allowOpen`` / ``allowWrite``
   / ``resolvePath`` / ... are consulted *before* the backend runs.
   Secure by default (everything denied). Presets: ``ConsoleOnlyPolicy``,
   ``SandboxedPolicy``, ``UnrestrictedPolicy``. Security is thus
   decoupled from capability -- e.g. a ``FileBackend`` behind a
   ``ConsoleOnlyPolicy`` can only do console I/O.

``Device``
   The 32-byte memory-mapped peripheral. Owns the registers, decodes
   the 16-byte guest-native ``RIFF_PTR``, parses requests, runs the
   Policy-then-Backend dispatch, and writes responses back into the
   guest's pre-allocated RETN/ERRO payloads. Conforms to spec v0.2.0:
   STATUS is a bitmask (TIMER / RESPONSE_READY / PROTO_ERROR), and
   unparseable requests are reported through the ``ERROR_CODE`` register
   without touching guest memory.

Platform configuration and CNFG
-------------------------------

The host library is told the guest's integer size, pointer size, and
byte order at construction time -- via ``PlatformConfig`` in C++, via
``zbc_host_set_platform_config()`` in C. Per the spec's
"platform-provided defaults", this makes the CNFG chunk an *override*
rather than a requirement: a request that omits CNFG is processed with
the platform configuration; a request that includes CNFG adopts its
values for the session. A host with platform-provided defaults never
raises ``Missing CNFG``.

Error reporting
---------------

Both host libraries use the same tiered error model:

- Normal results and host ``errno`` go in the RETN chunk.
- Protocol errors are written to the guest's pre-allocated ERRO chunk
  when one exists and the container parsed.
- If the request is unparseable (or no ERRO chunk was provided), the
  device writes **nothing** to guest memory and instead latches the
  code into the ``ERROR_CODE`` register and sets ``STATUS`` bit 2
  (PROTO_ERROR).

Integration sketch (C++)
------------------------

The following sketch shows how the four collaborators come together in
the C++ host. The C library's integration pattern -- callback structs
plus the ``zbc_host_init`` / ``zbc_host_process`` entry points -- is
covered in :doc:`emulator-integration`.

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
