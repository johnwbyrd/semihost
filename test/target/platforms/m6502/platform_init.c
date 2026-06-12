/*
 * m6502 (MAME) Platform Init
 *
 * The MAME platform exposes a ZBC RIFF/doorbell device at the
 * width-neutral semihost address. zbc_client_init() already installs
 * the RIFF transport, so this hook is intentionally empty.
 */

#include "zbc_client.h"

void zbc_platform_init_transport(zbc_client_state_t *state)
{
    (void)state;
}
