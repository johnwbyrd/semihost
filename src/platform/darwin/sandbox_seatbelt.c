/*
 * macOS Seatbelt Sandbox
 *
 * Restricts the process to file I/O within a specified directory
 * using macOS App Sandbox (Seatbelt) profiles.
 *
 * Enable with: cmake -DZBC_USE_SEATBELT=ON
 */

#ifdef ZBC_HAVE_SEATBELT

#include "zbc_sandbox.h"
#include <sandbox.h>
#include <stdio.h>

static int sandbox_active_flag = 0;

int zbc_sandbox_init(const char *sandbox_dir)
{
    char *err = NULL;
    char profile[2048];
    int rc;

    if (!sandbox_dir) {
        sandbox_dir = "/tmp";
    }

    /*
     * Seatbelt profile:
     * - Deny everything by default
     * - Allow file read/write only within sandbox_dir
     * - Allow reading /dev/urandom (for libc)
     * - Allow sysctl reads (for system info)
     * - Allow minimal Mach IPC for system services
     */
    rc = snprintf(profile, sizeof(profile),
        "(version 1)\n"
        "(deny default)\n"
        "(allow file-read* file-write*\n"
        "    (subpath \"%s\"))\n"
        "(allow file-read-data\n"
        "    (literal \"/dev/urandom\")\n"
        "    (literal \"/dev/random\"))\n"
        "(allow sysctl-read)\n"
        "(allow mach-lookup\n"
        "    (global-name \"com.apple.system.logger\"))\n",
        sandbox_dir);

    if (rc < 0 || (size_t)rc >= sizeof(profile)) {
        return ZBC_ERR_DEVICE_ERROR;
    }

    rc = sandbox_init(profile, SANDBOX_NAMED_EXTERNAL, &err);
    if (rc != 0) {
        if (err) {
            sandbox_free_error(err);
        }
        return ZBC_ERR_DEVICE_ERROR;
    }

    sandbox_active_flag = 1;
    return ZBC_OK;
}

int zbc_sandbox_active(void)
{
    return sandbox_active_flag;
}

#endif /* ZBC_HAVE_SEATBELT */
