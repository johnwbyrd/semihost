Client Library
==============

The ZBC semihosting client library gives you file I/O, console access, and
time services from within your guest, without needing to write device drivers
for your particular real or virtual hardware.  It's the quickest way to bring
up code for your new architecture or emulator.

When running in a virtual machine or emulator, semihosting gains direct access
to the host filesystem and console. You can read test inputs, write output
files, and log messages to the host during development, without bringing up
those services on the emulated device itself.

Naturally, with great power comes great responsibility.  By design, semihosting
allows guest programs to read and write arbitrary files on the host system.
The semihosting device attempts to provide you some isolation features via
seccomp filters and directory jails on Linux; however, it's up to you implement
these features correctly for your use case.


What You Get
------------

- **File operations**: open, close, read, write, seek, get length, remove, rename
- **Console I/O**: read/write characters and strings
- **Time services**: wall clock time, elapsed ticks, tick frequency
- **System**: exit, get command line, get heap info

Quick Start
-----------

If you want a POSIX-like interface, use the high-level API:

.. code-block:: c

   #include "zbc_api.h"

   static zbc_client_state_t client;
   static zbc_api_t api;
   static uint8_t buf[512];

   void init(void) {
       zbc_client_init(&client, (void *)0xFFFF0000);
       zbc_api_init(&api, &client, buf, sizeof(buf));
   }

   void example(void) {
       /* Write to console */
       zbc_api_write0(&api, "Hello from semihosting!\n");

       /* File I/O */
       int fd = zbc_api_open(&api, "/tmp/test.txt", SH_OPEN_W);
       if (fd >= 0) {
           zbc_api_write(&api, fd, "Hello\n", 6);
           zbc_api_close(&api, fd);
       }

       /* Get time */
       int seconds = zbc_api_time(&api);
   }

See :doc:`api/high-level` for the complete high-level API reference.

Choosing an API
---------------

**Use the High-Level API** (``zbc_api.h``) when:

- You want POSIX-like function calls (``open``, ``read``, ``write``, etc.)
- You don't need to inspect the raw RIFF protocol

**Use the Low-Level API** (``zbc_call()``) when:

- You're implementing libc integration (``sys_semihost()``)
- You need direct control over the RIFF buffer
- You're building your own abstraction layer

The rest of this document covers the low-level API.

Low-Level Setup
---------------

Include the header and declare your state:

.. code-block:: c

   #include "zbc_client.h"

   static zbc_client_state_t client;
   static uint8_t riff_buf[512];

Initialize with the device base address for your particular architecture:

.. code-block:: c

   zbc_client_init(&client, (void *)0xFFFF0000);

Optionally verify the device exists:

.. code-block:: c

   if (!zbc_client_check_signature(&client)) {
       /* No semihosting device at this address */
   }

Making Calls
------------

All semihosting calls go through ``zbc_call()``:

.. code-block:: c

   int zbc_call(zbc_response_t *response, zbc_client_state_t *state,
                void *buf, size_t buf_size, int opcode, uintptr_t *args);

- ``response`` - receives parsed response
- ``state`` - initialized client state
- ``buf`` - working buffer for RIFF protocol (you provide this)
- ``buf_size`` - size of buffer
- ``opcode`` - syscall number (``SH_SYS_*`` constants from ``zbc_protocol.h``)
- ``args`` - array of arguments, layout depends on opcode

Returns ``ZBC_OK`` on success, or ``ZBC_ERR_*`` on protocol/transport error.
On success, ``response->result`` contains the syscall return value, and
``response->error_code`` contains the host errno.

Console Output
^^^^^^^^^^^^^^

Write a string to console (SYS_WRITE0):

.. code-block:: c

   zbc_response_t response;
   uintptr_t args[1];
   args[0] = (uintptr_t)"Hello, world!\n";
   zbc_call(&response, &client, riff_buf, sizeof(riff_buf), SH_SYS_WRITE0, args);

Opening a File
^^^^^^^^^^^^^^

SYS_OPEN takes path pointer, mode, and path length:

.. code-block:: c

   const char *path = "/tmp/test.txt";
   zbc_response_t response;
   uintptr_t args[3];
   args[0] = (uintptr_t)path;
   args[1] = SH_OPEN_W;  /* write mode */
   args[2] = strlen(path);

   int rc = zbc_call(&response, &client, riff_buf, sizeof(riff_buf),
                     SH_SYS_OPEN, args);
   if (rc != ZBC_OK || response.result < 0) {
       /* open failed */
   }
   int fd = response.result;

Writing to a File
^^^^^^^^^^^^^^^^^

SYS_WRITE takes fd, buffer pointer, and count. Returns bytes NOT written
(0 = success):

.. code-block:: c

   const char *data = "Hello\n";
   size_t len = 6;
   zbc_response_t response;
   uintptr_t args[3];
   args[0] = fd;
   args[1] = (uintptr_t)data;
   args[2] = len;

   zbc_call(&response, &client, riff_buf, sizeof(riff_buf), SH_SYS_WRITE, args);
   int not_written = response.result;

Reading from a File
^^^^^^^^^^^^^^^^^^^

SYS_READ takes fd, buffer pointer, and count. Returns bytes NOT read:

.. code-block:: c

   char my_read_buf[256];
   zbc_response_t response;
   uintptr_t args[3] = { fd, 0 /* unused */, sizeof(my_read_buf) };

   zbc_call(&response, &client, riff_buf, sizeof(riff_buf), SH_SYS_READ, args);
   size_t bytes_read = args[2] - (size_t)response.result;
   memcpy(my_read_buf, response.data, bytes_read);

Closing a File
^^^^^^^^^^^^^^

.. code-block:: c

   zbc_response_t response;
   uintptr_t args[1];
   args[0] = fd;
   zbc_call(&response, &client, riff_buf, sizeof(riff_buf), SH_SYS_CLOSE, args);

Getting the Time
^^^^^^^^^^^^^^^^

SYS_TIME returns seconds since Unix epoch:

.. code-block:: c

   zbc_response_t response;
   zbc_call(&response, &client, riff_buf, sizeof(riff_buf), SH_SYS_TIME, NULL);
   uintptr_t seconds = response.result;

Syscall Reference
-----------------

Each syscall has a specific args array layout. The opcode constants are
defined in ``zbc_protocol.h``.

.. list-table::
   :header-rows: 1
   :widths: 10 20 40 30

   * - Opcode
     - Name
     - Args
     - Returns
   * - 0x01
     - SH_SYS_OPEN
     - [0]=path, [1]=mode, [2]=len
     - fd or -1
   * - 0x02
     - SH_SYS_CLOSE
     - [0]=fd
     - 0 or -1
   * - 0x03
     - SH_SYS_WRITEC
     - [0]=char_ptr
     - (void)
   * - 0x04
     - SH_SYS_WRITE0
     - [0]=string_ptr
     - (void)
   * - 0x05
     - SH_SYS_WRITE
     - [0]=fd, [1]=buf, [2]=count
     - bytes NOT written
   * - 0x06
     - SH_SYS_READ
     - [0]=fd, [1]=buf, [2]=count
     - bytes NOT read
   * - 0x07
     - SH_SYS_READC
     - (none)
     - char or -1
   * - 0x08
     - SH_SYS_ISERROR
     - [0]=status
     - 1 if error, 0 otherwise
   * - 0x09
     - SH_SYS_ISTTY
     - [0]=fd
     - 1 if tty, 0 otherwise
   * - 0x0A
     - SH_SYS_SEEK
     - [0]=fd, [1]=pos
     - 0 or -1
   * - 0x0C
     - SH_SYS_FLEN
     - [0]=fd
     - length or -1
   * - 0x0D
     - SH_SYS_TMPNAM
     - [0]=buf, [1]=id, [2]=maxlen
     - 0 or -1, fills buf
   * - 0x0E
     - SH_SYS_REMOVE
     - [0]=path, [1]=len
     - 0 or -1
   * - 0x0F
     - SH_SYS_RENAME
     - [0]=old, [1]=old_len, [2]=new, [3]=new_len
     - 0 or -1
   * - 0x10
     - SH_SYS_CLOCK
     - (none)
     - centiseconds since start
   * - 0x11
     - SH_SYS_TIME
     - (none)
     - seconds since epoch
   * - 0x12
     - SH_SYS_SYSTEM
     - [0]=cmd, [1]=len
     - exit code
   * - 0x13
     - SH_SYS_ERRNO
     - (none)
     - last errno
   * - 0x15
     - SH_SYS_GET_CMDLINE
     - [0]=buf, [1]=size
     - 0 or -1, fills buf
   * - 0x16
     - SH_SYS_HEAPINFO
     - [0]=block_ptr
     - 0, fills 4 values
   * - 0x18
     - SH_SYS_EXIT
     - [0]=reason, [1]=subcode
     - (no return)
   * - 0x30
     - SH_SYS_ELAPSED
     - [0]=tick_ptr
     - 0, fills 8 bytes
   * - 0x31
     - SH_SYS_TICKFREQ
     - (none)
     - ticks per second

Open Mode Flags
^^^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1
   :widths: 15 20 65

   * - Value
     - Name
     - Description
   * - 0
     - SH_OPEN_R
     - Read only
   * - 1
     - SH_OPEN_RB
     - Read only, binary
   * - 4
     - SH_OPEN_W
     - Write, truncate/create
   * - 5
     - SH_OPEN_WB
     - Write binary, truncate/create
   * - 8
     - SH_OPEN_A
     - Append, create if needed

See ``zbc_protocol.h`` for the full list (modes 0-11 corresponding to
fopen modes).

Buffer Management
-----------------

The library never allocates memory. You provide the RIFF buffer for each call.

**Sizing:**

- 256 bytes handles most syscalls
- 512 bytes is comfortable for file operations
- Match your largest read/write size plus ~64 bytes overhead

The buffer is reused for both request and response. After ``zbc_call()``
returns, the buffer contains the response data. For syscalls that return
data (like SYS_READ), ``response->data`` points into this buffer and
``response->data_size`` gives its length. Copy the data before making
another call.

See Also
--------

- :doc:`specification` -- wire format details
- ``include/zbc_client.h`` -- API declarations
- ``include/zbc_protocol.h`` -- opcodes and constants
