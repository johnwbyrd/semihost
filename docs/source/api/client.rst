Client API
==========

Header: ``zbc_client.h``

The client API provides functions for guest/embedded code to make
semihosting calls to the host system.

Types
-----

zbc_client_state_t
^^^^^^^^^^^^^^^^^^

Client state structure. Initialize with ``zbc_client_init()``.

.. code-block:: c

   typedef struct {
       volatile uint8_t *dev_base;  /* Pointer to device registers */
       uint8_t cnfg_sent;           /* 1 if CNFG chunk has been sent */
       uint8_t int_size;            /* sizeof(int) on this platform */
       uint8_t ptr_size;            /* sizeof(void*) on this platform */
       uint8_t endianness;          /* ZBC_ENDIAN_* */
       void (*doorbell_callback)(void *);  /* For testing */
       void *doorbell_ctx;
   } zbc_client_state_t;

zbc_response_t
^^^^^^^^^^^^^^

Response from a semihosting call.

.. code-block:: c

   typedef struct {
       int result;           /* Syscall return value */
       int error_code;       /* Errno value from host */
       const uint8_t *data;  /* Pointer to DATA payload (if any) */
       size_t data_size;     /* Size of DATA payload */
       int is_error;         /* 1 if ERRO chunk received */
       int proto_error;      /* Protocol error code from ERRO */
   } zbc_response_t;

Initialization
--------------

.. c:function:: void zbc_client_init(zbc_client_state_t *state, volatile void *dev_base)

   Initialize client state with device base address.

   :param state: Client state structure to initialize
   :param dev_base: Memory-mapped device base address

   The function detects the platform's integer size, pointer size, and
   endianness automatically using compile-time configuration macros.

.. c:function:: int zbc_client_check_signature(const zbc_client_state_t *state)

   Check if a semihosting device is present by reading the signature.

   :param state: Initialized client state
   :returns: 1 if device signature matches "SEMIHOST", 0 otherwise

   Call this before making semihosting calls to verify the device exists.

.. c:function:: int zbc_client_device_present(const zbc_client_state_t *state)

   Check device present bit in status register.

   :param state: Initialized client state
   :returns: 1 if device present bit is set, 0 otherwise

.. c:function:: void zbc_client_reset_cnfg(zbc_client_state_t *state)

   Reset the CNFG sent flag, forcing resend on next call.

   :param state: Initialized client state

   Normally the CNFG chunk is sent only once. Use this if you need
   to resend configuration (e.g., after device reset).

Making Calls
------------

.. c:function:: int zbc_call(zbc_response_t *response, zbc_client_state_t *state, void *buf, size_t buf_size, int opcode, uintptr_t *args)

   Execute a semihosting syscall.

   :param response: Receives parsed response (result, errno, data pointer)
   :param state: Initialized client state
   :param buf: RIFF buffer (caller-provided)
   :param buf_size: Size of buffer in bytes
   :param opcode: ``SH_SYS_*`` opcode from ``zbc_protocol.h``
   :param args: Array of arguments (layout depends on opcode), may be NULL
   :returns: ``ZBC_OK`` on success, ``ZBC_ERR_*`` on protocol/transport error

   This is the main entry point for making semihosting calls. The function:

   1. Builds a RIFF request from the opcode table
   2. Submits the request to the device
   3. Waits for the response (polling)
   4. Parses the response into the ``response`` structure

   On success, check ``response->result`` for the syscall return value
   and ``response->error_code`` for the host errno.

   **Example:**

   .. code-block:: c

      zbc_response_t response;
      uintptr_t args[1];
      args[0] = (uintptr_t)"Hello\n";

      int rc = zbc_call(&response, &client, buf, sizeof(buf),
                        SH_SYS_WRITE0, args);
      if (rc != ZBC_OK) {
          /* Protocol error */
      }

.. c:function:: uintptr_t zbc_semihost(zbc_client_state_t *state, uint8_t *riff_buf, size_t riff_buf_size, uintptr_t op, uintptr_t param)

   ARM-compatible semihost entry point.

   :param state: Initialized client state
   :param riff_buf: RIFF buffer
   :param riff_buf_size: Size of buffer
   :param op: ``SH_SYS_*`` opcode
   :param param: Pointer to args array (cast from ``uintptr_t*``)
   :returns: Syscall result, or ``(uintptr_t)-1`` on error

   This is a thin wrapper around ``zbc_call()`` that accepts the ARM-style
   parameter block format (op, pointer-to-args) used by picolibc and newlib.

   Use this to implement ``sys_semihost()`` for libc integration:

   .. code-block:: c

      uintptr_t sys_semihost(uintptr_t op, uintptr_t param)
      {
          return zbc_semihost(&client, riff_buf, sizeof(riff_buf), op, param);
      }

Low-Level Functions
-------------------

.. c:function:: int zbc_client_submit_poll(zbc_client_state_t *state, void *buf, size_t size)

   Submit a RIFF request and poll for response.

   :param state: Initialized client state
   :param buf: RIFF buffer containing the request
   :param size: Size of request data
   :returns: ``ZBC_OK`` on success, error code on failure

   This writes the buffer address to RIFF_PTR, triggers DOORBELL, and
   polls STATUS until RESPONSE_READY is set. Most users should use
   ``zbc_call()`` instead.

.. c:function:: int zbc_parse_response(zbc_response_t *response, const uint8_t *buf, size_t capacity, const zbc_client_state_t *state)

   Parse response from RIFF buffer.

   :param response: Receives parsed response
   :param buf: RIFF buffer containing the response
   :param capacity: Buffer capacity
   :param state: Client state (for int_size/endianness)
   :returns: ``ZBC_OK`` on success, ``ZBC_ERR_PARSE_ERROR`` on failure

   Extracts the result, errno, and data from a RETN or ERRO chunk.
   Most users should use ``zbc_call()`` instead.

Configuration Macros
--------------------

These macros can be overridden at compile time to configure the client
for non-standard platforms:

``ZBC_CLIENT_INT_SIZE``
   Size of ``int`` in bytes. Default: ``sizeof(int)``

``ZBC_CLIENT_PTR_SIZE``
   Size of pointers in bytes. Default: ``sizeof(void *)``

``ZBC_CLIENT_ENDIANNESS``
   Endianness (``ZBC_ENDIAN_LITTLE`` or ``ZBC_ENDIAN_BIG``).
   Default: detected from ``__BYTE_ORDER__``
