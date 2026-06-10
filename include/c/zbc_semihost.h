/*
 * ZBC Semihosting Library
 *
 * Unified header for client and host libraries.
 * Define ZBC_CLIENT before including for client-side API.
 * Define ZBC_HOST before including for host-side API.
 *
 * This is an umbrella header that includes all modules.
 * You may also include individual headers for finer-grained control.
 */

#ifndef ZBC_SEMIHOST_H
#define ZBC_SEMIHOST_H

/* Protocol definitions - always included */
#include "zbc_protocol.h"

/* Client API (define ZBC_CLIENT to enable) */
#ifdef ZBC_CLIENT
#include "zbc_client.h"
#endif

/* Host API (define ZBC_HOST to enable) */
#ifdef ZBC_HOST
#include "zbc_host.h"
#include "zbc_backend.h"
#include "zbc_backend_ansi.h"
#endif

#endif /* ZBC_SEMIHOST_H */
