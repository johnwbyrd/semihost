Client API
==========

Header: ``zbc_client.h``

The client API provides functions for guest/embedded code to make
semihosting calls to the host system.

Types
-----

.. doxygenstruct:: zbc_client_state_t

.. doxygenstruct:: zbc_response_t

Initialization
--------------

.. doxygenfunction:: zbc_client_init

.. doxygenfunction:: zbc_client_check_signature

.. doxygenfunction:: zbc_client_reset_cnfg

Making Calls
------------

.. doxygenfunction:: zbc_call

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

.. doxygenfunction:: zbc_semihost

Use this to implement ``sys_semihost()`` for libc integration:

.. code-block:: c

   uintptr_t sys_semihost(uintptr_t op, uintptr_t param)
   {
       return zbc_semihost(&client, riff_buf, sizeof(riff_buf), op, param);
   }

Low-Level Functions
-------------------

.. doxygenfunction:: zbc_client_submit

.. doxygenfunction:: zbc_parse_response

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
