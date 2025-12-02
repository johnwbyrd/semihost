/*
 * Mock Memory for Host Library Testing
 *
 * Simulates guest memory for the host library to read/write.
 */

#ifndef MOCK_MEMORY_H
#define MOCK_MEMORY_H

#include "zbc_semihost.h"

#ifdef __cplusplus
extern "C" {
#endif

/*------------------------------------------------------------------------
 * Mock memory structure
 *------------------------------------------------------------------------*/

typedef struct mock_memory {
  uint8_t *data;   /* Memory buffer */
  size_t size;     /* Buffer size */
  int read_count;  /* Number of read operations */
  int write_count; /* Number of write operations */
} mock_memory_t;

/*------------------------------------------------------------------------
 * Mock memory operations (for zbc_host_mem_ops_t)
 *------------------------------------------------------------------------*/

uint8_t mock_mem_read_u8(uint64_t addr, void *ctx);
void mock_mem_write_u8(uint64_t addr, uint8_t val, void *ctx);
void mock_mem_read_block(void *dest, uint64_t addr, size_t size, void *ctx);
void mock_mem_write_block(uint64_t addr, const void *src, size_t size,
                          void *ctx);

/*------------------------------------------------------------------------
 * Helper functions
 *------------------------------------------------------------------------*/

/* Initialize mock memory with a buffer */
void mock_memory_init(mock_memory_t *mem, uint8_t *buf, size_t size);

/* Reset counters */
void mock_memory_reset_counters(mock_memory_t *mem);

/* Fill with test pattern */
void mock_memory_fill_pattern(mock_memory_t *mem);

#ifdef __cplusplus
}
#endif

#endif /* MOCK_MEMORY_H */
