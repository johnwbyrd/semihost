Examples
========

MAME 6502 Integration
---------------------

`examples/mame/` demonstrates ZBC semihosting on 6502 in MAME.

Files:
- **hello.c**: Prints "Hello, ZBC!" via semihosting.
- **crt6502.c**: Custom CRT0 with zbc_semihost() integration for picolibc.
- **zbc6502.ld**: Linker script mapping device at 0xFFFF0000.
- **Makefile**: Build + MAME softlist.

Build/Run:

.. code-block:: bash

   cd examples/mame
   make
   mame zbc6502 -window -nomax -natural

Expected: "Hello, ZBC!" on host console.

Customization
-------------

- **Device Address**: Change in crt6502.c/zbc6502.ld.
- **Backend**: Edit emulator host_init() for secure sandbox.
- **CPU**: Works 8/16/32/64-bit; adjust CNFG int_size/ptr_size.

See MAME ZBC driver for full machine emulation.
