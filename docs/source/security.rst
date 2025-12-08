Security
=========

ZBC Semihosting provides several layers of protection to prevent guest code
from compromising the host system. However, semihosting is inherently powerful --
guest code can access host files, console, and system calls. Use appropriate
backends and configurations for your threat model.

Backends
--------

**Insecure ANSI Backend** (`zbc_backend_ansi_insecure`)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- **Unrestricted host access**: Guest can read/write/delete any file the host process can access.
- **No sandboxing**: Full ``fopen``/``fread``/``fwrite`` using host stdio.
- **Use case**: Trusted guest code, early development, test suites.
- **Risk**: Malicious guest can delete host files, read sensitive data.
- **Init**: ``zbc_ansi_insecure_init(&state);``

**Secure ANSI Backend** (`zbc_backend_ansi`)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- **Directory jail**: All paths prefixed with ``sandbox_dir`` (e.g., ``/tmp/zbc-sandbox/``).
- **Path traversal blocked**: ``..`` and absolute paths outside sandbox denied.
- **Additional paths**: Allow read-only/write dirs via ``zbc_ansi_add_path()``.
- **Flags**:
  - ``ZBC_ANSI_FLAG_ALLOW_SYSTEM``: Enable ``system()``.
  - ``ZBC_ANSI_FLAG_READ_ONLY``: Block writes/removes.
  - ``ZBC_ANSI_FLAG_ALLOW_EXIT``: Allow ``exit()`` to terminate host.
- **Custom policy**: ``zbc_ansi_set_policy()`` for OS-specific checks.
- **Callbacks**: ``zbc_ansi_set_callbacks()`` for violations/exits.
- **Init**: ``zbc_ansi_init(&state, "/path/to/sandbox/");``
- **Risk**: Still vulnerable to symlink attacks, DoS via large files.

**Dummy Backend** (`zbc_backend_dummy`)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- **No-op**: All ops succeed without side effects.
- **Use case**: Testing host integration without I/O.
- **No risk**: Purely simulated.

Seccomp Sandbox (Linux)
-----------------------

When built with ``-DZBC_USE_SECCOMP=ON``, the host process is restricted to
**minimal syscalls** needed for semihosting:

- File I/O: ``openat``, ``read``, ``write``, ``close``, ``lseek``, ``ftruncate``, etc.
- Time: ``clock_gettime``, ``gettimeofday``.
- Memory: ``brk``, ``mmap`` (libc needs).
- Exit: ``exit_group``.
- No network, no process creation (except fork for tests), no signals.

**Activation**: Automatic via ``zbc_sandbox_init()``.
**Purpose**: Even if ANSI backend exploited, host can't e.g. ``execve`` malware.
**Check**: ``zbc_sandbox_active()`` returns 1 if loaded.

Fuzzing and Testing
-------------------

- **RIFF Parser Fuzzing**: ``fuzz/fuzz_riff_parser.c`` with ClusterFuzzLite.
- **Corpus Generation**: ``fuzz/gen_malformed_corpus.py`` creates invalid RIFF inputs.
- **Unit Tests**: ``test/`` covers roundtrip, ANSI ops, sandbox.
- **Sanitizers**: ASan/UBSan in CI.

**Best Practices**
- Use secure backend + seccomp for untrusted guests.
- Small sandbox dirs, read-only where possible.
- Limit open FDs (``ZBC_ANSI_MAX_FILES=64``).
- Custom policies for fine-grained control.
- Fuzz your emulator integration.

See :doc:`emulator-integration` for backend setup.
