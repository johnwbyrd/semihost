QEMU Guest Transports Proposal
==============================

.. warning::

   This document describes a **proposal for future expansion** of the ZBC
   client library. **None of these transports are currently implemented.**

   The purpose of this document is to design how a guest program built
   against the ZBC client API can run unmodified on a stock QEMU machine
   that has no ZBC semihosting device, by thunking semihost calls down to
   devices QEMU already ships: virtio-9p for files, virtio-console for the
   console, and (where available) QEMU's native trap-based semihosting.

.. contents:: Table of Contents
   :local:
   :depth: 2

Overview
--------

Motivation
^^^^^^^^^^

Today a ZBC guest requires a host with a ZBC semihosting device: MAME's ZBC
machines, an emulator that embeds one of the host libraries, or future
silicon. QEMU ships no such device, and patching one in (while planned as a
proof of concept) means users must build a custom QEMU.

The goal of this proposal is the inverse arrangement: **the host stays
stock; the guest carries the drivers.** A guest program written against the
ZBC client API -- ``zbc_api_*``, ``zbc_call()``, or the ARM-style
``zbc_semihost()`` entry point -- recompiles (or, per platform, simply
relinks) and runs identically on:

- a MAME ZBC machine, via the RIFF/doorbell device protocol (today's path);
- a stock QEMU machine, via devices QEMU already provides;
- real or FPGA hardware with a physical ZBC device.

The invariant is the **guest-facing interface**, not the wire. The RIFF
protocol defined in :doc:`specification` remains the canonical wire format
for the ZBC device and is unaffected by this proposal; it becomes one
*transport* among several rather than the only one.

What Stock QEMU Provides
^^^^^^^^^^^^^^^^^^^^^^^^

Three host-side capabilities exist in unmodified QEMU and map onto the
semihosting operation set:

.. list-table::
   :header-rows: 1
   :widths: 25 30 45

   * - QEMU facility
     - Covers
     - Notes
   * - Trap semihosting (``-semihosting``)
     - Everything (files, console, time, exit)
     - ARM, AArch64, RISC-V, MIPS, m68k, Xtensa only; not x86
   * - virtio-9p (``-fsdev`` / ``-virtfs``)
     - File operations
     - Built into QEMU; no external daemon (unlike virtio-fs/virtiofsd)
   * - virtio-console
     - Console I/O
     - Rides the same virtqueue core as 9p

virtio-9p is chosen over virtio-fs deliberately: virtio-fs requires the
external ``virtiofsd`` vhost-user daemon and a FUSE client in the guest,
while 9P2000.L is a compact synchronous RPC protocol served by QEMU itself.
Its strict one-request/one-response model maps one-to-one onto ZBC's
synchronous call model.

Architecture: The Transport Seam
--------------------------------

Current Call Path
^^^^^^^^^^^^^^^^^

``zbc_call()`` in ``src/c/zbc_client.c`` currently hard-wires three steps:
build a RIFF request from the opcode table, submit it by writing RIFF_PTR
and DOORBELL (``zbc_client_submit()``), and parse the RETN/ERRO response.

Proposed Seam
^^^^^^^^^^^^^

Introduce a guest-side transport vtable at the **opcode level** -- the
mirror image of the host's ``zbc_backend_t``:

.. code-block:: c

   /* A transport executes one semihosting operation. It fills the
    * response (result, errno, optional data) exactly as the RIFF
    * transport does today. */
   typedef struct zbc_transport_s {
       int (*call)(void *ctx, zbc_response_t *response,
                   int opcode, uintptr_t *args);
   } zbc_transport_t;

``zbc_client_state_t`` gains a transport pointer (plus context).
``zbc_call()`` becomes a dispatcher; the existing
build-RIFF/doorbell/parse path moves behind the vtable as the **default
transport**, preserving current behavior bit-for-bit. ``zbc_api_*`` and
``zbc_semihost()`` do not change at all.

The transport field is public. Assigning it directly before the first
call overrides the probe chain entirely -- that is the whole
selection-override mechanism, and there is no dedicated API for it.
Users own the code; they can break it if they want.

A transport need not implement every opcode itself; transports compose.
The composition mirrors the C++ host library's Backend hierarchy
(inert base → ``ConsoleBackend`` → ``FileBackend``): a console-capable
transport layers over an inert base that fails everything with ``ENOSYS``,
and a file-capable transport layers over that. Concretely, the QEMU
transport is a dispatcher that routes console opcodes (and file opcodes on
fds 0-2) to the virtio-console driver, file opcodes to the 9p driver, and
everything else to platform hooks.

Normative Equivalence Requirement
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Whatever the transport, the observable semantics at the ``zbc_call()``
boundary MUST be identical:

- Return value conventions follow :doc:`specification` (e.g. SYS_WRITE
  returns bytes **not** written; SYS_READ returns bytes **not** read).
- ``response->error_code`` carries a POSIX errno, 0 on success.
- File descriptors 0-2 are console (stdin/stdout/stderr); user files
  start at 3 -- matching the host libraries' ``FileDescTable``.
- ``SYS_ERRNO`` returns the errno of the most recent failing operation.
- Unimplemented operations fail with result -1 and errno ``ENOSYS``;
  they never trap or hang.

A transport-equivalence test suite enforces this (see `Testing Strategy`_).

Transport Discovery
-------------------

Probe Chain
^^^^^^^^^^^

Runtime discovery is signature-driven, which is precisely what the ZBC
SIGNATURE register exists for. At ``zbc_client_init()`` time (or first
call), the library probes in order:

1. **ZBC device.** Read 8 bytes at the platform's device base (the
   address-placement formula in :doc:`specification`). If they spell
   ``SEMIHOST``, select the RIFF/doorbell transport. A machine that has a
   ZBC device -- MAME, patched QEMU, silicon -- always wins, so the same
   binary prefers the native device wherever one exists.
2. **virtio-mmio scan.** Walk the platform's virtio-mmio window (a small
   per-platform table, below). A slot is live if MagicValue (offset 0x000)
   reads ``0x74726976`` ("virt", little-endian) and Version (offset 0x004)
   is 2 (modern). DeviceID (offset 0x008) selects the driver: 3 = console,
   9 = 9P transport, 0 = empty slot. Select the QEMU composite transport
   with whichever devices were found.
3. **No transport.** Select the **null transport**: every operation
   returns -1 with errno ``ENOSYS``, immediately and deterministically.
   The library never hangs. The selected transport is a public field of
   ``zbc_client_state_t``, so a guest that wants to react to probe
   failure can inspect it.

Trap semihosting is **not** part of the runtime chain (see below); it is a
build-time selection.

Known virtio-mmio Windows
^^^^^^^^^^^^^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1
   :widths: 25 20 15 15 25

   * - QEMU machine
     - Base
     - Stride
     - Slots
     - Notes
   * - ARM/AArch64 ``virt``
     - 0x0a000000
     - 0x200
     - 32
     - DTB also describes these; fixed in practice
   * - RISC-V ``virt``
     - 0x10001000
     - 0x1000
     - 8
     - rv32 and rv64
   * - x86 ``microvm``
     - 0xfeb00000
     - 0x200
     - 8
     - Avoids PCI enumeration on x86

These constants live in the per-platform port (alongside the ZBC device
base address), not in the transport core. Machines that expose virtio only
over PCI (x86 ``pc``/``q35``) are **permanently out of scope**: PCI
enumeration is exactly the kind of platform-specific surface this library
refuses to grow, and ``microvm`` is the supported x86 machine.

Transport: Native Trap Semihosting
----------------------------------

ZBC deliberately uses ARM semihosting opcode numbers, and QEMU implements
trap-based semihosting for ARM, AArch64, RISC-V, MIPS, m68k, and Xtensa
when started with ``-semihosting`` (or ``-semihosting-config``). On these
targets the entire transport is a per-architecture thunk of roughly ten
instructions: place the opcode and parameter-block pointer in the
ABI-defined registers and execute the trap sequence (e.g. ``hlt #0xf000``
on AArch64; the ``slli``/``ebreak``/``srai`` sequence on RISC-V).

Properties:

- Covers the **entire** operation set -- files, console, time, exit --
  with zero guest driver code and zero host configuration beyond the flag.
- The parameter block layout is exactly what ``zbc_semihost()`` already
  accepts, so the thunk slots in beneath the existing API trivially.
- **Not safely probeable.** Executing the trap sequence on a QEMU started
  without ``-semihosting`` (or on real hardware) raises an exception. The
  known workaround -- install a fault handler, attempt a benign call such
  as SYS_ERRNO, recover -- drags per-architecture vector management into
  the library. Therefore the trap transport is an explicit **build-time
  selection** (``-DZBC_TRANSPORT_TRAP``) for ports that want it, not a
  link in the runtime probe chain. This is a final decision, not a
  stopgap: maintaining per-architecture fault shims is precisely the
  platform-specific code this library exists to avoid.

Where the trap transport is selected, the virtio drivers are unnecessary
and are not linked.

Transport: virtio Core
----------------------

Both virtio drivers share one transport core:

**virtio-mmio, modern (version 2) only.** Legacy (version 1) devices are
rejected; QEMU has defaulted to modern for years and supporting both
doubles the test matrix for no audience.

**Single polled split virtqueue engine.** ZBC semihosting is synchronous,
so the driver never needs interrupts: place buffers in the descriptor
table, publish to the available ring, write QueueNotify, then spin reading
the used ring's index until the device consumes the request. This mirrors
the "portable guests poll RESPONSE_READY" discipline of the ZBC device
itself.

**Minimal feature negotiation.** Acknowledge ``VIRTIO_F_VERSION_1``;
offer nothing else. In particular the console's ``MULTIPORT`` feature is
*not* negotiated, which pins the console to the simple two-queue layout.

**Static allocation.** The client library's zero-heap rule (statically
verified by ``test/CMakeLists.txt``) applies. Virtqueue rings, the 9p
message buffer, and the fd table live in caller-provided or static storage.
Queue sizes are negotiated down to small powers of two (8 descriptors
suffices for strictly serial request/response use). Descriptor addresses
are guest-physical; ports run with the MMU off or identity-mapped, the
same assumption the ZBC spec already makes for physical devices.

**Little-endian discipline.** Virtio 1.x structures and all 9P fields are
little-endian by specification. Big-endian guests byteswap through shared
helpers, exactly as the RIFF codec already does for chunk headers (and
conveniently, the big-endian poster child m68k is a trap-semihosting
target under QEMU, so it rarely needs the virtio path at all).

Estimated size: 300-500 lines of C90 for probe + virtqueue engine.

Transport: virtio-console
-------------------------

Device ID 3. Without MULTIPORT, the device exposes queue 0 (receive) and
queue 1 (transmit) for a single port wired to the QEMU chardev.

.. list-table::
   :header-rows: 1
   :widths: 25 75

   * - Opcode
     - Mapping
   * - SYS_WRITEC
     - One byte into the transmit queue
   * - SYS_WRITE0
     - String bytes into the transmit queue
   * - SYS_WRITE (fd 1, 2)
     - Buffer into the transmit queue; returns 0 (all written)
   * - SYS_READC
     - Poll the receive queue for one byte (blocking)
   * - SYS_READ (fd 0)
     - Drain up to ``count`` bytes from the receive queue
   * - SYS_ISTTY
     - 1 for fds 0-2, 0 otherwise

Estimated size: 100-200 lines over the virtio core.

A per-board polled UART thunk remains possible as a size optimization for
a specific platform port, but it is explicitly **not** the architecture:
there is no universal "the UART" across QEMU's machines (16550, PL011,
SiFive, CMSDK, ESCC, SCIF, ...), and per-board UART drivers are exactly
the burden ZBC exists to remove. Since the virtqueue core must exist for
9p anyway, virtio-console's marginal cost is small and it works on any
virtio-capable machine.

Transport: virtio-9p
--------------------

Device ID 9. QEMU serves 9P2000.L over a single virtqueue (queue 0); each
request occupies one descriptor chain (driver-writable reply buffer chained
after the device-readable request buffer).

Host-side setup is one flag on a stock QEMU::

   -fsdev local,id=fs0,path=$SHARE_DIR,security_model=none \
   -device virtio-9p-device,fsdev=fs0,mount_tag=zbc

(``virtio-9p-device`` is the transport-agnostic/MMIO variant;
``virtio-9p-pci`` is the PCI variant used on PCI-only machines.)

Session and Fid Lifecycle
^^^^^^^^^^^^^^^^^^^^^^^^^

At transport initialization:

1. ``Tversion(msize, "9P2000.L")`` -- negotiate the maximum message size.
   The guest offers its static buffer size (default 8 KiB) and accepts the
   server's (possibly smaller) reply. Reads and writes are clamped to
   ``msize - 24`` (the Rread header) per round trip; larger SYS_READ /
   SYS_WRITE calls loop internally.
2. ``Tattach(root_fid, afid=NOFID, uname="zbc", aname=mount_tag)`` --
   obtain the root fid.

Per open file the guest holds one fid, cloned from the root by ``Twalk``
(up to 16 path components per message, per the protocol's MAXWELEM; deeper
paths chain walks). The guest fd table maps each fd to ``{fid, offset,
open_flags}``; fds 0-2 are reserved for the console transport, user files
start at 3. Because 9p reads and writes carry explicit offsets, SYS_SEEK
is pure fd-table state and never touches the wire.

The fd table is a **private detail of the 9p transport**, not shared seam
infrastructure: 9p is the only transport with client-side file
descriptors (the RIFF and trap transports delegate fd ownership to the
host). The equivalence suite verifies that fd semantics nevertheless
match across transports.

Errno mapping is direct: every 9P2000.L failure is an ``Rlerror`` carrying
a Linux errno, which flows into ``response->error_code`` unchanged.

Normative Opcode Mapping
^^^^^^^^^^^^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1
   :widths: 20 35 45

   * - Opcode
     - 9P messages
     - Notes
   * - SYS_OPEN
     - ``Twalk`` (110) + ``Tlopen`` (12) or ``Tlcreate`` (14)
     - Create/truncate modes use Tlcreate on the parent or
       Tlopen+O_TRUNC; see flag table below
   * - SYS_CLOSE
     - ``Tclunk`` (120)
     - Releases the fid; fd-table slot freed
   * - SYS_READ
     - ``Tread`` (116)
     - Offset from fd table; advances it; loops over msize
   * - SYS_WRITE
     - ``Twrite`` (118)
     - Offset from fd table; advances it; loops over msize
   * - SYS_SEEK
     - *(none)*
     - Sets the fd-table offset
   * - SYS_FLEN
     - ``Tgetattr`` (24)
     - Returns the size field
   * - SYS_REMOVE
     - ``Twalk`` + ``Tremove`` (122)
     - Tremove clunks the fid even on error
   * - SYS_RENAME
     - ``Twalk`` (to both parent dirs) + ``Trenameat`` (74)
     - Old and new parent fids, leaf names
   * - SYS_TMPNAM
     - *(none)*
     - Name generated guest-side, as the host backends do
   * - SYS_ISTTY
     - *(none)*
     - 0 for fds >= 3
   * - SYS_ISERROR, SYS_ERRNO
     - *(none)*
     - Guest-side state

SYS_OPEN Mode Mapping
^^^^^^^^^^^^^^^^^^^^^

``Tlopen``/``Tlcreate`` take Linux ``open(2)`` flags. Binary variants map
identically to their text counterparts (the distinction is meaningless on
POSIX hosts, matching the host backends' behavior).

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - SH_OPEN mode
     - Linux flags
   * - R / RB
     - ``O_RDONLY``
   * - R+ / R+B
     - ``O_RDWR``
   * - W / WB
     - ``O_WRONLY | O_CREAT | O_TRUNC``
   * - W+ / W+B
     - ``O_RDWR | O_CREAT | O_TRUNC``
   * - A / AB
     - ``O_WRONLY | O_CREAT | O_APPEND``
   * - A+ / A+B
     - ``O_RDWR | O_CREAT | O_APPEND``

Relationship to the Linux Extensions Proposal
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The syscalls proposed in :doc:`linux-extensions-proposal` (stat, mkdir,
readdir, fsync, ...) have direct 9P2000.L messages (``Tgetattr``,
``Tmkdir``, ``Treaddir``, ``Tfsync``), so if those opcodes are adopted the
9p transport implements them with no protocol invention. The opcode table
and this mapping should grow together.

Estimated size: 600-1000 lines of C90 over the virtio core.

Operations With No Stock-QEMU Device
------------------------------------

Some opcodes have no universal device behind them. These route to small
per-platform hooks with safe defaults:

.. list-table::
   :header-rows: 1
   :widths: 25 40 35

   * - Opcode
     - Stock-QEMU options
     - Default if no hook
   * - SYS_EXIT / SYS_EXIT_EXTENDED
     - ``isa-debug-exit`` (x86), ``sifive_test`` (RISC-V ``virt``),
       PSCI ``SYSTEM_OFF`` (ARM ``virt``)
     - -1 / ``ENOSYS`` (returns to the caller; the library never hangs)
   * - SYS_CLOCK, SYS_ELAPSED, SYS_TICKFREQ
     - Cycle counter / CLINT ``mtime`` / TSC, per platform
     - -1 / ``ENOSYS``
   * - SYS_TIME
     - No portable wall clock without an RTC driver
     - -1 / ``ENOSYS``
   * - SYS_SYSTEM, SYS_GET_CMDLINE
     - None
     - -1 / ``ENOSYS``
   * - SYS_HEAPINFO
     - Linker-script symbols
     - -1 / ``ENOSYS``
   * - SYS_TIMER_CONFIG
     - Platform timer + IRQ, out of scope initially
     - -1 / ``ENOSYS``

Where the trap transport is in use, all of these are served by QEMU's
semihosting implementation and no hooks are needed.

Impact on Existing Code
-----------------------

The Wire Protocol and the Spec
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Unchanged. :doc:`specification` governs the ZBC device transport exactly
as before. This proposal adds no chunks, registers, or opcodes.

The C Client Library
^^^^^^^^^^^^^^^^^^^^

- ``zbc_client.c``: extract the RIFF build/submit/parse path into the
  default transport; ``zbc_call()`` dispatches through the vtable.
- New sources (built only into ports that want them):
  ``zbc_transport_virtio.c`` (core), ``zbc_transport_vcon.c``,
  ``zbc_transport_9p.c``, ``zbc_transport_trap_<arch>.S``.
- New shared header for transport-facing constants (virtio register
  offsets, 9P message types, flag mappings) so a future second
  implementation cannot drift -- the same single-definition discipline
  ``zbc_protocol.h`` provides today and the C++ tier's ``Protocol.h``
  consumes.

The C Host Library and the C++ Host Library
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Untouched. The C++ tier is host-side only (``zbc::Device`` embeds in
MAME-class emulators); since the wire does not change, the conformance
suite's byte-for-byte C-vs-C++ equivalence is unaffected. The planned
QEMU ZBC-device proof of concept is a separate effort that would embed the
**C** host library (QEMU is a C codebase) through ``zbc_host_mem_ops_t``,
exactly as :doc:`emulator-integration` describes; it serves as the escape
hatch for guests that need full semihosting semantics on QEMU before --
or instead of -- the virtio transports.

Testing Strategy
----------------

**Unit tests (host-built, no emulator).** A mock virtio-mmio device over
the existing ``test/common`` mock-memory infrastructure exercises probe,
feature negotiation, and the virtqueue engine; a scripted 9p server
validates message encoding, fid lifecycle, msize clamping, and Rlerror
handling.

**Transport equivalence.** The same operation script runs against the
RIFF transport (mock ZBC device) and the 9p/console transports (mock
virtio device); results, errnos, and fd behavior must match. This is the
client-side analogue of the existing C-vs-C++ host conformance suite.

**On-target.** ``test/target/runners/qemu.cmake`` stops being a stub. The
existing ``zbc_target_test.c`` runs unmodified::

   # RISC-V virt, virtio transports
   qemu-system-riscv32 -machine virt -nographic \
     -bios none -kernel zbc_target_test.elf \
     -fsdev local,id=fs0,path=$SANDBOX,security_model=none \
     -device virtio-9p-device,fsdev=fs0,mount_tag=zbc \
     -device virtio-serial-device \
     -device virtconsole,chardev=c0 -chardev stdio,id=c0

   # ARM virt, trap transport build
   qemu-system-arm -machine virt -nographic -semihosting \
     -kernel zbc_target_test_trap.elf

The MAME 6502/i386 runs continue unchanged, demonstrating the thesis:
one test program, one client API, three transports, two emulators.

Implementation Plan
-------------------

.. list-table::
   :header-rows: 1
   :widths: 30 50 20

   * - Step
     - Deliverable
     - Risk
   * - 1. Transport seam
     - Vtable in ``zbc_client_state_t``; RIFF path behind it;
       all existing tests pass bit-for-bit
     - Low
   * - 2. Trap transport
     - ARM or RISC-V thunk + QEMU trap CI job
     - Low
   * - 3. virtio core
     - Probe + polled virtqueue + mock-device unit tests
     - Medium
   * - 4. virtio-console
     - Console opcodes on stock QEMU
     - Low
   * - 5. 9p transport
     - File opcodes on stock QEMU; equivalence suite
     - Medium
   * - 6. QEMU runner
     - ``qemu.cmake`` implemented; on-target CI
     - Low

Step 2 is deliberately early: it proves the seam end-to-end on stock QEMU
with trivial transport code before the virtio investment.

Design Decisions
----------------

Questions raised by the initial draft, settled during review, and
recorded here with rationale.

1. **Probe failure never hangs.** If no transport is found, the null
   transport is selected and every call fails immediately with -1 /
   ``ENOSYS``. No spin loops, no weak panic hooks: the library either
   works or fails deterministically.
2. **The fd table belongs to the 9p transport.** A shared fd layer in
   the seam would be infrastructure only one transport uses; instead the
   9p transport owns its table privately, and the equivalence suite
   proves fd semantics match across transports.
3. **Trap transport is build-time, permanently.** Runtime trap probing
   would require per-architecture fault shims -- exactly the
   N-platforms-of-specialized-code burden this design exists to avoid.
4. **No virtio-pci, ever.** ``microvm`` is the supported x86 machine,
   indefinitely. PCI enumeration is rejected as platform-specific
   surface area.
5. **Transport override is just the public vtable field.** Assign
   ``state->transport`` before the first call to bypass the probe; no
   dedicated selection API. Users have the code and may break it as
   they please.
6. **Partial-transfer accounting is pinned early.** The equivalence
   suite covers EOF-mid-loop reads and short writes from the first 9p
   milestone, so looped ``Tread``/``Twrite`` accounting cannot drift
   from host-backend behavior.

Revision History
----------------

.. list-table::
   :header-rows: 1
   :widths: 20 80

   * - Date
     - Changes
   * - 2026-06
     - Initial proposal: transport seam, probe chain, trap/virtio-console/
       virtio-9p transports for stock QEMU
   * - 2026-06
     - Open questions resolved: null transport on probe failure (never
       hang), 9p-private fd table, trap transport build-time permanently,
       microvm-only on x86 (no virtio-pci), override via public vtable
       field
