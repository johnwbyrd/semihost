/*
 * Sandbox Tests
 *
 * Fork-based tests that verify seccomp sandbox actually blocks
 * dangerous syscalls. Uses fork() to isolate each test since sandbox
 * activation is irreversible within a process.
 *
 * Build: cmake -B build -DZBC_USE_SECCOMP=ON && cmake --build build
 * Run:   ./build/test/zbc_sandbox_tests
 */

#ifndef ZBC_HOST
#define ZBC_HOST
#endif

#include "zbc_sandbox.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef ZBC_HAVE_SECCOMP

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/ptrace.h>

/*------------------------------------------------------------------------
 * Test tracking
 *------------------------------------------------------------------------*/

static int tests_run = 0;
static int tests_passed = 0;

/*------------------------------------------------------------------------
 * Test utilities
 *------------------------------------------------------------------------*/

#define MAX_PATH 256

static void make_temp_path(char *buf, size_t size, const char *name)
{
    const char *tmp = getenv("TMPDIR");
    if (!tmp) tmp = getenv("TMP");
    if (!tmp) tmp = getenv("TEMP");
    if (!tmp) tmp = "/tmp";
    snprintf(buf, size, "%s/zbc_sandbox_test_%d_%s", tmp, (int)getpid(), name);
}

/*
 * Run a test function in a forked child process.
 * Returns: 1 if test passed, 0 if failed
 */
typedef int (*test_fn)(void);

static int run_forked_test(const char *name, test_fn fn, int expect_signal)
{
    pid_t pid;
    int status;
    int result = 0;

    printf("  %s... ", name);
    fflush(stdout);
    tests_run++;

    pid = fork();
    if (pid < 0) {
        printf("FAIL (fork failed: %s)\n", strerror(errno));
        return 0;
    }

    if (pid == 0) {
        /* Child process - run test */
        int rc = fn();
        _exit(rc ? 0 : 1);
    }

    /* Parent - wait for child */
    if (waitpid(pid, &status, 0) < 0) {
        printf("FAIL (waitpid failed: %s)\n", strerror(errno));
        return 0;
    }

    if (expect_signal) {
        /* Expecting child to be killed by signal */
        if (WIFSIGNALED(status)) {
            int sig = WTERMSIG(status);
            /* Seccomp with SCMP_ACT_KILL_PROCESS sends SIGSYS */
            if (sig == SIGSYS) {
                printf("PASS (killed by SIGSYS as expected)\n");
                tests_passed++;
                result = 1;
            } else {
                printf("FAIL (killed by signal %d, expected SIGSYS)\n", sig);
            }
        } else if (WIFEXITED(status)) {
            printf("FAIL (exited with %d, expected signal)\n", WEXITSTATUS(status));
        } else {
            printf("FAIL (unknown status)\n");
        }
    } else {
        /* Expecting normal exit */
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            printf("PASS\n");
            tests_passed++;
            result = 1;
        } else if (WIFEXITED(status)) {
            printf("FAIL (exit code %d)\n", WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            printf("FAIL (killed by signal %d)\n", WTERMSIG(status));
        } else {
            printf("FAIL (unknown status)\n");
        }
    }

    return result;
}

/*------------------------------------------------------------------------
 * Test implementations
 *------------------------------------------------------------------------*/

static int test_sandbox_init_succeeds_impl(void)
{
    int rc;

    rc = zbc_sandbox_init("/tmp");
    if (rc != ZBC_OK) {
        return 0;
    }

    if (!zbc_sandbox_active()) {
        return 0;
    }

    return 1;
}

static int test_allowed_file_ops_impl(void)
{
    char path[MAX_PATH];
    int fd;
    char buf[32] = "test data";
    ssize_t n;
    int rc;

    make_temp_path(path, sizeof(path), "allowed.txt");

    rc = zbc_sandbox_init("/tmp");
    if (rc != ZBC_OK) {
        return 0;
    }

    /* Test open/write */
    fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) {
        return 0;
    }

    n = write(fd, buf, strlen(buf));
    if (n != (ssize_t)strlen(buf)) {
        close(fd);
        return 0;
    }

    close(fd);

    /* Test open/read */
    fd = open(path, O_RDONLY);
    if (fd < 0) {
        return 0;
    }

    memset(buf, 0, sizeof(buf));
    n = read(fd, buf, sizeof(buf) - 1);
    if (n <= 0) {
        close(fd);
        return 0;
    }

    close(fd);

    /* Test unlink */
    if (unlink(path) < 0) {
        return 0;
    }

    return 1;
}

static int test_socket_blocked_impl(void)
{
    int rc;
    int fd;

    rc = zbc_sandbox_init(NULL);
    if (rc != ZBC_OK) {
        _exit(2);  /* Sandbox init failed, unexpected */
    }

    /* This should be blocked and kill us with SIGSYS */
    fd = socket(AF_INET, SOCK_STREAM, 0);

    /* If we get here, socket wasn't blocked */
    if (fd >= 0) close(fd);
    return 1;  /* Return "success" - parent will see we weren't killed */
}

static int test_execve_blocked_impl(void)
{
    int rc;
    char *args[] = { "/bin/true", NULL };

    rc = zbc_sandbox_init(NULL);
    if (rc != ZBC_OK) {
        _exit(2);
    }

    /* This should be blocked and kill us with SIGSYS */
    execve("/bin/true", args, NULL);

    /* If we get here, execve wasn't blocked (or /bin/true doesn't exist) */
    return 1;
}

static int test_ptrace_blocked_impl(void)
{
    int rc;

    rc = zbc_sandbox_init(NULL);
    if (rc != ZBC_OK) {
        _exit(2);
    }

    /* This should be blocked and kill us with SIGSYS */
    ptrace(PTRACE_TRACEME, 0, NULL, NULL);

    /* If we get here, ptrace wasn't blocked */
    return 1;
}

/*------------------------------------------------------------------------
 * Main
 *------------------------------------------------------------------------*/

int main(void)
{
    printf("Sandbox Tests (seccomp)\n");
    printf("========================\n\n");

    run_forked_test("sandbox_init_succeeds", test_sandbox_init_succeeds_impl, 0);
    run_forked_test("allowed_file_ops", test_allowed_file_ops_impl, 0);

    /* Seccomp blocking tests - expect SIGSYS */
    run_forked_test("socket_blocked", test_socket_blocked_impl, 1);
    run_forked_test("execve_blocked", test_execve_blocked_impl, 1);
    run_forked_test("ptrace_blocked", test_ptrace_blocked_impl, 1);

    printf("\n========================\n");
    printf("Results: %d/%d passed\n", tests_passed, tests_run);

    return (tests_passed == tests_run) ? 0 : 1;
}

#else /* !ZBC_HAVE_SECCOMP */

/*
 * Stub tests - verify the no-op implementation works correctly
 */

static int tests_run = 0;
static int tests_passed = 0;

int main(void)
{
    int rc;

    printf("Sandbox Tests (stub - no sandbox enabled)\n");
    printf("=========================================\n\n");

    printf("  stub_init_returns_ok... ");
    tests_run++;
    rc = zbc_sandbox_init(NULL);
    if (rc == ZBC_OK) {
        printf("PASS\n");
        tests_passed++;
    } else {
        printf("FAIL (returned %d)\n", rc);
    }

    printf("  stub_active_returns_zero... ");
    tests_run++;
    if (zbc_sandbox_active() == 0) {
        printf("PASS\n");
        tests_passed++;
    } else {
        printf("FAIL\n");
    }

    printf("\n=========================================\n");
    printf("Results: %d/%d passed\n", tests_passed, tests_run);

    return (tests_passed == tests_run) ? 0 : 1;
}

#endif /* ZBC_HAVE_SECCOMP */
