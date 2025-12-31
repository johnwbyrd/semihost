Backend API
===========

Header: ``zbc_backend.h``

Backends provide the actual implementation of semihosting operations
(file I/O, console, time). The host library dispatches requests to
a backend vtable.

Backend Vtable
--------------

.. autoctype:: zbc_backend.h::zbc_backend_t

Return Value Conventions
^^^^^^^^^^^^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1
   :widths: 25 35 40

   * - Operation
     - Success
     - Error
   * - open
     - file descriptor (>=0)
     - -1
   * - close, seek, remove, rename
     - 0
     - -1
   * - read, write
     - bytes NOT transferred (0 = complete)
     - -1
   * - flen
     - file length
     - -1
   * - clock
     - centiseconds since start
     - -1
   * - time
     - seconds since epoch
     - -1
   * - tmpnam
     - 0 (fills buf)
     - -1

Set unused operations to NULL. The host library returns an error
to the guest for any NULL operation.

Built-in Backends
-----------------

Secure ANSI Backend
^^^^^^^^^^^^^^^^^^^

.. autocfunction:: zbc_backend.h::zbc_backend_ansi

**State initialization** (from ``zbc_backend_ansi.h``):

.. code-block:: c

   #include "zbc_backend_ansi.h"

   static zbc_ansi_state_t state;

   zbc_ansi_init(&state, "/path/to/sandbox/");

**Configuration:**

.. code-block:: c

   /* Add additional allowed paths */
   zbc_ansi_add_path(&state, "/usr/share/data/", 0);  /* read-only */
   zbc_ansi_add_path(&state, "/tmp/output/", 1);      /* read-write */

   /* Set flags */
   state.flags |= ZBC_ANSI_FLAG_ALLOW_SYSTEM;  /* enable system() */
   state.flags |= ZBC_ANSI_FLAG_READ_ONLY;     /* block all writes */

   /* Set callbacks for violations, exit, and timer config */
   zbc_ansi_set_callbacks(&state, violation_handler, exit_handler,
                          timer_handler, ctx);

**Cleanup:**

.. code-block:: c

   zbc_ansi_cleanup(&state);

Insecure ANSI Backend
^^^^^^^^^^^^^^^^^^^^^

.. autocfunction:: zbc_backend.h::zbc_backend_ansi_insecure

**Use only for trusted code** (e.g., your own test programs).

**State initialization** (from ``zbc_backend_ansi.h``):

.. code-block:: c

   #include "zbc_backend_ansi.h"

   static zbc_ansi_insecure_state_t state;

   zbc_ansi_insecure_init(&state);

**Cleanup:**

.. code-block:: c

   zbc_ansi_insecure_cleanup(&state);

Dummy Backend
^^^^^^^^^^^^^

.. autocfunction:: zbc_backend.h::zbc_backend_dummy

Implementing Custom Backends
----------------------------

To implement a custom backend:

1. Define your context structure (if needed)
2. Implement the vtable functions you need
3. Create a static ``zbc_backend_t`` with your function pointers
4. Use NULL for operations you don't support

**Example:**

.. code-block:: c

   typedef struct {
       int console_fd;
       /* ... */
   } my_backend_ctx_t;

   static int my_open(void *ctx, const char *path, size_t len, int mode)
   {
       my_backend_ctx_t *my = ctx;
       /* Implementation */
       return fd;
   }

   static void my_writec(void *ctx, char c)
   {
       my_backend_ctx_t *my = ctx;
       write(my->console_fd, &c, 1);
   }

   static const zbc_backend_t my_backend = {
       .open = my_open,
       .writec = my_writec,
       /* Other operations NULL - returns error to guest */
   };

   /* Usage */
   my_backend_ctx_t my_ctx = { .console_fd = STDOUT_FILENO };
   zbc_host_init(&host, &mem_ops, NULL, &my_backend, &my_ctx,
                 work_buf, sizeof(work_buf));

ANSI Backend Types
------------------

Header: ``zbc_backend_ansi.h``

Secure Backend State
^^^^^^^^^^^^^^^^^^^^

.. autoctype:: zbc_backend_ansi.h::zbc_ansi_state_t

.. autocfunction:: zbc_backend_ansi.h::zbc_ansi_init

.. autocfunction:: zbc_backend_ansi.h::zbc_ansi_add_path

.. autocfunction:: zbc_backend_ansi.h::zbc_ansi_set_policy

.. autocfunction:: zbc_backend_ansi.h::zbc_ansi_set_callbacks

.. autocfunction:: zbc_backend_ansi.h::zbc_ansi_cleanup

Insecure Backend State
^^^^^^^^^^^^^^^^^^^^^^

.. autoctype:: zbc_backend_ansi.h::zbc_ansi_insecure_state_t

.. autocfunction:: zbc_backend_ansi.h::zbc_ansi_insecure_init

.. autocfunction:: zbc_backend_ansi.h::zbc_ansi_insecure_cleanup

Policy Vtable
^^^^^^^^^^^^^

.. autoctype:: zbc_backend_ansi.h::zbc_ansi_policy_t
