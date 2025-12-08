Backend API
===========

Header: ``zbc_backend.h``

Backends provide the actual implementation of semihosting operations
(file I/O, console, time). The host library dispatches requests to
a backend vtable.

Backend Vtable
--------------

.. code-block:: c

   typedef struct zbc_backend_s {
       /* File operations */
       int (*open)(void *ctx, const char *path, size_t path_len, int mode);
       int (*close)(void *ctx, int fd);
       int (*read)(void *ctx, int fd, void *buf, size_t count);
       int (*write)(void *ctx, int fd, const void *buf, size_t count);
       int (*seek)(void *ctx, int fd, int pos);
       int (*flen)(void *ctx, int fd);
       int (*remove)(void *ctx, const char *path, size_t path_len);
       int (*rename)(void *ctx, const char *old_path, size_t old_len,
                     const char *new_path, size_t new_len);
       int (*tmpnam)(void *ctx, char *buf, size_t buf_size, int id);

       /* Console operations */
       void (*writec)(void *ctx, char c);
       void (*write0)(void *ctx, const char *str);
       int (*readc)(void *ctx);

       /* Status operations */
       int (*iserror)(void *ctx, int status);
       int (*istty)(void *ctx, int fd);
       int (*clock)(void *ctx);
       int (*time)(void *ctx);
       int (*elapsed)(void *ctx, unsigned int *lo, unsigned int *hi);
       int (*tickfreq)(void *ctx);

       /* System operations */
       int (*do_system)(void *ctx, const char *cmd, size_t cmd_len);
       int (*get_cmdline)(void *ctx, char *buf, size_t buf_size);
       int (*heapinfo)(void *ctx, unsigned int *heap_base, unsigned int *heap_limit,
                       unsigned int *stack_base, unsigned int *stack_limit);
       void (*do_exit)(void *ctx, unsigned int reason, unsigned int subcode);
       int (*get_errno)(void *ctx);
   } zbc_backend_t;

Return Value Conventions
^^^^^^^^^^^^^^^^^^^^^^^^

.. list-table::
   :header-rows: 1
   :widths: 25 35 40

   * - Operation
     - Success
     - Error
   * - open
     - file descriptor (≥0)
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

.. c:function:: const zbc_backend_t *zbc_backend_ansi(void)

   Get the secure (sandboxed) ANSI backend.

   :returns: Pointer to backend vtable

   The secure backend restricts file access to a sandbox directory.
   Guest code cannot escape the sandbox or access arbitrary host files.

   **State initialization:**

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

      /* Set callbacks for violations and exit */
      zbc_ansi_set_callbacks(&state, violation_handler, exit_handler, ctx);

   **Cleanup:**

   .. code-block:: c

      zbc_ansi_cleanup(&state);

Insecure ANSI Backend
^^^^^^^^^^^^^^^^^^^^^

.. c:function:: const zbc_backend_t *zbc_backend_ansi_insecure(void)

   Get the insecure (unrestricted) ANSI backend.

   :returns: Pointer to backend vtable

   The insecure backend provides unrestricted access to the host
   filesystem. Guest code can read, write, and delete any file the
   host process can access.

   **Use only for trusted code** (e.g., your own test programs).

   **State initialization:**

   .. code-block:: c

      #include "zbc_backend_ansi.h"

      static zbc_ansi_insecure_state_t state;

      zbc_ansi_insecure_init(&state);

   **Cleanup:**

   .. code-block:: c

      zbc_ansi_insecure_cleanup(&state);

Dummy Backend
^^^^^^^^^^^^^

.. c:function:: const zbc_backend_t *zbc_backend_dummy(void)

   Get the dummy (no-op) backend.

   :returns: Pointer to backend vtable

   All operations succeed with no side effects. Useful for testing
   the host processing logic without actual I/O.

   No state required -- pass NULL as backend_ctx.

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

Backend Operation Details
-------------------------

File Operations
^^^^^^^^^^^^^^^

``open(ctx, path, path_len, mode)``
   Open a file. Mode values are ``SH_OPEN_*`` constants.
   Path is NOT null-terminated; use ``path_len``.

``close(ctx, fd)``
   Close a file descriptor.

``read(ctx, fd, buf, count)``
   Read up to ``count`` bytes. Returns bytes NOT read (0 = all read).

``write(ctx, fd, buf, count)``
   Write ``count`` bytes. Returns bytes NOT written (0 = all written).

``seek(ctx, fd, pos)``
   Seek to absolute position ``pos``.

``flen(ctx, fd)``
   Return file length.

``remove(ctx, path, path_len)``
   Delete a file. Path is NOT null-terminated.

``rename(ctx, old_path, old_len, new_path, new_len)``
   Rename a file. Paths are NOT null-terminated.

``tmpnam(ctx, buf, buf_size, id)``
   Generate a temporary filename. Write to ``buf``, return 0 on success.

Console Operations
^^^^^^^^^^^^^^^^^^

``writec(ctx, c)``
   Write a single character to console.

``write0(ctx, str)``
   Write a null-terminated string to console.

``readc(ctx)``
   Read a character from console (blocking). Return character or -1.

System Operations
^^^^^^^^^^^^^^^^^

``clock(ctx)``
   Return centiseconds since program start.

``time(ctx)``
   Return seconds since Unix epoch.

``elapsed(ctx, lo, hi)``
   Return 64-bit tick count in ``*lo`` and ``*hi``.

``tickfreq(ctx)``
   Return ticks per second.

``do_system(ctx, cmd, cmd_len)``
   Execute a shell command. Return exit code.

``do_exit(ctx, reason, subcode)``
   Guest is exiting. Handle as appropriate (stop emulation, etc.).

``get_errno(ctx)``
   Return last errno value.
