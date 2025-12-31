API Reference
=============

This section provides detailed API documentation for all public functions
and types in the ZBC Semihosting library.

Overview
--------

The library is organized into five main components:

**High-Level API** (``zbc_api.h``)
   Type-safe wrapper functions for guest code that wants a POSIX-like
   interface. Bundles client state and buffer management into a single
   ``zbc_api_t`` context.

**Client API** (``zbc_client.h``)
   Lower-level functions for guest/embedded code. Use this when
   implementing libc integration (``sys_semihost()``), or when you need
   direct control over the RIFF protocol.

**Host API** (``zbc_host.h``)
   Functions for processing semihosting requests from guest code.
   Use this when implementing a semihosting device in an emulator.

**Backend API** (``zbc_backend.h``)
   Backend vtable and factory functions. Backends provide the
   actual file I/O, console, and time implementations.

**Protocol API** (``zbc_protocol.h``)
   Low-level RIFF chunk parsing, constants, and helper functions.
   Most users won't need these directly.

Design Principles
-----------------

The library follows these design principles:

- **C90 compliant** -- works with any C compiler
- **Zero allocation** -- caller provides all buffers
- **Explicit error handling** -- all errors via return values
- **Architecture agnostic** -- works from 8-bit to 64-bit systems
- **Output parameters first** -- consistent parameter ordering

Error Codes
-----------

All functions return ``ZBC_OK`` (0) on success or a negative error code:

.. code-block:: c

   #define ZBC_OK                     0
   #define ZBC_ERR_NULL_ARG          (-1)
   #define ZBC_ERR_HEADER_OVERFLOW   (-2)
   #define ZBC_ERR_DATA_OVERFLOW     (-3)
   #define ZBC_ERR_BAD_RIFF_MAGIC    (-4)
   #define ZBC_ERR_BAD_FORM_TYPE     (-5)
   #define ZBC_ERR_RIFF_OVERFLOW     (-6)
   #define ZBC_ERR_NOT_FOUND         (-7)
   #define ZBC_ERR_BUFFER_FULL       (-8)
   #define ZBC_ERR_UNKNOWN_OPCODE    (-9)
   #define ZBC_ERR_NOT_INITIALIZED   (-10)
   #define ZBC_ERR_DEVICE_ERROR      (-11)
   #define ZBC_ERR_TIMEOUT           (-12)
   #define ZBC_ERR_INVALID_ARG       (-13)
   #define ZBC_ERR_PARSE_ERROR       (-14)

Check for errors with ``if (rc < 0)``.


.. toctree::
   :maxdepth: 2
   :caption: API Subsections

   high-level
   client
   host
   backend
   protocol
