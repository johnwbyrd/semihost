High-Level API
==============

Header: ``zbc_api.h``

The high-level API provides POSIX-like wrapper functions for guest code.
It bundles client state, buffer, and errno tracking into a single
``zbc_api_t`` context, so you can write code like ``zbc_api_open()``
instead of manually packing argument arrays for ``zbc_call()``.

**Example:**

.. code-block:: c

   #include "zbc_api.h"

   zbc_client_state_t client;
   zbc_api_t api;
   uint8_t buf[256];

   zbc_client_init(&client, (void *)0xFFFF0000);
   zbc_api_init(&api, &client, buf, sizeof(buf));

   int fd = zbc_api_open(&api, "/tmp/test.txt", SH_OPEN_W);
   zbc_api_write(&api, fd, "Hello\n", 6);
   zbc_api_close(&api, fd);

Types
-----

.. doxygenstruct:: zbc_api_t

Initialization
--------------

.. doxygenfunction:: zbc_api_init

.. doxygenfunction:: zbc_api_errno

File Operations
---------------

.. doxygenfunction:: zbc_api_open

.. doxygenfunction:: zbc_api_close

.. doxygenfunction:: zbc_api_read

.. doxygenfunction:: zbc_api_write

.. doxygenfunction:: zbc_api_seek

.. doxygenfunction:: zbc_api_flen

.. doxygenfunction:: zbc_api_istty

.. doxygenfunction:: zbc_api_remove

.. doxygenfunction:: zbc_api_rename

.. doxygenfunction:: zbc_api_tmpnam

Console Operations
------------------

.. doxygenfunction:: zbc_api_writec

.. doxygenfunction:: zbc_api_write0

.. doxygenfunction:: zbc_api_readc

Time Operations
---------------

.. doxygenfunction:: zbc_api_clock

.. doxygenfunction:: zbc_api_time

.. doxygenfunction:: zbc_api_tickfreq

.. doxygenfunction:: zbc_api_elapsed

.. doxygenfunction:: zbc_api_timer_config

System Operations
-----------------

.. doxygenfunction:: zbc_api_iserror

.. doxygenfunction:: zbc_api_get_errno

.. doxygenfunction:: zbc_api_system

.. doxygenfunction:: zbc_api_get_cmdline

.. doxygenfunction:: zbc_api_heapinfo

.. doxygenfunction:: zbc_api_exit

.. doxygenfunction:: zbc_api_exit_extended
