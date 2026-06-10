Host API
========

Header: ``zbc_host.h``

The host API provides functions for processing semihosting requests
from guest code. Use this when implementing a semihosting device in
an emulator or virtual machine.

Types
-----

.. autoctype:: zbc_host.h::zbc_host_mem_ops_t

.. autoctype:: zbc_host.h::zbc_host_state_t

Functions
---------

.. autocfunction:: zbc_host.h::zbc_host_init

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

.. autocfunction:: zbc_host.h::zbc_host_set_platform_config

**Example:**

.. code-block:: c

   /* Emulator knows the guest CPU: CNFG becomes optional */
   zbc_host_set_platform_config(&host, /*int_size=*/2, /*ptr_size=*/2,
                                ZBC_ENDIAN_LITTLE);

.. autocfunction:: zbc_host.h::zbc_host_set_proto_error_cb

**Example:**

.. code-block:: c

   /* Latch register-channel diagnostics into the device registers */
   static void on_proto_error(void *ctx, int code) {
       my_device_t *dev = ctx;
       dev->error_code = (uint16_t)code;       /* ERROR_CODE @ 0x1A */
       dev->status |= ZBC_STATUS_PROTO_ERROR;  /* STATUS bit 2 */
   }

   zbc_host_set_proto_error_cb(&host, on_proto_error, &my_device);

.. autocfunction:: zbc_host.h::zbc_host_process

**Example:**

.. code-block:: c

   void on_doorbell_write(uintptr_t riff_ptr) {
       zbc_host_process(&host, riff_ptr);
       /* Response (or register error) is complete: set RESPONSE_READY */
       my_device.status |= ZBC_STATUS_RESPONSE_READY;
   }

Helper Functions
----------------

.. autocfunction:: zbc_host.h::zbc_host_read_guest_int

.. autocfunction:: zbc_host.h::zbc_host_write_guest_int

Work Buffer Sizing
------------------

The work buffer should be large enough to hold the largest RIFF request
plus space for the response. Recommended sizes:

- **Minimum:** 256 bytes (most syscalls)
- **Typical:** 1024 bytes (comfortable for file operations)
- **Large reads:** Match the largest expected SYS_READ count plus overhead

For SYS_READ operations, the buffer is split: half for reading the request,
half for the read data. So a 1024-byte buffer supports reads up to ~500 bytes.
