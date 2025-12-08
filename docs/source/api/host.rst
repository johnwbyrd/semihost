Host API
========

Header: ``zbc_host.h``

The host API provides functions for processing semihosting requests
from guest code. Use this when implementing a semihosting device in
an emulator or virtual machine.

Types
-----

zbc_host_mem_ops_t
^^^^^^^^^^^^^^^^^^

Memory access callbacks for reading/writing guest memory.

.. code-block:: c

   typedef struct {
       uint8_t (*read_u8)(uint64_t addr, void *ctx);
       void (*write_u8)(uint64_t addr, uint8_t val, void *ctx);
       void (*read_block)(void *dest, uint64_t addr, size_t size, void *ctx);
       void (*write_block)(uint64_t addr, const void *src, size_t size, void *ctx);
   } zbc_host_mem_ops_t;

You must implement all four callbacks. The ``ctx`` parameter is passed
through from ``zbc_host_init()``.

zbc_host_state_t
^^^^^^^^^^^^^^^^

Host state structure. Initialize with ``zbc_host_init()``.

.. code-block:: c

   typedef struct {
       zbc_host_mem_ops_t mem_ops;
       void *mem_ctx;
       const struct zbc_backend_s *backend;
       void *backend_ctx;
       uint8_t *work_buf;
       size_t work_buf_size;
       uint8_t guest_int_size;
       uint8_t guest_ptr_size;
       uint8_t guest_endianness;
       uint8_t cnfg_received;
   } zbc_host_state_t;

Functions
---------

.. c:function:: void zbc_host_init(zbc_host_state_t *state, const zbc_host_mem_ops_t *mem_ops, void *mem_ctx, const struct zbc_backend_s *backend, void *backend_ctx, uint8_t *work_buf, size_t work_buf_size)

   Initialize host state.

   :param state: Host state structure to initialize
   :param mem_ops: Memory operation callbacks
   :param mem_ctx: Context passed to memory callbacks
   :param backend: Backend vtable (from ``zbc_backend_*()`` factory)
   :param backend_ctx: Backend-specific state
   :param work_buf: Working buffer for RIFF parsing
   :param work_buf_size: Size of working buffer

   **Example:**

   .. code-block:: c

      static zbc_host_state_t host;
      static uint8_t work_buf[1024];
      static zbc_ansi_insecure_state_t backend_state;

      zbc_host_mem_ops_t mem_ops = {
          .read_u8 = my_read_u8,
          .write_u8 = my_write_u8,
          .read_block = my_read_block,
          .write_block = my_write_block
      };

      zbc_ansi_insecure_init(&backend_state);

      zbc_host_init(&host, &mem_ops, NULL,
                    zbc_backend_ansi_insecure(), &backend_state,
                    work_buf, sizeof(work_buf));

.. c:function:: int zbc_host_process(zbc_host_state_t *state, uint64_t riff_addr)

   Process a semihosting request.

   :param state: Initialized host state
   :param riff_addr: Guest address of RIFF buffer
   :returns: ``ZBC_OK`` on success, error code on failure

   Call this when the guest writes to DOORBELL. The function:

   1. Reads the RIFF request from guest memory
   2. Parses the CNFG chunk (first request only)
   3. Parses the CALL chunk and sub-chunks
   4. Dispatches to the appropriate backend function
   5. Writes the RETN (or ERRO) chunk back to guest memory

   After this returns, set STATUS bit 0 (RESPONSE_READY) in your
   device register emulation.

   **Example:**

   .. code-block:: c

      void on_doorbell_write(uint64_t riff_ptr) {
          int rc = zbc_host_process(&host, riff_ptr);
          if (rc == ZBC_OK) {
              set_status_response_ready();
          }
      }

Helper Functions
----------------

.. c:function:: int zbc_host_read_guest_int(const zbc_host_state_t *state, const uint8_t *data, size_t size)

   Read an integer from guest-endian data.

   :param state: Host state (for guest endianness)
   :param data: Pointer to integer data
   :param size: Size of integer in bytes
   :returns: Integer value

   Converts from guest endianness to host integer.

.. c:function:: void zbc_host_write_guest_int(const zbc_host_state_t *state, uint8_t *data, int value, size_t size)

   Write an integer in guest-endian format.

   :param state: Host state (for guest endianness)
   :param data: Destination buffer
   :param value: Integer value to write
   :param size: Size of integer in bytes

   Converts from host integer to guest endianness.

Work Buffer Sizing
------------------

The work buffer should be large enough to hold the largest RIFF request
plus space for the response. Recommended sizes:

- **Minimum:** 256 bytes (most syscalls)
- **Typical:** 1024 bytes (comfortable for file operations)
- **Large reads:** Match the largest expected SYS_READ count plus overhead

For SYS_READ operations, the buffer is split: half for reading the request,
half for the read data. So a 1024-byte buffer supports reads up to ~500 bytes.
