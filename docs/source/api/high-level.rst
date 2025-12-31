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

.. autoctype:: zbc_api.h::zbc_api_t

Initialization
--------------

.. autocfunction:: zbc_api.h::zbc_api_init

.. autocfunction:: zbc_api.h::zbc_api_errno

File Operations
---------------

.. autocfunction:: zbc_api.h::zbc_api_open

.. autocfunction:: zbc_api.h::zbc_api_close

.. autocfunction:: zbc_api.h::zbc_api_read

.. autocfunction:: zbc_api.h::zbc_api_write

.. autocfunction:: zbc_api.h::zbc_api_seek

.. autocfunction:: zbc_api.h::zbc_api_flen

.. autocfunction:: zbc_api.h::zbc_api_istty

.. autocfunction:: zbc_api.h::zbc_api_remove

.. autocfunction:: zbc_api.h::zbc_api_rename

.. autocfunction:: zbc_api.h::zbc_api_tmpnam

Console Operations
------------------

.. autocfunction:: zbc_api.h::zbc_api_writec

.. autocfunction:: zbc_api.h::zbc_api_write0

.. autocfunction:: zbc_api.h::zbc_api_readc

Time Operations
---------------

.. autocfunction:: zbc_api.h::zbc_api_clock

.. autocfunction:: zbc_api.h::zbc_api_time

.. autocfunction:: zbc_api.h::zbc_api_tickfreq

.. autocfunction:: zbc_api.h::zbc_api_elapsed

.. autocfunction:: zbc_api.h::zbc_api_timer_config

System Operations
-----------------

.. autocfunction:: zbc_api.h::zbc_api_iserror

.. autocfunction:: zbc_api.h::zbc_api_get_errno

.. autocfunction:: zbc_api.h::zbc_api_system

.. autocfunction:: zbc_api.h::zbc_api_get_cmdline

.. autocfunction:: zbc_api.h::zbc_api_heapinfo

.. autocfunction:: zbc_api.h::zbc_api_exit

.. autocfunction:: zbc_api.h::zbc_api_exit_extended
