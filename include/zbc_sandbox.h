/**
 * @file zbc_sandbox.h
 * @brief ZBC Semihosting Sandbox API
 *
 * Optional OS-level sandboxing for defense-in-depth against malicious
 * guest code. Supports seccomp (Linux) and Seatbelt (macOS).
 *
 * Enable via CMake options:
 *   -DZBC_USE_SECCOMP=ON   (Linux, requires libseccomp)
 *   -DZBC_USE_SEATBELT=ON  (macOS)
 */

#ifndef ZBC_SANDBOX_H
#define ZBC_SANDBOX_H

#include "zbc_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize process sandbox.
 *
 * Call this BEFORE processing any guest data. Once called, the process
 * is restricted to a minimal set of syscalls/capabilities.
 *
 * @param sandbox_dir  Directory to restrict file access to (may be NULL
 *                     if not applicable to the platform)
 * @return ZBC_OK on success or if sandboxing unavailable,
 *         ZBC_ERR_DEVICE_ERROR if sandbox setup failed
 */
int zbc_sandbox_init(const char *sandbox_dir);

/**
 * Check if sandboxing is currently active.
 *
 * @return 1 if sandbox is active, 0 otherwise
 */
int zbc_sandbox_active(void);

#ifdef __cplusplus
}
#endif

#endif /* ZBC_SANDBOX_H */
