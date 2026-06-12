================
Linux Extensions
================

This document describes the Linux-extension opcodes that ZBC adds to
the ARM-semihosting syscall set. These opcodes let a Linux VFS driver
expose host files directly to a guest Linux kernel, support block
device access via disk image files, and back a TTY driver with
non-blocking console input.

The opcodes are defined in :doc:`api/protocol` (header
``zbc_protocol.h``), dispatched in the C host
(:doc:`api/host`), and implemented in the ANSI backends
(:doc:`api/backend`).

.. contents:: Table of Contents
   :local:
   :depth: 2

Background
----------

The ARM-compatible syscalls (opcodes 0x01-0x32) provide basic file
I/O, console, and timekeeping services -- sufficient for bare-metal
applications and simple embedded systems. To support a Linux
``semihostfs`` VFS driver, additional operations are required.

The extension opcodes start at **0x80** to avoid collision with the
ARM range. Each one wraps a POSIX function directly on the host side.

Networking is explicitly out of scope for ZBC semihosting.

Syscalls
--------

Directory Operations
^^^^^^^^^^^^^^^^^^^^

SYS_OPENDIR (0x80)
""""""""""""""""""

Open a directory for enumeration.

**Arguments:**

.. list-table::
   :header-rows: 1
   :widths: 10 40 50

   * - Slot
     - Type
     - Description
   * - [0]
     - pointer
     - Path to directory (DATA chunk)
   * - [1]
     - integer
     - Path length

**Returns:**

- ``>= 0``: Directory handle
- ``-1``: Error (errno set)

**Host implementation:** Wraps POSIX ``opendir()``.

SYS_READDIR (0x81)
""""""""""""""""""

Read one directory entry.

**Arguments:**

.. list-table::
   :header-rows: 1
   :widths: 10 40 50

   * - Slot
     - Type
     - Description
   * - [0]
     - integer
     - Directory handle (from SYS_OPENDIR)
   * - [1]
     - pointer
     - Output buffer (DATA chunk destination)
   * - [2]
     - integer
     - Buffer size

**Returns:**

- ``> 0``: Bytes written to buffer (one entry)
- ``0``: End of directory
- ``-1``: Error (errno set)

**Output buffer format:**

::

   d_ino[8]    - Inode number (little-endian)
   d_type[1]   - File type (DT_REG, DT_DIR, DT_LNK, etc.)
   d_namlen[1] - Name length (not including null terminator)
   d_name[...] - Null-terminated filename

**Host implementation:** Wraps POSIX ``readdir()``. One entry per call;
guest loops until return value is 0.

SYS_CLOSEDIR (0x82)
"""""""""""""""""""

Close a directory handle.

**Arguments:**

.. list-table::
   :header-rows: 1
   :widths: 10 40 50

   * - Slot
     - Type
     - Description
   * - [0]
     - integer
     - Directory handle

**Returns:**

- ``0``: Success
- ``-1``: Error (errno set)

**Host implementation:** Wraps POSIX ``closedir()``.

File Metadata
^^^^^^^^^^^^^

SYS_STAT (0x83)
"""""""""""""""

Get file metadata by path.

**Arguments:**

.. list-table::
   :header-rows: 1
   :widths: 10 40 50

   * - Slot
     - Type
     - Description
   * - [0]
     - pointer
     - Path to file (DATA chunk)
   * - [1]
     - integer
     - Path length
   * - [2]
     - pointer
     - Output buffer (DATA chunk destination, 48 bytes)
   * - [3]
     - integer
     - Buffer size (must be 48)

**Returns:**

- ``0``: Success
- ``-1``: Error (ENOENT, EACCES, etc.)

**Output buffer format (48 bytes, all little-endian):**

::

   ino[8]   - Inode number
   mode[4]  - File type and permissions (S_IFREG, S_IFDIR, etc.)
   nlink[4] - Number of hard links
   size[8]  - File size in bytes
   mtime[8] - Modification time (seconds since epoch)
   atime[8] - Access time (seconds since epoch)
   ctime[8] - Change time (seconds since epoch)

**Host implementation:** Wraps POSIX ``stat()``. Returns real host
permissions (the guest needs accurate permission info for the VFS
driver).

**Why 48 bytes:** The buffer contains all fields required by Linux VFS
``struct kstat`` (inode, nlink, mode, size, timestamps). Per the
`Linux VFS documentation
<https://www.kernel.org/doc/html/next/filesystems/vfs.html>`_,
``getattr`` must populate these fields.

SYS_FSTAT (0x84)
""""""""""""""""

Get file metadata by descriptor.

**Arguments:**

.. list-table::
   :header-rows: 1
   :widths: 10 40 50

   * - Slot
     - Type
     - Description
   * - [0]
     - integer
     - File descriptor
   * - [1]
     - pointer
     - Output buffer (DATA chunk destination, 48 bytes)
   * - [2]
     - integer
     - Buffer size (must be 48)

**Returns:**

- ``0``: Success
- ``-1``: Error

**Host implementation:** Wraps POSIX ``fstat()``. Same output format
as SYS_STAT.

SYS_LSTAT (0x8D)
""""""""""""""""

``stat()`` that does not follow symbolic links. Same arguments and
output format as SYS_STAT. Wraps POSIX ``lstat()``.

File Operations
^^^^^^^^^^^^^^^

SYS_MKDIR (0x85)
""""""""""""""""

Create a directory.

**Arguments:**

.. list-table::
   :header-rows: 1
   :widths: 10 40 50

   * - Slot
     - Type
     - Description
   * - [0]
     - pointer
     - Path (DATA chunk)
   * - [1]
     - integer
     - Path length
   * - [2]
     - integer
     - Mode (passed to host mkdir, may be masked by umask)

**Returns:**

- ``0``: Success
- ``-1``: Error (EEXIST, ENOENT for missing parent, etc.)

**Host implementation:** Wraps POSIX ``mkdir()``.

SYS_RMDIR (0x86)
""""""""""""""""

Remove an empty directory.

**Arguments:**

.. list-table::
   :header-rows: 1
   :widths: 10 40 50

   * - Slot
     - Type
     - Description
   * - [0]
     - pointer
     - Path (DATA chunk)
   * - [1]
     - integer
     - Path length

**Returns:**

- ``0``: Success
- ``-1``: Error (ENOTEMPTY, ENOENT, etc.)

**Host implementation:** Wraps POSIX ``rmdir()``.

SYS_FTRUNCATE (0x87)
""""""""""""""""""""

Truncate an open file to a specified length.

**Arguments:**

.. list-table::
   :header-rows: 1
   :widths: 10 40 50

   * - Slot
     - Type
     - Description
   * - [0]
     - integer
     - File descriptor
   * - [1]
     - pointer
     - Pointer to 8-byte length value (DATA chunk, little-endian)
   * - [2]
     - integer
     - Length size (always 8)

**Returns:**

- ``0``: Success
- ``-1``: Error

**Host implementation:** Wraps POSIX ``ftruncate()``. Length is sent
as an 8-byte little-endian value in a DATA chunk to handle 64-bit
file sizes on all guest architectures.

SYS_FSYNC (0x88)
""""""""""""""""

Flush file data to storage.

**Arguments:**

.. list-table::
   :header-rows: 1
   :widths: 10 40 50

   * - Slot
     - Type
     - Description
   * - [0]
     - integer
     - File descriptor

**Returns:**

- ``0``: Success
- ``-1``: Error

**Host implementation:** Wraps POSIX ``fsync()``.

Console Extension
^^^^^^^^^^^^^^^^^

SYS_READC_POLL (0x89)
"""""""""""""""""""""

Non-blocking console character read.

**Problem:** The existing SYS_READC (0x07) blocks forever waiting for
input. A Linux TTY driver cannot block the kernel; it needs to poll
for input availability.

**Arguments:** None.

**Returns:**

- ``0-255``: Character read
- ``-1``: No character available (not an error, just empty)

**Host implementation:** Uses ``select()`` or ``poll()`` on stdin with
zero timeout, then ``read()`` if data is available.

Symlink Operations
^^^^^^^^^^^^^^^^^^

These are lower priority than the core directory and metadata
operations but are required for complete filesystem support.

.. list-table::
   :header-rows: 1
   :widths: 10 20 25 45

   * - Opcode
     - Syscall
     - POSIX Wrapper
     - Description
   * - 0x8A
     - SYS_LINK
     - ``link()``
     - Create hard link
   * - 0x8B
     - SYS_SYMLINK
     - ``symlink()``
     - Create symbolic link
   * - 0x8C
     - SYS_READLINK
     - ``readlink()``
     - Read symbolic link target

Opcode Summary
^^^^^^^^^^^^^^

**Directory Operations (3):**

.. list-table::
   :header-rows: 1
   :widths: 15 25 30

   * - Opcode
     - Syscall
     - POSIX Wrapper
   * - 0x80
     - SYS_OPENDIR
     - ``opendir()``
   * - 0x81
     - SYS_READDIR
     - ``readdir()``
   * - 0x82
     - SYS_CLOSEDIR
     - ``closedir()``

**File Metadata (3):**

.. list-table::
   :header-rows: 1
   :widths: 15 25 30

   * - Opcode
     - Syscall
     - POSIX Wrapper
   * - 0x83
     - SYS_STAT
     - ``stat()``
   * - 0x84
     - SYS_FSTAT
     - ``fstat()``
   * - 0x8D
     - SYS_LSTAT
     - ``lstat()``

**File Operations (4):**

.. list-table::
   :header-rows: 1
   :widths: 15 25 30

   * - Opcode
     - Syscall
     - POSIX Wrapper
   * - 0x85
     - SYS_MKDIR
     - ``mkdir()``
   * - 0x86
     - SYS_RMDIR
     - ``rmdir()``
   * - 0x87
     - SYS_FTRUNCATE
     - ``ftruncate()``
   * - 0x88
     - SYS_FSYNC
     - ``fsync()``

**Console (1):**

.. list-table::
   :header-rows: 1
   :widths: 15 25 30

   * - Opcode
     - Syscall
     - Implementation
   * - 0x89
     - SYS_READC_POLL
     - ``select()`` + ``read()``

**Symlinks (3):**

.. list-table::
   :header-rows: 1
   :widths: 15 25 30

   * - Opcode
     - Syscall
     - POSIX Wrapper
   * - 0x8A
     - SYS_LINK
     - ``link()``
   * - 0x8B
     - SYS_SYMLINK
     - ``symlink()``
   * - 0x8C
     - SYS_READLINK
     - ``readlink()``

**Total: 14 opcodes in the 0x80-0x8D range.**

Linux Driver Architecture
-------------------------

Filesystem Driver (semihostfs)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

A Linux VFS driver implements these operations against the extension
opcodes:

.. code-block:: c

   static struct file_system_type semihostfs_type = {
       .name     = "semihostfs",
       .mount    = semihostfs_mount,
       .kill_sb  = kill_litter_super,
   };

   static const struct inode_operations semihostfs_dir_iops = {
       .lookup   = semihostfs_lookup,    /* SYS_STAT */
       .mkdir    = semihostfs_mkdir,     /* SYS_MKDIR */
       .rmdir    = semihostfs_rmdir,     /* SYS_RMDIR */
       .create   = semihostfs_create,    /* SYS_OPEN with O_CREAT */
       .unlink   = semihostfs_unlink,    /* SYS_REMOVE */
       .rename   = semihostfs_rename,    /* SYS_RENAME */
   };

   static const struct file_operations semihostfs_dir_fops = {
       .iterate_shared = semihostfs_readdir,  /* SYS_READDIR */
   };

   static const struct file_operations semihostfs_file_fops = {
       .read     = semihostfs_read,      /* SYS_READ */
       .write    = semihostfs_write,     /* SYS_WRITE */
       .llseek   = semihostfs_llseek,    /* SYS_SEEK */
       .fsync    = semihostfs_fsync,     /* SYS_FSYNC */
   };

   static const struct inode_operations semihostfs_file_iops = {
       .getattr  = semihostfs_getattr,   /* SYS_FSTAT */
       .setattr  = semihostfs_setattr,   /* SYS_FTRUNCATE for size */
   };

**Mount usage:**

.. code-block:: bash

   mount -t semihostfs none /mnt/host
   ls /mnt/host              # Lists host's share directory
   cat /mnt/host/foo.txt     # Reads host file
   echo "hi" > /mnt/host/x   # Creates/writes host file

Block Device Access
^^^^^^^^^^^^^^^^^^^

A Linux block driver can use disk image files on the host via the
existing ARM-compatible syscalls. No block-specific opcodes are
needed:

- **SYS_OPEN**: Open disk image file
- **SYS_READ / SYS_WRITE**: Read/write sectors
- **SYS_SEEK**: Seek to sector offset
- **SYS_FSYNC**: Flush writes to storage
- **SYS_FSTAT**: Get image size

Synchronous operation is acceptable for an initial implementation.

TTY Driver
^^^^^^^^^^

A Linux TTY driver uses the existing console syscalls plus
SYS_READC_POLL:

.. list-table::
   :header-rows: 1
   :widths: 15 20 45

   * - Opcode
     - Syscall
     - Linux Use
   * - 0x03
     - SYS_WRITEC
     - Write single character to console
   * - 0x04
     - SYS_WRITE0
     - Write null-terminated string
   * - 0x07
     - SYS_READC
     - Read character (blocking)
   * - 0x89
     - SYS_READC_POLL
     - Read character (non-blocking, for poll/select)
   * - 0x09
     - SYS_ISTTY
     - Check if fd is a TTY

The blocking SYS_READC is usable for simple console input;
SYS_READC_POLL enables proper TTY driver ``poll()`` implementation.

Other Existing Syscalls Useful from Linux
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Heap / Memory Info (SYS_HEAPINFO, 0x16):**

- Early boot: kernel can discover available RAM
- Platform driver: export memory layout to ``/sys/firmware/``
- Stack guard: inform kernel of stack boundaries

**Time Services:**

- SYS_CLOCK (0x10): monotonic clock (centiseconds since start)
- SYS_TIME (0x11): RTC / wall clock (seconds since epoch)
- SYS_ELAPSED (0x30): high-resolution timer (64-bit tick count)
- SYS_TICKFREQ (0x31): timer frequency (ticks per second)

**System Services:**

- SYS_GET_CMDLINE (0x15): pass kernel boot parameters from host
- SYS_EXIT (0x18) / SYS_EXIT_EXTENDED (0x20): implement ``reboot()``
  / ``halt()``

Implementation Notes
--------------------

Guest-Side Allocation Model
^^^^^^^^^^^^^^^^^^^^^^^^^^^

The ZBC protocol requires the **guest to allocate everything**; the
host only fills in pre-allocated space. This applies to all the
extension opcodes:

1. Guest builds the complete RIFF buffer with:

   - RIFF header (``RIFF`` + size + ``SEMI``)
   - CNFG chunk (on first request only)
   - CALL chunk with sub-chunks (opcode, PARM, DATA)
   - **Pre-allocated RETN chunk** (sized for the expected response)
   - **Pre-allocated ERRO chunk** (typically 64 bytes)

2. Guest writes buffer address to RIFF_PTR register.
3. Host reads buffer, executes syscall, writes response into RETN.
4. Guest reads response from same buffer.

For opcodes returning variable-length data (SYS_READDIR, SYS_STAT),
the guest must pre-allocate sufficient RETN space based on the
maximum expected response size.

Opcode Table Entries
^^^^^^^^^^^^^^^^^^^^

Each extension opcode has an entry in the opcode table. Example for
SYS_STAT:

.. code-block:: c

   {SH_SYS_STAT, 4,
    {{ZBC_CHUNK_DATA_PTR, 0, 1},   /* path from args[0], len from args[1] */
     {ZBC_CHUNK_PARM_UINT, 1, 0},  /* path_len */
     {ZBC_CHUNK_NONE, 0, 0},
     {ZBC_CHUNK_NONE, 0, 0}},
    ZBC_RESP_DATA, 2, 3}           /* dest=args[2], max_len=args[3]=48 */

   /* Guest call: args = {path_ptr, path_len, stat_buf_ptr, 48} */

Backend Vtable
^^^^^^^^^^^^^^

The host ``zbc_backend_t`` vtable carries one function pointer per
extension opcode:

.. code-block:: c

   /* Directory operations - wrap POSIX opendir/readdir/closedir */
   int (*opendir)(void *ctx, const char *path, size_t path_len);
   int (*readdir)(void *ctx, int dirfd, void *buf, size_t buf_size);
   int (*closedir)(void *ctx, int dirfd);

   /* File metadata - wrap POSIX stat/fstat/lstat */
   int (*stat)(void *ctx, const char *path, size_t path_len, void *stat_buf);
   int (*fstat)(void *ctx, int fd, void *stat_buf);
   int (*lstat)(void *ctx, const char *path, size_t path_len, void *stat_buf);

   /* File operations - wrap POSIX directly */
   int (*mkdir)(void *ctx, const char *path, size_t path_len, int mode);
   int (*rmdir)(void *ctx, const char *path, size_t path_len);
   int (*ftruncate)(void *ctx, int fd, uint64_t length);
   int (*fsync)(void *ctx, int fd);

   /* Console */
   int (*readc_poll)(void *ctx);

   /* Symlinks */
   int (*link)(void *ctx, const char *src, size_t src_len,
               const char *dst, size_t dst_len);
   int (*symlink)(void *ctx, const char *target, size_t target_len,
                  const char *linkpath, size_t link_len);
   int (*readlink)(void *ctx, const char *path, size_t path_len,
                   void *buf, size_t buf_size);

Client Code Size Impact
^^^^^^^^^^^^^^^^^^^^^^^

For constrained platforms like the 6502, adding the 14 extension
opcodes to the client library costs approximately **200-600 bytes**
of code, depending on which subset is actually called. Unused
opcodes are dead-code eliminated by the linker.
