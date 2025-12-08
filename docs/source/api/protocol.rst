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

zbc_chunk_t
^^^^^^^^^^^

Generic RIFF chunk structure.

.. code-block:: c

   typedef struct {
       uint32_t id;      /* FourCC, little-endian */
       uint32_t size;    /* Payload size (excluding header) */
       uint8_t  data[1]; /* Payload (variable length) */
   } zbc_chunk_t;

zbc_riff_t
^^^^^^^^^^

RIFF container structure.

.. code-block:: c

   typedef struct {
       uint32_t riff_id;    /* Must be ZBC_ID_RIFF */
       uint32_t size;       /* Size after this field */
       uint32_t form_type;  /* e.g., ZBC_ID_SEMI */
       uint8_t  data[1];    /* Container chunks */
   } zbc_riff_t;

zbc_parsed_t
^^^^^^^^^^^^

Result of parsing a RIFF SEMI buffer.

.. code-block:: c

   typedef struct {
       /* From CNFG chunk */
       uint8_t int_size;
       uint8_t ptr_size;
       uint8_t endianness;
       uint8_t has_cnfg;

       /* From CALL chunk */
       uint8_t opcode;
       uint8_t has_call;

       /* Parameters from PARM sub-chunks */
       int parm_count;
       int parms[ZBC_MAX_PARMS];

       /* Data from DATA sub-chunks */
       int data_count;
       struct {
           const uint8_t *ptr;
           size_t size;
       } data[ZBC_MAX_DATA];

       /* From RETN chunk */
       int result;
       int host_errno;
       uint8_t has_retn;

       /* From ERRO chunk */
       uint16_t proto_error;
       uint8_t has_erro;
   } zbc_parsed_t;

Chunk Functions
---------------

.. c:function:: int zbc_chunk_validate(const zbc_chunk_t *chunk, const uint8_t *container_end)

   Validate that a chunk fits within container bounds.

   :param chunk: Chunk to validate
   :param container_end: First byte past the container
   :returns: ``ZBC_OK``, ``ZBC_ERR_NULL_ARG``, ``ZBC_ERR_HEADER_OVERFLOW``, or ``ZBC_ERR_DATA_OVERFLOW``

.. c:function:: int zbc_chunk_next(zbc_chunk_t **out, const zbc_chunk_t *chunk)

   Get pointer to next sibling chunk.

   :param out: Receives pointer to next chunk
   :param chunk: Current chunk
   :returns: ``ZBC_OK`` or ``ZBC_ERR_NULL_ARG``

   Caller must validate the returned chunk before accessing.

.. c:function:: int zbc_chunk_first_sub(zbc_chunk_t **out, const zbc_chunk_t *container, size_t header_size)

   Get pointer to first sub-chunk within a container.

   :param out: Receives pointer to first sub-chunk
   :param container: Parent chunk (e.g., CALL)
   :param header_size: Bytes to skip before sub-chunks
   :returns: ``ZBC_OK`` or ``ZBC_ERR_NULL_ARG``

.. c:function:: int zbc_chunk_end(const uint8_t **out, const zbc_chunk_t *chunk)

   Get end pointer for a chunk.

   :param out: Receives pointer past chunk data
   :param chunk: The chunk
   :returns: ``ZBC_OK`` or ``ZBC_ERR_NULL_ARG``

.. c:function:: int zbc_chunk_find(zbc_chunk_t **out, const uint8_t *start, const uint8_t *end, uint32_t id)

   Find chunk by ID within bounds.

   :param out: Receives pointer to found chunk
   :param start: Start of search region
   :param end: End of search region
   :param id: FourCC to find
   :returns: ``ZBC_OK``, ``ZBC_ERR_NOT_FOUND``, or validation error

RIFF Functions
--------------

.. c:function:: int zbc_riff_validate(const zbc_riff_t *riff, size_t buf_size, uint32_t expected_form)

   Validate RIFF container.

   :param riff: Pointer to RIFF container
   :param buf_size: Total buffer size
   :param expected_form: Expected form type (e.g., ``ZBC_ID_SEMI``)
   :returns: ``ZBC_OK`` or validation error

.. c:function:: int zbc_riff_end(const uint8_t **out, const zbc_riff_t *riff)

   Get end pointer for RIFF container.

   :param out: Receives pointer past RIFF data
   :param riff: The RIFF container
   :returns: ``ZBC_OK`` or ``ZBC_ERR_NULL_ARG``

.. c:function:: int zbc_riff_parse(zbc_parsed_t *out, const uint8_t *buf, size_t buf_size, int int_size, int endian)

   Parse a RIFF SEMI buffer.

   :param out: Receives parsed structure
   :param buf: RIFF buffer to parse
   :param buf_size: Size of buffer
   :param int_size: Guest int size (for decoding values)
   :param endian: Guest endianness (``ZBC_ENDIAN_*``)
   :returns: ``ZBC_OK`` or parse error

   This is the main parsing entry point. It walks all chunks and
   populates the ``zbc_parsed_t`` structure. After calling:

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

.. c:function:: size_t zbc_strlen(const char *s)

   String length (no libc dependency).

   :param s: Null-terminated string
   :returns: Length in bytes (excluding null)

.. c:function:: void zbc_write_native_uint(uint8_t *buf, unsigned int value, int size, int endianness)

   Write integer in specified endianness.

   :param buf: Destination buffer
   :param value: Value to write
   :param size: Size in bytes (1-4)
   :param endianness: ``ZBC_ENDIAN_LITTLE`` or ``ZBC_ENDIAN_BIG``

.. c:function:: int zbc_read_native_int(const uint8_t *buf, int size, int endianness)

   Read signed integer in specified endianness.

   :param buf: Source buffer
   :param size: Size in bytes (1-4)
   :param endianness: ``ZBC_ENDIAN_LITTLE`` or ``ZBC_ENDIAN_BIG``
   :returns: Signed integer value

.. c:function:: unsigned int zbc_read_native_uint(const uint8_t *buf, int size, int endianness)

   Read unsigned integer in specified endianness.

   :param buf: Source buffer
   :param size: Size in bytes (1-4)
   :param endianness: ``ZBC_ENDIAN_LITTLE`` or ``ZBC_ENDIAN_BIG``
   :returns: Unsigned integer value

Opcode Table
------------

.. c:function:: const zbc_opcode_entry_t *zbc_opcode_lookup(int opcode)

   Look up opcode in the syscall table.

   :param opcode: ``SH_SYS_*`` opcode
   :returns: Pointer to table entry, or NULL if not found

.. c:function:: int zbc_opcode_count(void)

   Get number of entries in opcode table.

   :returns: Count of supported opcodes

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
