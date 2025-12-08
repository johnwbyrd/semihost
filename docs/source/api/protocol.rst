Protocol API
============

Header: ``zbc_protocol.h``

Low-level RIFF chunk parsing, constants, and helper functions. Most
users don't need these directly -- use the client or host APIs instead.

Constants
---------

ARM Semihosting Opcodes
^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: c

   #define SH_SYS_OPEN           0x01
   #define SH_SYS_CLOSE          0x02
   #define SH_SYS_WRITEC         0x03
   #define SH_SYS_WRITE0         0x04
   #define SH_SYS_WRITE          0x05
   #define SH_SYS_READ           0x06
   #define SH_SYS_READC          0x07
   #define SH_SYS_ISERROR        0x08
   #define SH_SYS_ISTTY          0x09
   #define SH_SYS_SEEK           0x0A
   #define SH_SYS_FLEN           0x0C
   #define SH_SYS_TMPNAM         0x0D
   #define SH_SYS_REMOVE         0x0E
   #define SH_SYS_RENAME         0x0F
   #define SH_SYS_CLOCK          0x10
   #define SH_SYS_TIME           0x11
   #define SH_SYS_SYSTEM         0x12
   #define SH_SYS_ERRNO          0x13
   #define SH_SYS_GET_CMDLINE    0x15
   #define SH_SYS_HEAPINFO       0x16
   #define SH_SYS_EXIT           0x18
   #define SH_SYS_EXIT_EXTENDED  0x20
   #define SH_SYS_ELAPSED        0x30
   #define SH_SYS_TICKFREQ       0x31

Open Mode Flags
^^^^^^^^^^^^^^^

.. code-block:: c

   #define SH_OPEN_R         0   /* "r" */
   #define SH_OPEN_RB        1   /* "rb" */
   #define SH_OPEN_R_PLUS    2   /* "r+" */
   #define SH_OPEN_R_PLUS_B  3   /* "r+b" */
   #define SH_OPEN_W         4   /* "w" */
   #define SH_OPEN_WB        5   /* "wb" */
   #define SH_OPEN_W_PLUS    6   /* "w+" */
   #define SH_OPEN_W_PLUS_B  7   /* "w+b" */
   #define SH_OPEN_A         8   /* "a" */
   #define SH_OPEN_AB        9   /* "ab" */
   #define SH_OPEN_A_PLUS    10  /* "a+" */
   #define SH_OPEN_A_PLUS_B  11  /* "a+b" */

RIFF FourCC Codes
^^^^^^^^^^^^^^^^^

.. code-block:: c

   #define ZBC_ID_RIFF  ZBC_MAKEFOURCC('R','I','F','F')
   #define ZBC_ID_SEMI  ZBC_MAKEFOURCC('S','E','M','I')
   #define ZBC_ID_CNFG  ZBC_MAKEFOURCC('C','N','F','G')
   #define ZBC_ID_CALL  ZBC_MAKEFOURCC('C','A','L','L')
   #define ZBC_ID_PARM  ZBC_MAKEFOURCC('P','A','R','M')
   #define ZBC_ID_DATA  ZBC_MAKEFOURCC('D','A','T','A')
   #define ZBC_ID_RETN  ZBC_MAKEFOURCC('R','E','T','N')
   #define ZBC_ID_ERRO  ZBC_MAKEFOURCC('E','R','R','O')

Device Register Offsets
^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: c

   #define ZBC_REG_SIGNATURE   0x00  /* 8 bytes, R */
   #define ZBC_REG_RIFF_PTR    0x08  /* 16 bytes, RW */
   #define ZBC_REG_DOORBELL    0x18  /* 1 byte, W */
   #define ZBC_REG_IRQ_STATUS  0x19  /* 1 byte, R */
   #define ZBC_REG_IRQ_ENABLE  0x1A  /* 1 byte, RW */
   #define ZBC_REG_IRQ_ACK     0x1B  /* 1 byte, W */
   #define ZBC_REG_STATUS      0x1C  /* 1 byte, R */
   #define ZBC_REG_SIZE        0x20  /* Total: 32 bytes */

Structures
----------

.. autoctype:: zbc_protocol.h::zbc_chunk_t

.. autoctype:: zbc_protocol.h::zbc_riff_t

.. autoctype:: zbc_protocol.h::zbc_parsed_t

Chunk Functions
---------------

.. autocfunction:: zbc_protocol.h::zbc_chunk_validate

.. autocfunction:: zbc_protocol.h::zbc_chunk_next

.. autocfunction:: zbc_protocol.h::zbc_chunk_first_sub

.. autocfunction:: zbc_protocol.h::zbc_chunk_end

.. autocfunction:: zbc_protocol.h::zbc_chunk_find

RIFF Functions
--------------

.. autocfunction:: zbc_protocol.h::zbc_riff_validate

.. autocfunction:: zbc_protocol.h::zbc_riff_end

.. autocfunction:: zbc_protocol.h::zbc_riff_parse

**Example:**

.. code-block:: c

   zbc_parsed_t parsed;
   int rc = zbc_riff_parse(&parsed, buf, size, 4, ZBC_ENDIAN_LITTLE);
   if (rc == ZBC_OK) {
       if (parsed.has_retn) {
           /* Use parsed.result, parsed.host_errno */
       }
       if (parsed.has_erro) {
           /* Handle protocol error */
       }
   }

Helper Functions
----------------

.. autocfunction:: zbc_protocol.h::zbc_strlen

.. autocfunction:: zbc_protocol.h::zbc_write_native_uint

.. autocfunction:: zbc_protocol.h::zbc_read_native_int

.. autocfunction:: zbc_protocol.h::zbc_read_native_uint

RIFF Writing Functions
----------------------

.. autocfunction:: zbc_protocol.h::zbc_riff_begin_chunk

.. autocfunction:: zbc_protocol.h::zbc_riff_patch_size

.. autocfunction:: zbc_protocol.h::zbc_riff_write_bytes

.. autocfunction:: zbc_protocol.h::zbc_riff_pad

.. autocfunction:: zbc_protocol.h::zbc_riff_begin_container

.. autocfunction:: zbc_protocol.h::zbc_riff_validate_container

RIFF Reading Functions
----------------------

.. autocfunction:: zbc_protocol.h::zbc_riff_read_header

.. autocfunction:: zbc_protocol.h::zbc_riff_skip_chunk

Opcode Table
------------

.. autocfunction:: zbc_protocol.h::zbc_opcode_lookup

.. autocfunction:: zbc_protocol.h::zbc_opcode_count

Byte Manipulation Macros
------------------------

.. code-block:: c

   /* Read/write little-endian 32-bit */
   ZBC_WRITE_U32_LE(buf, val)
   ZBC_READ_U32_LE(buf)

   /* Read/write little-endian 16-bit */
   ZBC_WRITE_U16_LE(buf, val)
   ZBC_READ_U16_LE(buf)

   /* Write FourCC */
   ZBC_WRITE_FOURCC(buf, c0, c1, c2, c3)

   /* Memory operations (no libc) */
   ZBC_MEMCPY(dst, src, n)
   ZBC_MEMSET(dst, val, n)

   /* Padding to even boundary */
   ZBC_PAD_SIZE(size)
