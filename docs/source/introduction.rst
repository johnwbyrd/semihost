============
Introduction
============

Zero Board Computer (ZBC) exists because embedded bring-up has a
chicken-and-egg problem, and you should not have to keep solving
it for every new project.

This document lays out the problem ZBC addresses, who it is for,
and how it works. If you already know you want to use it, you can
skip ahead to :doc:`client-library` (to call ZBC from your
firmware) or :doc:`emulator-integration` (to add ZBC to your
simulator).

.. contents:: Table of Contents
   :local:
   :depth: 2

The chicken-and-egg problem
---------------------------

You can't debug your firmware until the serial port works. You
can't test your filesystem until the flash driver works. You can't
validate your timer until you've built a way to read the clock
back out. The first six weeks of every new-hardware project go to
fighting your way to a working printf -- not building the product,
not testing the silicon, not exercising the design, just trying to
see what your software is doing.

This is true of every new chip, every new board, every new
microcontroller, every prototype. It has been true for decades.
The work doesn't get easier; it just gets done over and over
again, by every team, on every project, before any of them can
start on the work that actually matters.

What Zero Board Computer does
-----------------------------

ZBC eliminates the chicken-and-egg. Any CPU -- real, emulated, or
yet to be designed -- gets working file input/output, console, and
clock services through a 32-byte memory-mapped device. Day one.
Before any hardware exists.

The same firmware that runs against your simulator runs against a
field-programmable gate array (FPGA) prototype, against the
manufactured chip, and against a forty-year-old machine inside the
Multiple Arcade Machine Emulator (MAME), because the contract is
the same everywhere: read and write memory.

What you get on the guest side:

- **File operations:** open, close, read, write, seek, length,
  remove, rename
- **Console:** read and write characters and strings
- **Time:** wall clock, elapsed ticks, tick frequency
- **System:** exit, command line, heap information
- **A periodic timer interrupt,** if your CPU has interrupts at all

What you have to write to get all of that: nothing. Link the
client library, point it at the device's base address, and call
the application programming interface (API). The library is
written in C90, never allocates memory, and is small enough to
fit on an 8-bit microcontroller with a few kilobytes of RAM.

Why is this new?
----------------

The general trick of borrowing services from a host machine has a
name: *semihosting*. ARM defined a flavor of it in the early
1990s. It works, but only on ARM chips, only with a hardware
debugging probe physically attached to the board, and only via
ARM-specific trap instructions that have to be intercepted by the
debugger.

That was a perfectly reasonable design for 1993. It is a less
reasonable design today, when every CPU family has its own trap
mechanism, when prototypes increasingly run in software simulators
long before any silicon exists, and when the hobbyist building
something on perfboard does not own a hardware debug probe.

ZBC rebuilds the same idea around plain load and store operations.
There are no special instructions to define. There is no debug
probe to attach. There is no privileged execution mode required.
If the CPU can read and write memory -- and every CPU can read and
write memory, by definition -- ZBC works on it.

Is this for you?
----------------

You are bringing up a new board
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

You are dreading the first month. You don't have to write the
serial-port driver. You don't have to write the flash driver. You
don't have to build the throwaway logging trick you always end up
building. Add 32 bytes of memory-mapped decode to your bus, link
the host library into your emulator (or instrument the real bus on
the prototype), and your firmware has printf, fopen, time(), and
system() before the board comes back from the factory.

The license is MIT -- about as permissive as software licenses
come. The client library is C90 and small enough to fit on the
tiniest microcontrollers. It never calls malloc. You don't sign
anything, and you don't depend on any vendor's software
development kit. The agreement between your firmware and the
device is a single specification file in this repository, and the
libraries shipped here are reference implementations of that
specification -- not the other way around.

You are a hobbyist with an old chip
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

You just soldered a 6502, a Z80, or a 68000 onto perfboard and you
want it to do something real. ZBC gives it printf and a real
filesystem this afternoon.

MAME, the open-source recreation of historical computing hardware,
already ships ZBC machines for over two hundred vintage systems:
home microcomputers, arcade boards, classic minicomputers, chips
you've only ever read about. Compile your C with a cross-compiler
that targets your chip, drop the resulting executable into the
emulated machine, and your 1979 hardware is reading configuration
files off your laptop. No serial-port driver. No storage-card
driver. No filesystem code. No driver work at all.

When the physical board you built is ready to run the same code,
you point the client library at the device address you wired in,
and the same binary just runs.

You are greenlighting a new chip program
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Your schedule has firmware blocked on silicon for the first
quarter. It doesn't have to be. The surface area of ZBC is 32
bytes of memory-mapped input/output -- small enough that the
simulator and the manufactured chip cannot meaningfully diverge.

Firmware development starts on day one against the simulator,
with full file access and a real console. Silicon arrival becomes
an integration milestone, not a starting gun. The same firmware,
the same tests, the same logs, before and after tape-out. A
quarter of calendar time recovered. Nothing proprietary
introduced.

How it works
------------

ZBC trades trap instructions for a 32-byte memory-mapped device.

The guest writes a small message into a buffer in its own RAM,
using a tagged container format called the Resource Interchange
File Format (RIFF) -- the same format used by WAV and AVI files.
The guest writes the buffer's address into the device's
``RIFF_PTR`` register, then writes any value to the ``DOORBELL``
register to signal the host. The host reads the buffer, executes
the requested operation, writes the response back into the same
buffer, and sets a status bit. The guest reads the response.

Three properties fall out of this design:

**Architecture-agnostic by construction.** The guest declares its
integer size, pointer size, and byte order in the first message
it ever sends. The host honors what the guest says. An 8-bit chip
that reads in little-endian and a 64-bit chip that reads in
big-endian speak the same protocol; they just fill in different
sizes for the same fields. Live on-target tests for both ship in
this repository.

**No debugger required.** The host is not a debugger interrupting
the CPU. The host is a peripheral on the bus, like a serial port
or a real-time clock chip. ZBC works in a stock software
simulator with no debugging infrastructure attached, and it works
on real hardware with no probe attached.

**ARM-compatible operation numbering.** The operation numbers
(open, close, read, write, exit, and so on) match ARM's
semihosting numbering, so existing C standard library
implementations like picolibc and newlib that already speak ARM
semihosting work nearly unchanged. Decades of library effort
transfers in without rewriting.

The wire format is the contract
-------------------------------

The single source of truth for what ZBC is, byte for byte, is
:doc:`specification` in this repository. The C and C++ host
libraries shipped here are *implementations* of that contract,
and the conformance test suite in
``test/conformance/test_conformance.cpp`` enforces byte-for-byte
equivalence between them on every continuous integration run.

This matters because future implementations -- Rust bindings, an
FPGA core, a clean-room reimplementation in a vendor's emulator
-- all conform to the same wire format. Semihosting stops being a
per-platform reinvention. A guest binary written today against
this specification will still run, twenty years from now, against
any conforming host that exists then.

Where to go next
----------------

- :doc:`client-library` -- how to call ZBC from your firmware
- :doc:`emulator-integration` -- how to add ZBC to your simulator
- :doc:`specification` -- the wire format, definitive
- :doc:`security` -- sandbox backends and process-level
  restrictions for untrusted guests
- :doc:`testing` -- the test suite, the conformance check, and
  on-target testing
- :doc:`building` -- build instructions for the libraries and
  tests
- :doc:`examples` -- minimal client examples for several
  architectures
