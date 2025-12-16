/*
 * Mock Device for Client Library Testing
 *
 * Simulates the memory-mapped semihosting device registers.
 */

#ifndef MOCK_DEVICE_H
#define MOCK_DEVICE_H

#include "zbc_semihost.h"

#ifdef __cplusplus
extern "C" {
#endif

/*------------------------------------------------------------------------
 * Mock device structure
 *------------------------------------------------------------------------*/

typedef struct mock_device {
  /* Simulated register space */
  uint8_t regs[ZBC_REG_SIZE];

  /* Pointer captured from RIFF_PTR register (native pointer) */
  uint8_t *riff_buf;

  /* Host library state for processing requests */
  zbc_host_state_t host_state;

  /* Work buffer for host processing */
  uint8_t work_buf[4096];

  /* Counters */
  int doorbell_count;
  int process_count;

  /* Custom response handler (optional) */
  void (*custom_handler)(struct mock_device *dev);

} mock_device_t;

/*------------------------------------------------------------------------
 * Initialization
 *------------------------------------------------------------------------*/

/* Initialize mock device with dummy backend */
void mock_device_init(mock_device_t *dev);

/* Set "SEMIHOST" signature in register space */
void mock_device_set_signature(mock_device_t *dev);

/* Set DEVICE_PRESENT bit in STATUS register */
void mock_device_set_present(mock_device_t *dev);

/*------------------------------------------------------------------------
 * Register access (for client library)
 *
 * The client library reads/writes to dev->regs. We provide accessors
 * that also trigger side effects (e.g., doorbell triggers processing).
 *------------------------------------------------------------------------*/

/* Called when client writes to DOORBELL register */
void mock_device_doorbell(mock_device_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* MOCK_DEVICE_H */
