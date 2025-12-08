/*
 * Generic Sandbox Stub
 *
 * No-op implementation when platform sandboxing is not available.
 * Used when neither ZBC_HAVE_SECCOMP nor ZBC_HAVE_SEATBELT is defined.
 */

#include "zbc_sandbox.h"

#if !defined(ZBC_HAVE_SECCOMP) && !defined(ZBC_HAVE_SEATBELT)

int zbc_sandbox_init(const char *sandbox_dir)
{
    (void)sandbox_dir;
    return ZBC_OK;  /* No-op: sandboxing not available */
}

int zbc_sandbox_active(void)
{
    return 0;
}

#endif /* !ZBC_HAVE_SECCOMP && !ZBC_HAVE_SEATBELT */
