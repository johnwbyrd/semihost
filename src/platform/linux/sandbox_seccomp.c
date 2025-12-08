/*
 * Linux Seccomp Sandbox
 *
 * Restricts the process to a minimal set of syscalls needed for
 * semihosting file I/O operations. Uses libseccomp for portability
 * across architectures.
 *
 * Enable with: cmake -DZBC_USE_SECCOMP=ON
 */

#ifdef ZBC_HAVE_SECCOMP

#include "zbc_sandbox.h"
#include <seccomp.h>

static int sandbox_active_flag = 0;

int zbc_sandbox_init(const char *sandbox_dir)
{
    scmp_filter_ctx ctx;
    int rc;

    (void)sandbox_dir;  /* seccomp doesn't use directory restriction */

    ctx = seccomp_init(SCMP_ACT_KILL_PROCESS);
    if (!ctx) {
        return ZBC_ERR_DEVICE_ERROR;
    }

    /* File I/O - core semihosting operations */
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(openat), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(close), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(read), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(write), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(pread64), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(pwrite64), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(lseek), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(fstat), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(ftruncate), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(fsync), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(unlinkat), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(renameat), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getcwd), 0);

    /* Time operations - SYS_TIME, SYS_CLOCK, SYS_TICKFREQ */
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(clock_gettime), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(gettimeofday), 0);

    /* Memory management - needed by libc */
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(brk), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(mmap), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(munmap), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(mprotect), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(mremap), 0);

    /* Exit - SYS_EXIT */
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(exit_group), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(exit), 0);

    /* Threading/locking - may be needed by libc */
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(futex), 0);

    /* Misc libc needs */
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getrandom), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(rt_sigreturn), 0);

    /* newfstatat - used by glibc fstat on some architectures */
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(newfstatat), 0);

#if defined(__x86_64__)
    /* Legacy syscalls on x86_64 */
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(open), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(stat), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(unlink), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(rename), 0);
#endif

    rc = seccomp_load(ctx);
    seccomp_release(ctx);

    if (rc < 0) {
        return ZBC_ERR_DEVICE_ERROR;
    }

    sandbox_active_flag = 1;
    return ZBC_OK;
}

int zbc_sandbox_active(void)
{
    return sandbox_active_flag;
}

#endif /* ZBC_HAVE_SECCOMP */
